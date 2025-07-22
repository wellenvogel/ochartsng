/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  Shop View

 The MIT License (MIT)

 Copyright (c) 2023 Andreas Vogel (andreas@wellenvogel.net)

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */
import React, { Component } from 'react';
import PropTypes from 'prop-types';
import OverlayDialog, {ConfirmDialog} from './components/OverlayDialog.js';
import {InstallProgress, ProgressDisplay} from "./components/InstallProgress";
import DongleState from "./components/DongleState";
import ReadyState from "./components/ReadyState";
import {resetError, setError} from "./components/ErrorDisplay";
import ShopLogin, {LoginState, SystemState} from "./components/ShopLogin";
import SpinnerDialog from "./components/SpinnerDialog";
import {fetchJson, nestedEquals} from "./util/Util";
import assign from 'object-assign';
import StatusItem from "./components/StatusItem";
import StatusLine from "./components/StatusLine";
import {fetchChartList} from "./util/Fetcher";
import {SHOPURL, UPLOADURL} from "./util/Constants";
import OverlayDisplay from "./components/OverlayDialog.js";



/**
 * compare 2 editions 2023/1-2
 * @param existing
 * @param candidate
 * @return 0-equal, 1-candidate is newer, 2 - candidate is older
 */
const compareEdition=(existing,candidate)=>{
    try {
        let nve = existing.split("/");
        let nvc = candidate.split("/");
        if (nve.length !== 2) return 1; //assume newer if unknown format
        if (nvc.length !== 2) {
            if (nve === nvc) return 0;
            return 2;
        }
        if (nve[0] === nvc[0]) {
            let se = nve[1].split("-");
            let sc = nvc[1].split("-");
            if (se.length !== 2 || sc.length !== 2) {
                return (parseInt(nvc[1]) === parseInt(nve[1])) ? 0 : 1;
            }
            if (se[0] === sc[0]) {
                if (se[1] == sc[1]) return 0;
                return (parseInt(se[1]) < parseInt(sc[1])) ? 1 : 2;
            }
            return (parseInt(se[0]) < parseInt(sc[0])) ? 1 : 2;
        }
        return (parseInt(nve[0]) < parseInt(nvc[0])) ? 1 : 2;
    }catch (e){}
    return 1;
};

const DL_MODES={
    '0': {
        label: 'Shop',
        dl: false
    },
    '1':{
        label: 'Update',
        dl:true,
    },
    '2':{
        label: 'Downgrade',
        dl:true
    },
    '3':{
        label: 'Install',
        dl: true
    }
};

const ShopChart=(props)=>{
    let mode=3; //
    if (props.localEdition){
        mode=compareEdition(props.localEdition,props.edition)
    }
    let modeDescription=DL_MODES[mode]||DL_MODES[3];
    return(
        <StatusItem title={
            <span>
                    <span className="primary" >{props.name}</span>
                    <span className="secondary">{props.system}</span>
                </span>
        }>
            <StatusLine label="Local" value={props.localEdition||"---"}/>
            {modeDescription.dl ?
                <div className="chartDownload">
                        <button
                            className="button chartAction"
                            onClick={() => props.onClick()}
                        >{modeDescription.label}</button>
                    <div className="statusValue"><span className="statusText">{props.edition}</span></div>
                </div>
                :
                <StatusLine label={modeDescription.label} value={props.edition}/>
            }
        </StatusItem>
    );
}

class RegisterDialog extends React.Component{
    constructor(props) {
        super(props);
        this.state={
            name:props.name
        }
        this.dialog=new OverlayDisplay(this);
    }
    validSystemName(name){
        //see ochartsShop.cpp::doGetNewSystemName
        if (! name || name === "") return "name must not be empty"
        if (name.length < 3) return "name must be at least 3 characters";
        if (name.length > 15) return "name must be <= 15 characters";
        if (name.match(/[^0-9A-Za-z]/)) return "invalid characters in name, only characters and numbers";
        let knownSystems=ShopLogin.getKnownSystems();
        if (knownSystems){
            for (let i=0;i<knownSystems.length;i++){
                if (knownSystems[i] === name) return "name already used for another system";
            }
        }
    }
    render(){
        let headLine=this.props.forDongle?"Register Dongle":"Register System";
        let error;
        if (! this.props.forDongle){
            error=this.validSystemName(this.state.name);
        }
        let nameClass=(error !== undefined)?"invalidValue":"";
        return (
            <React.Fragment>
            <this.dialog.render/>
            <div className="dialog ShopRegisterDialog">
                <h3>{headLine}</h3>
                <div className="flexInner">
                    {this.props.forDongle ?
                        <div className="dialogRow">
                            <div className="label">DongleName</div>
                            <div className="value"> {this.state.name}</div>
                        </div>
                        :
                        <div className="dialogRow">
                            <div className="label">SystemName</div>
                            <input type="text"
                                   className={nameClass}
                                   value={this.state.name}
                                   disabled={this.props.predefinedSystemName}
                                   onChange={(ev) => this.setState({name: ev.target.value})}/>
                        </div>
                    }
                    {(error !== undefined) && <div className="dialogRow">
                        <div className="label">Error</div>
                        <div className="value error">{error}</div>
                    </div>}

                </div>
                <div className="dialogRow dialogButtons">
                    <button
                        className="button cancel"
                        onClick={this.props.closeCallback}
                    >Cancel</button>
                    <button
                        className="button ok"
                        disabled={error !== undefined}
                        onClick={()=>{
                            if (!this.state.name){
                                setError("system name must not be empty",10000);
                                return;
                            }
                            this.dialog.setDialog((props)=>{
                                return (<ConfirmDialog
                                        {...props}
                                        title={"Register system "+this.state.name}
                                        text={"ok to register?\nThis cannot be undone."}
                                        okCallback={()=>{
                                            this.props.closeCallback();
                                            this.props.onChange(this.state.name);
                                        }}
                                    />
                                )
                            });
                        }}
                    >OK</button>
                </div>
            </div>
            </React.Fragment>
        )
    }

}

class ShopView extends Component {

    constructor(props){
        super(props);
        this.state={ready:false,
            hasDongle:false,
            dongleName:undefined,
            canHaveDongle: false, //if false we are on android and can try 2 fprs for identify
            loggedIn:ShopLogin.isLoggedIn(),
            shopCharts:ShopLogin.getCurrentChartList(),
            shopSystem: ShopLogin.getIdentifiedSystem(),
            localCharts:[],
            predefinedSystemName:undefined,
            knownSystems: ShopLogin.getKnownSystems()
        };
        this.dialog=new OverlayDialog(this);
        this.timer=undefined;
        this.showDialog=this.showDialog.bind(this);
        this.listRef=React.createRef();
        this.chartListFetcher=fetchChartList(this,1000,(err)=>{
            setError("unable to fetch charts: "+err,2000)
        },"localCharts");
        this.loginStateChanged=this.loginStateChanged.bind(this);
        this.loginDialogAction=this.loginDialogAction.bind(this);
    }

    showSpinner(title){
        if (title) {
            resetError();
            this.showDialog((props) => {
                return <SpinnerDialog {...props} title={title}/>
            });
        }
        else{
            this.dialog.hideDialog();
        }
    }
    showDialog(dialog){
        this.dialog.setDialog(dialog);
    }
    getSnapshotBeforeUpdate(pp,np){
        const list=this.listRef.current;
        if (list === null) return null;
        return list.scrollTop;
    }
    loginStateChanged(keys){
        let newState=assign({},this.state,{
            loggedIn: ShopLogin.isLoggedIn(),
            shopCharts: ShopLogin.getCurrentChartList(),
            shopSystem: ShopLogin.getIdentifiedSystem(),
            knownSystems: ShopLogin.getKnownSystems()
        });
        if (! nestedEquals(this.state,newState)) {
            this.setState(newState);
        }
    }
    componentDidMount() {
        ShopLogin.registerShopCallback(this.loginStateChanged);
        fetchJson(SHOPURL+"predefinedname")
            .then((json)=>{
                this.setState({predefinedSystemName:json.data.name})
            })
            .catch((err)=>{console.log("unable to get predefined name")})
    }
    componentWillUnmount() {
        ShopLogin.deregisterShopCallback(this.loginStateChanged);
    }

    componentDidUpdate(pp,ps,snapshot){
        if (snapshot != null && (pp.currentView == this.props.currentView)){
            const list = this.listRef.current;
            if (list === null) return;
            list.scrollTop = snapshot;
        }
    }
    handleReadyState(isReady) {
        if (isReady) {
            return "READY";
        } else {
            return "BUSY";
        }
    }
    handleSubError(error) {
        setError(error);
    }
    identifyAndChartList(asPromise){
        const runChartList=()=>{
            this.showSpinner("chartlist");
            return ShopLogin.chartList()
                .then(()=>{
                    this.showSpinner()
                })
        }
        this.showSpinner("identify");
        return ShopLogin.tryIdentify()
            .then((system)=>{
                //if we are on Android (canHaveDongle == false)
                //we make a second try with the alternative fpr
                if (system !== undefined || this.state.canHaveDongle) return runChartList();
                this.showSpinner("identify alt");
                return ShopLogin.tryIdentify(true)
                    .then((system)=>{
                        return runChartList();
                    })
            })
            .catch((err)=>{
                this.showSpinner();
                if (asPromise) return Promise.reject(err);
                setError(err)
            });
    }

    loginDialogAction(data) {
        this.showSpinner("login");
        ShopLogin.tryShopLogin(data)
            .then(() => {
                return this.identifyAndChartList(true);
            })
            .catch((error) => {
                this.showSpinner();
                setError(error)
            });
    }

    /**
     * find a local chart that either has the same version (active or inactive)
     * or an active local chart with a different version
     * @param id
     * @param version
     * @returns {*}
     */
    findLocalChart(id,version){
        if (! this.state.localCharts || this.state.localCharts.length < 1) return;
        let candidate=undefined;
        let inactiveCandidate=undefined;
        for (let i=0;i<this.state.localCharts.length;i++){
            let chart=this.state.localCharts[i];
            if (! chart.info) continue;
            if (id === chart.info.id){
                if (chart.info.version === version) {
                    return chart.info;
                }
                if (chart.active && ! candidate){
                    candidate=chart.info;
                }
                if (! chart.active && ! inactiveCandidate){
                    inactiveCandidate=chart.info;
                }
            }
        }
        if (candidate) return candidate;
        return inactiveCandidate;
    }
    getFilteredShopCharts(forDongle){
        let compareName;
        if (!forDongle) {
            if (this.state.shopSystem === undefined || this.state.shopSystem.name === undefined) return [];
            compareName=this.state.shopSystem.name;
        }
        else{
            if (! this.isDongleIdentified()){
                return [];
            }
            compareName=this.state.dongleName;
        }
        let rt=[];
        let shopCharts=this.state.shopCharts||[];
        shopCharts.forEach((chart)=>{
            if (!chart.assigments) return;
            for (let i=0;i<chart.assigments.length;i++){
                if (chart.assigments[i].name === compareName){
                    let ce={
                        id:chart.id,
                        name:chart.name,
                        edition: chart.edition,
                        slotUuid: chart.assigments[i].slotUuid,
                        systemName: chart.assigments[i].name
                    };
                    let localChart=this.findLocalChart(chart.id,ce.edition);
                    if (localChart){
                        ce.localEdition=localChart.version;
                    }
                    rt.push(ce);
                }
            }
        })
        return rt;
    }
    startShopDownload(chart){
        console.log("shopDownload",chart);
        this.showSpinner("get chart url");
        ShopLogin.getDownloadUrls(chart)
            .then((res)=>{
                this.showSpinner();
                fetchJson(UPLOADURL+"downloadShop?chartUrl="
                    +encodeURIComponent(res.chartUrl)
                    +"&keyUrl="+encodeURIComponent(res.keyUrl)
                )
                    .then(()=>{})
                    .catch((err)=>setError(err));
            })
            .catch((err)=>{
                this.showSpinner();
                setError(err);
            })
    }
    isDongleIdentified(){
        if (this.state.hasDongle && this.state.dongleName){
            if (this.state.knownSystems){
                for (let i=0;i<this.state.knownSystems.length;i++){
                    if (this.state.dongleName === this.state.knownSystems[i]){
                        return true;
                    }
                }
            }
        }
        return false;
    }
    isSystemIdentified(){
        if (! this.state.shopSystem) return false;
        return this.state.shopSystem.name !== undefined;
    }
    register(forDongle, name){
        this.showSpinner("register "+name);
        ShopLogin.register(forDongle,name)
            .then(()=>{
                if (forDongle){
                    this.showSpinner()
                    return;
                }
                else{
                    return this.identifyAndChartList(true);
                }
            })
            .catch((err)=>{
                this.showSpinner();
                setError(err)
            });
    }
    render() {
        let self = this;
        let systemCharts = this.getFilteredShopCharts(false);
        let dongleCharts = this.getFilteredShopCharts(true);
        let shopDongleState="INACTIVE";
        let shopDongleText="unknown";
        if (this.state.hasDongle && this.state.dongleName){
            shopDongleState="YELLOW";
            shopDongleText="not known";
            if (this.isDongleIdentified()){
                shopDongleText="identified "+this.state.dongleName;
                shopDongleState="GREEN";
            }
        }
        return (
            <div className="view shopView">
                <this.dialog.render/>
                <div className="chartsBody" ref={self.listRef}>
                    <div className="statusItem">
                        <DongleState
                            className="ChartViewStatus"
                            onChange={(dongle) => {
                                if (!dongle) dongle = {
                                    present: false,
                                    canhave: false
                                }
                                if (this.state.hasDongle !== dongle.present
                                    || this.state.dongleName !== dongle.name
                                )
                                    this.setState({
                                        dongleName: dongle.name,
                                        hasDongle: dongle.present,
                                        canHaveDongle: dongle.canhave
                                    })
                            }}
                            onError={(err) => {
                                this.handleSubError(err)
                            }}/>
                        <ReadyState className="ChartViewStatus"
                                    onChange={(v) => this.handleReadyState(v)}
                                    onError={(e) => this.handleSubError(e)}
                        />
                        <LoginState/>
                        {this.state.loggedIn &&<SystemState/>}
                        {(this.state.loggedIn && this.state.hasDongle) &&
                            <StatusLine label={"Shop Dongle"} value={shopDongleState} text={shopDongleText} icon={true}/>
                        }
                        <InstallProgress
                            className="ChartViewStatus"
                            interval={1000}
                        />

                        <div className="actions globalActions">
                            <button className="button actions"
                                    onClick={() => {
                                        this.showDialog(ShopLogin.createLoginDialog(this.loginDialogAction))
                                    }}
                                    disabled={this.state.loggedIn}
                            >Login
                            </button>
                            <button className="button actions"
                                    onClick={() => {
                                        this.identifyAndChartList(false);
                                    }}
                                    disabled={!this.state.loggedIn}
                            >Refresh
                            </button>
                            <button className="button actions"
                                    onClick={() => {
                                        console.log("shop website");
                                        let url="https://o-charts.org/shop/en/autenticacion?";
                                        if (this.state.loggedIn){
                                            url+="email="+encodeURIComponent(ShopLogin.getUser());
                                            url+="&passwd="+encodeURIComponent(ShopLogin.getPassword());
                                            url+="&back=my-account&";
                                            url+="SubmitLogin=1";
                                            let system=ShopLogin.getIdentifiedSystem();
                                            let sname=undefined;
                                            if (system !== undefined && system.name !== undefined){
                                                sname=system.name;
                                            }
                                            this.showDialog((props)=>{
                                                return <ConfirmDialog
                                                    {...props}
                                                    title="O-Charts Shop"
                                                    html={<div>
                                                        You will be redirected to the o-charts shop.
                                                        <br/>
                                                        {(sname === undefined) &&<span>You need to register your system before you can assign charts to it.</span>}
                                                        {(sname !== undefined) &&<span>Your system is known as <b>{sname}</b> in the shop.</span>}
                                                        <br/>
                                                        Please check if you have been checked in with the correct account on the shop page.
                                                    </div>}
                                                    okCallback={()=>window.open(url,"shop")}
                                                />
                                            })
                                            return;
                                        }
                                        window.open(url,"shop");
                                    }}

                            >ShopPage
                            </button>
                            <button className="button actions"
                                    onClick={() => {
                                        ShopLogin.logout()
                                    }}
                                    disabled={!this.state.loggedIn}
                            >Logout
                            </button>
                        </div>
                    </div>
                    <div className="actions globalActions">
                        {(this.state.loggedIn && !this.isSystemIdentified()) &&
                        <button className="button actions"
                                onClick={() => {
                                    this.showDialog((props)=>{
                                        return (
                                            <RegisterDialog {...props}
                                                forDongle={false}
                                                name={this.state.predefinedSystemName||""}
                                                predefinedSystemName={this.state.predefinedSystemName !== undefined && this.state.predefinedSystemName !== ""}
                                                onChange={(name)=>this.register(false,name)}/>
                                        )
                                    })
                                }}
                        >Register system
                        </button>
                        }
                        {(this.state.loggedIn && ! this.isDongleIdentified() && this.state.hasDongle) &&
                        <button className="button actions"
                                onClick={() => {
                                    this.showDialog((props)=>{
                                        return (
                                            <RegisterDialog {...props}
                                                            forDongle={true}
                                                            name={this.state.dongleName}
                                                            onChange={()=>this.register(true, this.state.dongleName)}/>
                                        )
                                    })
                                }}
                        >Register dongle
                        </button>
                        }
                    </div>
                    <div className="systemCharts">
                        {systemCharts.map((chart) => {
                            return (
                                <ShopChart {...chart}
                                           system={(this.state.shopSystem||{}).name}
                                           onClick={()=>{
                                               this.startShopDownload(chart)
                                           }}
                                />
                            )
                        })}
                    </div>
                    <div className="dongleCharts">
                        {dongleCharts.map((chart) => {
                            return (
                                <ShopChart {...chart}
                                           system={this.state.dongleName}
                                           onClick={()=>{
                                               this.startShopDownload(chart)
                                           }}
                                />
                            )
                        })}
                    </div>
                </div>
            </div>
        )
    }

}
ShopView.propTypes={
    currentView: PropTypes.string
};



export default ShopView;

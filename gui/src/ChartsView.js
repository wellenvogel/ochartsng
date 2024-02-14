/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  Settings display

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
import React, {Component, useRef} from 'react';
import PropTypes from 'prop-types';
import {setError} from './components/ErrorDisplay.js';
import OverlayDialog from './components/OverlayDialog.js';
import ChartSetStatus from './components/ChartSetStatus.js';
import {CHARTSURL, SETTINGSURL, SHOPURL, STATUSURL, TOPIC_INSTALL, TOPIC_UPLOAD, UPLOADURL} from './util/Constants';
import {InstallProgress, PROGRESS_STORE_KEY, ProgressDisplay} from "./components/InstallProgress";
import {fetchJson, hexToBase64, nestedEquals, uploadFile} from "./util/Util";
import {resetError} from "./components/ErrorDisplay";
import SpinnerDialog from "./components/SpinnerDialog";
import {fetchChartList, fetchReadyState} from "./util/Fetcher";
import ReadyState from "./components/ReadyState";
import Store from "./util/store";
import DongleState from "./components/DongleState";


const DisabledByDialog=(props)=>{
    return (
        <div className="dialog disabledBy">
            <h3>{props.item.title}</h3>
            <div>Version: {props.item.version}</div>
            <p>This chart set has been disabled as there was another chart set with the same name that
                has a newer version or was explicitely enabled by the user</p>
            <p>Consider disabling chart set<br/> {props.other.title} (Version: {props.other.version})<br/> before</p>
            <div className="dialogRow dialogButtons">
                <button className="button cancel" onClick={props.closeCallback}>Cancel</button>
                <button
                    className="button"
                    onClick={()=>{props.enableCallback();props.closeCallback();}}
                    >
                    Enable Anyway</button>
            </div>
        </div>
    );
};


const FPRDialog=(props)=>{
    return (
        <div className="dialog FPRDialog">
            <h3>Fingerprint Created</h3>
            <div className="dialogRow">
                <span className="label">FileName</span>
                <span className="value">{props.fileName}</span>
            </div>
            <div className="dialogRow dialogButtons">
                <button className="button cancel" onClick={props.closeCallback}>Cancel</button>
                <a
                    className="button download"
                    download={props.fileName}
                    href={"data:application/octet-stream;base64,"+hexToBase64(props.data)}
                    onClick={props.closeCallback}
                    >
                    Download</a>
            </div>
        </div>
    );
};

const readyState=(ready)=>{
    return ready?"READY":"BUSY";
};


const RESTART_WINDOW=60000; //ms to ignore errors during restart
const MIN_RESTART_WINDOW=2000; //ms before we reset the restart flag
class ChartsView extends Component {

    constructor(props) {
        super(props);
        this.state = {
            ready: false,
            uploadIndicator: undefined,
            hasDongle: false,
            dongleName: undefined,
            currentRequest: undefined
        };
        this.formRef = React.createRef();
        this.fileInputref = React.createRef();
        this.downloadRef=React.createRef();
        this.store = new Store("chartView");
        this.dialog = new OverlayDialog(this);
        this.restartTime = undefined;
        this.showDialog = this.showDialog.bind(this);
        this.triggerRestart = this.triggerRestart.bind(this);
        this.isReady = this.isReady.bind(this);
        this.startUpload = this.startUpload.bind(this);
        this.listRef = React.createRef();
        this.chartListFetcher = fetchChartList(this, 2000, (err) => {
            if (this.omitErrors(this.restartTime)) return;
            setError("Error fetching current status: " + error)
        })

    }
    handleReadyState(isReady) {
        if (this.state.ready !== isReady){
            this.setState({ready:isReady});
        }
        if (isReady) {
            let now = (new Date()).getTime();
            if (this.restartTime !== undefined && now >= (this.restartTime + MIN_RESTART_WINDOW)) {
                //restart restart timer on successfull receive
                self.restartTime = undefined;
                self.dialog.hideDialog();
                resetError();
                self.chartListFetcher();
            }
            return "READY";
        } else {
            return "BUSY";
        }
    }

    handleSubError(error) {
        if (!this.omitErrors(this.restartTime)) {
            setError(error);
            return "ERROR";
        } else {
            return "RESTARTING";
        }
    }
    omitErrors(curRestart){
        if (curRestart === undefined && this.restartTime === undefined) return false;
        let now=(new Date()).getTime();
        if (now > (this.restartTime + RESTART_WINDOW)){
            this.restartTime=undefined;
        }
        if (curRestart === undefined) return true;
        if (now > (curRestart + RESTART_WINDOW)){
            return false;
        }
        return true;
    }

    showSpinner(title){
        this.showDialog((props)=>{
            return <SpinnerDialog {...props} title={title}/>
        });
    }
    showDialog(dialog){
        this.dialog.setDialog(dialog);
    }
    getSnapshotBeforeUpdate(pp,np){
        const list=this.listRef.current;
        if (list === null) return null;
        return list.scrollTop;
    }
    componentDidUpdate(pp,ps,snapshot){
        if (snapshot != null && (pp.currentView == this.props.currentView)){
            const list = this.listRef.current;
            if (list === null) return;
            list.scrollTop = snapshot;
        }
    }

    getFPR(forDongle){
        resetError();
        this.showSpinner();
        let self=this;
        let url=SHOPURL+"fpr";
        if (forDongle) url+="?dongle=true";
        fetchJson(url)
            .then((jsonData) => {
                if (!jsonData.data || !jsonData.data.name || !jsonData.data.value) throw new Error("no fpr returned");
                let dialog = (props) => {
                    return <FPRDialog
                        {...props}
                        data={jsonData.data.value}
                        fileName={jsonData.data.name}
                    />
                };
                self.dialog.setDialog(dialog);
            })
            .catch((error) => {
                self.dialog.hideDialog();
                setError(error);
            })

    }
    uploadSet(){
        resetError();
        this.formRef.current.reset();
        this.fileInputref.current.click();
    }
    startUpload(ev){
        let self=this;
        let fileObject=ev.target;
        if (! fileObject.files || fileObject.files.length < 1) {
            return;
        }
        let file=fileObject.files[0];
        if (! file.name.match(/\.zip$/i)){
            setError("only files .zip are allowed");
            return;
        }
        let handler={};
        handler.errorhandler=(error)=>{
                self.finishUpload(true);
                setError(error);
            };
        handler.starthandler=(xhdr)=>{
                self.setState({
                    uploadIndicator: {xhdr:xhdr,handler:handler}
                });
            };
        handler.progresshandler=(ev)=> {
            let ps = {
                percent: 0
            };
            if (ev.lengthComputable) {
                ps.loaded = ev.loaded;
                ps.total = ev.total;
                if (ev.total > 0) {
                    ps.progress = Math.floor(ev.loaded * 100 / ev.total);
                }
            }
            this.store.storeData(PROGRESS_STORE_KEY,ps);
        };
        handler.okhandler=(jsonData)=>{
                self.finishUpload();
        };
        uploadFile(UPLOADURL+"uploadzip",file,handler);
        return;
    }
    finishUpload(opt_cancel){
        if (opt_cancel){
            if (this.state.uploadIndicator && this.state.uploadIndicator.xhdr){
                this.state.uploadIndicator.xhdr.abort();
            }
        }
        this.setState({
            uploadIndicator:undefined
        });
        this.dialog.hideDialog();
        this.chartListFetcher();
    }
    triggerRestart(){
        resetError();
        let self=this;
        let now=(new Date()).getTime();
        self.restartTime=now;
        fetchJson(SETTINGSURL+"restart")
            .then((jsonData)=>{
                self.dialog.alert("restart triggered");
            })
            .catch((error)=>{
                self.restartTime=undefined;
                setError(error);
            })
    }
    isReady(){
        return this.state.ready;
    }
    findSetByKey(key){
        if (!this.state.charts) return;
        let chartSets=this.state.charts;
        for (let k in chartSets){
            if (!chartSets[k].info) continue;
            if (chartSets[k].info.name === key) return chartSets[k];
        }
    }
    render() {
        let self=this;
        let chartSets=this.state.charts||[];
        let disabled=this.isReady()?"":" disabled ";
        let EnableButton=(props)=>{
            if (! props.info || ! props.info.name) return null;
            let changeUrl=SETTINGSURL+"enable+?chartSet="+encodeURI(props.info.name)+"&enable="+(props.active?"disable":"enable");
            let buttonText=props.active?"Disable":"Enable";
            let addClass=self.isReady()?"":" disabled ";
            let onClickDo=()=>{
                this.showSpinner();
                fetchJson(changeUrl)
                    .then((jsonData)=>{
                        this.dialog.hideDialog();
                        if (jsonData.changed){
                            self.chartListFetcher();
                        }
                    })
                    .catch((error)=>{
                        this.dialog.hideDialog();
                        setError(error)
                    })
            };
            let onClick=()=>{
                if (props.disabledBy && ! props.active){
                    let other=self.findSetByKey(props.disabledBy);
                    if (other && other.active) {
                        let dialogFunction = (dbprops)=> {
                            return <DisabledByDialog
                                {...dbprops}
                                item={props.info}
                                enableCallback={onClickDo}
                                other={other.info||{}}/>
                        };
                        self.dialog.setDialog(dialogFunction);
                        return;
                    }
                }
                onClickDo();
            };
            return  <button className={"button"+addClass} onClick={onClick}>{buttonText}</button>;

        };
        let DownloadButton=(props)=>{
            if (! props.info) return null;
            if (! this.downloadRef.current) return null;
            return(
                <button className={"button download"} onClick={()=>{
                    if (this.downloadRef){
                        this.downloadRef.current.src=CHARTSURL+props.info.name+"/download";
                    }
                }}>Download</button>
            )
        }
        let AutoButton=(props)=>{
            if (! props.info) return null;
            let addClass=(self.isReady() && props.originalState != 0)?"":" disabled ";
            return(
                <button className={"button auto "+addClass} onClick={()=>{
                    let changeUrl=SETTINGSURL+"enable+?chartSet="+encodeURI(props.info.name)+"&enable=auto";
                    this.showSpinner();
                    fetchJson(changeUrl)
                        .then((jsonData)=>{
                            this.dialog.hideDialog();
                            if (jsonData.changed){
                                self.chartListFetcher();
                            }
                        })
                        .catch((error)=>{
                            this.dialog.hideDialog();
                            setError(error)
                        })
                }}>Auto</button>
            )
        }
        let DeleteButton=(props)=>{
            if (! props.canDelete || ! props.info) return null;
            let addClass=self.isReady()?"":" disabled ";
            let onClickDel=()=>{
                self.dialog.confirm("Really delete version "+props.info.version+"?",
                    props.info.title)
                    .then(()=>{
                        this.showSpinner();
                        fetchJson(UPLOADURL+"deleteSet?name="+encodeURIComponent(props.info.name))
                            .then(()=>{
                                self.chartListFetcher();
                                self.dialog.hideDialog();
                            })
                            .catch((error)=>{
                                self.dialog.hideDialog();
                                setError(error);
                            });
                    })
                    .catch(()=>{});
            };
            return <button className={"button"+addClass} onClick={onClickDel}>Delete</button>
        };
        let Content=(props)=>{
            if ( ! self.state.charts){
                return <div className="loading">Loading...</div>;
            }
            return (
                <React.Fragment>
                <div className="chartsBody" ref={self.listRef}>
                    <div className="statusItem">
                    <DongleState
                        className="ChartViewStatus"
                        onChange={(dongle)=>{
                            if (! dongle) dongle={
                                present:false
                            }
                            if (this.state.hasDongle !== dongle.present
                                || this.state.dongleName !== dongle.name
                            )
                                this.setState({
                                    dongleName: dongle.name,
                                    hasDongle: dongle.present
                                })
                        }}
                        onError={(err)=>{
                            this.handleSubError(err)
                        }}/>
                    <ReadyState className="ChartViewStatus"
                        onChange={(v)=>this.handleReadyState(v)}
                        onError={(e)=>this.handleSubError(e)}
                    />
                    <InstallProgress
                        className="ChartViewStatus"
                        interval={1000}
                    />
                    </div>
                    <div className="actions globalActions">
                        <button
                            className={"button fpr" + disabled}
                            onClick={()=>self.getFPR(false)}>Get Fingerprint</button>
                        {this.state.hasDongle && <button className={"button fprDongle" + disabled}
                                onClick={()=>self.getFPR(true)}>
                            Get Fingerprint(Dongle)</button>}
                        {false && <button
                            className={"button restart" + disabled}
                            onClick={self.triggerRestart}
                            >ReloadCharts (Restart)</button>}
                        <button className={"button upload" + disabled}
                                onClick={()=>self.uploadSet()}>
                            Upload Zip</button>
                    </div>
                    {
                        chartSets.map((chartSet)=>{
                            let status="INACTIVE";
                            if (chartSet.active && chartSet.status == 'INIT') {
                                status = "PENDING";
                            }
                            else status=chartSet.status;
                            return <ChartSetStatus
                                        key={chartSet.info?chartSet.info.name:""}
                                        {...chartSet}
                                        status={status}
                                >
                                <div className="chartSetButtons">
                                    <EnableButton {...chartSet}/>
                                    <AutoButton {...chartSet}/>
                                    <DeleteButton {...chartSet}/>
                                    <DownloadButton {...chartSet}/>
                                </div>
                                </ChartSetStatus>

                        })
                    }
                </div>
                    {self.state.uploadIndicator && <ProgressDisplay
                            store={this.store}
                            title="Uploading"
                            closeCallback={()=>self.finishUpload(true)}
                            upload100={()=>{
                                self.setState({uploadIndicator:undefined});
                            }}
                        />
                    }
                </React.Fragment>
            )
        };
        return (
            <div className="view chartsView">
                <form className={"UploadForm hidden"} ref={this.formRef}>
                    <input type={"file"} ref={this.fileInputref} onChange={this.startUpload}/>
                </form>
                <this.dialog.render/>
                <Content/>
                <iframe className="downloadFrame"
                        ref={this.downloadRef}
                        onLoad={(e) => {
                            let etxt = undefined;
                            try {
                                etxt = e.target.contentDocument.body.textContent;
                            } catch (e) {
                            }
                            setError((etxt !== undefined) ? etxt.replace("\n", " ") : "unable to download");
                        }}
                ></iframe>
            </div>
        );
    }

}
ChartsView.propTypes={
    currentView: PropTypes.string
};



export default ChartsView;

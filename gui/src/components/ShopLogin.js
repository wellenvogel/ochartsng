/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  Shop Login

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

import React from "react";
import {fetchJson, lifecycleTimer, nestedEquals, storeHelperState} from "../util/Util";
import {SHOPURL} from "../util/Constants";
import StatusLine from "./StatusLine";
import Store from "../util/store";
import {setError} from "./ErrorDisplay";

let loginStore=new Store("shopLogin");
const ST_LOGIN="login"; //user,password,key
const ST_SYSTEM="systemName"; //identified system
const ST_CHARTS="chartlist";
const ST_KNOWN_SYSTEMS="knownSystems";
const ALL_KEYS=[ST_LOGIN,ST_SYSTEM,ST_CHARTS,ST_KNOWN_SYSTEMS];


export class LoginState extends React.Component{
    constructor(props) {
        super(props);
        this.state={
            login: {}
        };
        this.storeHelper=storeHelperState(this,loginStore,ST_LOGIN)
    }
    render(){
        let statusText="not logged in";
        let status="INACTIVE";
        let current=this.state.login||{};
        if (current.key && current.user){
            statusText="user: "+current.user;
            status="READY";
        }
        return (
            <React.Fragment>
                {(status !== undefined) ?
                    <StatusLine label="Shop Login" className={this.props.className}
                                value={status}
                                text={statusText}
                                icon={true}/>
                    :
                    null
                }
            </React.Fragment>
        );
    }
}

export class SystemState extends React.Component{
    constructor(props) {
        super(props);
        this.state= {
            systemName: undefined
        };
        this.storeHelper=storeHelperState(this,loginStore,ST_SYSTEM)
    }
    render(){
        let statusText="not asked";
        let status="INACTIVE";
        let current=this.state.systemName;
        if (current){
            if (current.name) {
                statusText = current.name;
                status = "READY";
            }
            else{
                statusText="not registered";
                status="YELLOW";
            }
        }
        return (
            <React.Fragment>
                {(status !== undefined) ?
                    <StatusLine label="Shop System" className={this.props.className}
                                value={status}
                                text={statusText}
                                icon={true}/>
                    :
                    null
                }
            </React.Fragment>
        );
    }
}

class LoginDialog extends React.Component{
    constructor(props) {
        super(props);
        this.state={
            user:props.user,
            password:props.password
        };
    }
    render(){
        return (
            <div className="dialog ShopLoginDialog">
                <h3>Ocharts Shop Login</h3>
                <div className="flexInner">
                    <div className="dialogRow">
                        <div className="label">Username</div>
                        <input type="email" value={this.state.user}
                               onChange={(ev)=>this.setState({user:ev.target.value})}/>
                    </div>
                    <div className="dialogRow">
                        <div className="label">Password</div>
                        <input type="password" value={this.state.password}
                               onChange={(ev)=>this.setState({password:ev.target.value})}/>
                    </div>
                </div>
                 <div className="dialogRow dialogButtons">
                    <button
                        className="button cancel"
                        onClick={this.props.closeCallback}
                    >Cancel</button>
                    <button
                         className="button ok"
                         onClick={()=>{
                             if (!this.state.user){
                                 setError("username must not be empty",10000);
                                 return;
                             }
                             if (!this.state.password){
                                 setError("password must not be empty",10000);
                                 return;
                             }
                             this.props.closeCallback()
                             this.props.onChange(this.state);
                         }}
                    >OK</button>
                 </div>
            </div>
        )
    }
}
const shopErrors={
    '3c': 'unknown request',
    '3d': 'empty username',
    '3e': 'invalid username',
    '3f': 'empty password',
    '3g': 'wrong password',
    '3k': 'invalid characters',
    '8l': 'no system name for this device',
    '8h': 'something has changed in the device assigned to this system name',
    '8j': 'there is already a system name for this device',
    //our own errors
    'AV1': 'internal error',
    'AV2': 'communication error',
    'AV3': 'invalid shop url',
    'AV4': 'unable to connect to the shop (no internet?)',
    //plain numbers - seems to be n:...
    '2' : 'server in maintenance mode',
    '4' : 'user does not exist',
    '5' : 'invalid version of the plugin',
    '6' : 'invalid user/email name or password',
    '10': 'system name is disabled',
    '20': 'chart already assigned to this system',

};
class ShopError{
    constructor(code,text){
        this.code=code;
        if (text){
            this.text=text;
        }
        else{
            let lookup=code.replace(/:.*/,'');
            this.text=shopErrors[lookup];
        }
    }
    toString(){
        let rt='';
        if (this.text) rt+=this.text+"\n";
        if (this.code !== undefined) rt+="Code: "+this.code;
        return rt;
    }
}

const getXmlVal=(xml,name)=>{
    let parts=name.split(".");
    for (let p in parts){
        let np=parts[p];
        xml=xml.getElementsByTagName(np)[0];
        if (!xml) return;
    }
    if (xml) return xml.textContent;
}

class LoginHandler{
    constructor() {
        this.store=loginStore;
    }
    login(user,password,key){
        if (user !== undefined && password !== undefined && key !== undefined){
            this.store.storeData(ST_LOGIN,{user:user,password:password,key:key});
        }
    }
    logout(){
        ALL_KEYS.forEach((key)=>
        this.store.storeData(key,undefined)
    );
    }
    isLoggedIn(){
        let current=this.store.getData(ST_LOGIN);
        if (! current) return false;
        return (current.user !== undefined && current.key !== undefined && current.password !== undefined);
    }
    getUser(){
        if (! this.isLoggedIn()) return;
        return this.store.getData(ST_LOGIN,{}).user;
    }
    getPassword(){
        if (! this.isLoggedIn()) return;
        return this.store.getData(ST_LOGIN,{}).password;
    }
    getKey(){
        if (! this.isLoggedIn()) return;
        return this.store.getData(ST_LOGIN,{}).key;
    }
    registerShopCallback(callback,opt_keys){
        this.store.register(callback,opt_keys||ALL_KEYS);
    }
    deregisterShopCallback(callback){
        this.store.deregister(callback);
    }
    createLoginDialog(callback){
        return (props)=>{
            return (
                <LoginDialog
                    {...props}
                    onChange={(nv)=>callback(nv)}
                    />
            );
        }
    }
    executeShoprequest(task,parameters){
        let url=SHOPURL+"api?version=r.1.0.0&taskId="+encodeURIComponent(task);
        for (let k in parameters){
            url+="&"+encodeURIComponent(k)+"="+encodeURIComponent(parameters[k]);
        }
        return fetch(url)
            .then(resp => resp.text())
            .then(str => new window.DOMParser().parseFromString(str, "text/xml"));
    }
    getShopResultError(xml){
        let response=xml.getElementsByTagName("response")[0];
        if (! response){
            return new ShopError(0,"empty response");
        }
        let code=response.getElementsByTagName("result")[0];
        if (code === undefined || code === null || code === ""){
            return new ShopError(0, "unknown response");
        }
        code=code.textContent;
        if (code == 1) return;
        return new ShopError(code);
    }

    tryShopLogin(dialogResult){
        let user=dialogResult.user;
        let password=dialogResult.password;
        if (user === undefined || user === "") return Promise.reject("username must not be empty");
        if (password === undefined || password === "") return Promise.reject("password must not be empty");
        return this.executeShoprequest("login",{
            username:user,
            password:password
        })
            .then((xml)=>{
                let err=this.getShopResultError(xml);
                if (err) throw new Error(err.toString());
                let key=xml.getElementsByTagName("response")[0].getElementsByTagName("key")[0].textContent;
                if (! key){
                    throw Error("no key from shop");
                }
                this.login(user,password,key);
                return true;
            })
    }
    tryIdentify(opt_alternative){
        if (! this.isLoggedIn()) return Promise.reject("not logged in");
        let url=SHOPURL+"fpr";
        if (opt_alternative) url+="?alternative=true";
        return fetchJson(url)
            .then((json)=>{
                if (! json.data || ! json.data.name|| ! json.data.value){
                    throw new Error("unable to get fpr");
                }
                return this.executeShoprequest("identifySystem", {
                        key: this.getKey(),
                        username: this.getUser(),
                        xfprName: json.data.name,
                        xfpr: json.data.value
                    })
                    .then((xml)=>{
                        let err = this.getShopResultError(xml);
                        if (err && err.code !== '8l'){
                            throw new Error(err.toString());
                        }
                        if (err){
                            this.store.storeData(ST_SYSTEM,{
                                name:undefined,
                                queried: true
                            });
                            return;
                        }
                        else{
                            let systemName = xml.getElementsByTagName("response")[0].getElementsByTagName("systemName")[0].textContent;
                            this.store.storeData(ST_SYSTEM,{
                                name:systemName,
                                queried: true
                            });
                            return systemName;
                        }
                    })
            })
    }
    chartList(){
        if (! this.isLoggedIn()) return Promise.reject("not logged in");
        return this.executeShoprequest("getlist",{
            key: this.getKey(),
            username: this.getUser(),
        })
            .then((xml)=>{
                let err=this.getShopResultError(xml);
                if (err) throw new Error(err.toString());
                let systemNames=[];
                let systems=xml.getElementsByTagName("response")[0].getElementsByTagName("systemName");
                if (systems.length > 0) {
                    for (let i = 0; i < systems.length; i++) {
                        systemNames.push(systems[i].textContent);
                    }
                }
                let v={};
                v[ST_KNOWN_SYSTEMS]=systemNames;
                let charts=xml.getElementsByTagName("response")[0].getElementsByTagName("chart");
                let chartList=[];
                for (let i=0;i<charts.length;i++){
                    let id=getXmlVal(charts[i],"chartId");
                    let name=getXmlVal(charts[i],"chartName");
                    let edition=getXmlVal(charts[i],"edition");
                    let expired=getXmlVal(charts[i],"expired");
                    if (expired == 1) continue;
                    if (id !==undefined && name !== undefined && edition !== undefined){
                        let chartEntry={
                            id:id,
                            name:name,
                            edition:edition
                        }
                        let assignments=[];
                        let quantities=charts[i].getElementsByTagName("quantity");
                        if (quantities.length < 1) continue;
                        for (let qi=0;qi<quantities.length;qi++) {
                            let quantity = quantities[qi];
                            let slots = quantity.getElementsByTagName("slot");
                            if (slots.length < 1) break;
                            for (let si = 0; si < slots.length; si++) {
                                let assignment = {};
                                assignment.name = getXmlVal(slots[si], "assignedSystemName");
                                assignment.slotUuid = getXmlVal(slots[si], "slotUuid");
                                assignments.push(assignment);
                            }
                        }
                        if (assignments.length < 1){
                            continue;
                        }
                        chartEntry.assigments=assignments;
                        chartList.push(chartEntry);
                    }
                }
                v[ST_CHARTS]=chartList;
                this.store.storeMultiple(v);
            })
            .catch((err)=>{
                let v={};
                v[ST_KNOWN_SYSTEMS]=undefined;
                v[ST_CHARTS]=undefined;
                this.store.storeMultiple(v);
                return Promise.reject(err);
            })
    }
    getCurrentChartList(){
        return this.store.getData(ST_CHARTS,[]);
    }
    getIdentifiedSystem(){
        return this.store.getData(ST_SYSTEM);
    }
    getKnownSystems(){
        return this.store.getData(ST_KNOWN_SYSTEMS);
    }

    /**
     * get the urls for a download
     * @param chartInfo contains systemName,slotUuid,edition
     * @returns Promise resolves to {chartUrl,keyUrl}
     */
    getDownloadUrls(chartInfo){
        if (! this.isLoggedIn()) return Promise.reject("not logged in");
        return this.executeShoprequest("request", {
            assignedSystemName: chartInfo.systemName,
            slotUuid: chartInfo.slotUuid,
            requestedFile: "base",
            requestedVersion: chartInfo.edition,
            username: this.getUser(),
            key: this.getKey()
        })
            .then((xml)=>{
                let err=this.getShopResultError(xml);
                if (err) throw new Error(err.toString());
                let url=getXmlVal(xml,"response.file.link");
                let keyUrl=getXmlVal(xml,"response.file.chartKeysLink");
                if (!url || ! keyUrl) throw new Error("missing url or keyUrl");
                return {
                    chartUrl:url,
                    keyUrl:keyUrl
                };
            })
    }

    register(forDongle,name){
        if (! this.isLoggedIn()) return Promise.reject("not logged in");
        let url=SHOPURL+"fpr";
        if (forDongle){
            url+="?dongle=true";
        }
        return fetchJson(url)
            .then((json)=> {
                if (!json.data || !json.data.name || !json.data.value) {
                    throw new Error("unable to get fpr");
                }

                return this.executeShoprequest("xfpr", {
                    username: this.getUser(),
                    key: this.getKey(),
                    xfprName: json.data.name,
                    xfpr: json.data.value,
                    systemName: name
                })
                    .then((xml) => {
                        let err = this.getShopResultError(xml);
                        if (err) {
                            throw new Error(err.toString());
                        }
                    });
            });
    }

}

export default new LoginHandler();
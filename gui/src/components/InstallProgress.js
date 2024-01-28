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
import React, { Component } from 'react';
import PropTypes from 'prop-types';
import Store from "../util/store";
import {fetchJson, storeHelperState} from "../util/Util";
import {TOPIC_INSTALL, UPLOADURL} from "../util/Constants";
import assign from 'object-assign';
import StatusLine from "./StatusLine";
import {setError} from "./ErrorDisplay";

const MAXAGE=120000; //120seconds
export const PROGRESS_STORE_KEY="progress";
export class ProgressDisplay extends React.Component{
    constructor(props){
        super(props);
        this.state={
            progress:{
                title: props.title
            },
        };
        this.helper=storeHelperState(this,props.store,PROGRESS_STORE_KEY)
    }
    render(){
        let progress=this.state.progress||{};
        let percentComplete = progress.progress;
        let doneStyle = {
            width: percentComplete + "%"
        };
        return (
            <div className="installProgress">
                <div className="progressContainer">
                    {(progress.loaded !== undefined)?
                        <div className="progressInfo">{(progress.loaded || 0) + "/" + (progress.total || 0)}</div>
                        :
                        <div className="progressInfo">{percentComplete+"%"}</div>
                    }
                    <div className="progressDisplay">
                        <div className="progressDone" style={doneStyle}></div>
                    </div>
                    <div className="progressInfo">{progress.title||this.props.title}</div>
                </div>
                <button className="uploadCancel button" onClick={()=>{
                    this.props.closeCallback();
                }}
                />
            </div>);
    }
}
ProgressDisplay.propTypes={
    store: PropTypes.instanceOf(Store),
    progress100: PropTypes.func,
    title:PropTypes.string,
    closeCallback: PropTypes.func
};
export class InstallProgress extends React.Component{
    constructor(props) {
        super(props);
        this.state={
            currentRequest:undefined
        };
        this.store=new Store("installProgress");
        this.fetchCurrent=this.fetchCurrent.bind(this);
        this.finishCallbackFired=undefined;
    }
    isRunning(request){
        if (! request) return false;
        if (request.state === undefined) return false;
        if (request.state >=1 && request.state <= 5 ) return true;
        return false;
    }
    statusToText(state){
        switch (state) {
            case 0: return "waiting";
            case 1: return "downloading";
            case 2: return "uploading";
            case 3: return "unpacking";
            case 4: return "testing";
            case 5: return "parsing";
            case 6: return "error";
            case 7: return "interrupted";
            case 8: return "finished";
                return "unknown";
        }
    }
    fetchCurrent(){
        fetchJson(UPLOADURL+"requestStatus")
            .then((json)=> {
                if (!json.data) throw new Error("no data");
                let newRequestId = json.data.id;
                let isRunning = this.isRunning(json.data);
                if (!isRunning) {
                    if (this.finishCallbackFired !== json.data.id) {
                        if (this.props.finishCallback) this.props.finishCallback(json.data);
                        this.finishCallbackFired = json.data.id;
                    }
                }
                if (this.state.currentRequest === undefined
                    || this.state.currentRequest.id !== json.data.id
                    || this.state.currentRequest.state !== json.data.state
                ) {
                    this.setState({currentRequest: json.data})
                }
                let title = this.statusToText(json.data.state);
                this.store.storeData(PROGRESS_STORE_KEY,
                    assign({}, json.data,
                        {title: title}));
            })
            .catch((e)=>{
                if (this.state.currentRequest !== undefined){
                    this.setState({currentRequest:undefined});
                }
            })
    }
    componentDidMount() {
        this.fetchCurrent();
        this.timer=window.setInterval(this.fetchCurrent,this.props.interval||2000);
    }
    componentWillUnmount() {
        window.clearInterval(this.timer);
    }
    cancelRequest(){
        if (this.state.currentRequest !== undefined){
            fetchJson("/upload/cancelRequest?requestId=" + this.state.currentRequest.id)
                .then((jsonData) => {
                    this.fetchCurrent();
                })
                .catch((err) => setError("unable to cancel: "+err));
        }
    }
    render() {
        let showProgress=this.state.currentRequest !== undefined;
        if (showProgress) {
            if (!this.isRunning(this.state.currentRequest)) {
                showProgress = false;
            }
        }
        let statusText=undefined;
        let statusValue="INACTIVE";
        if (this.state.currentRequest){
            if (this.isRunning(this.state.currentRequest)){
                statusText=this.statusToText(this.state.currentRequest.state);
                statusValue="READING"; //yellow
            }
            else{
                if (this.state.currentRequest.age < MAXAGE) {
                    if (this.state.currentRequest.state == 6) {
                        statusText = "Error: " + this.state.currentRequest.error;
                        statusValue = "ERROR"
                    } else {
                        statusText = this.statusToText(this.state.currentRequest.state);
                        statusValue = "READY";
                    }
                }
            }
        }
        return (
            <React.Fragment>
                {(statusText !== undefined) &&
                    <StatusLine className={this.props.className} icon={true} label="Installer" text={statusText} value={statusValue}/>
                }
                {(showProgress) &&
                    <ProgressDisplay
                        store={this.store}
                        closeCallback={()=>this.cancelRequest()}
                    />
                }
            </React.Fragment>
        );
    }

}
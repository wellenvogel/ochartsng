/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  ready state

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
import StatusLine, {stateToIcon} from './StatusLine.js';
import {fetchJson} from "../util/Util";
import {UPLOADURL} from "../util/Constants";
import {fetcher} from "../util/Fetcher";


const setStv=(old,statestr,info)=>{
    if (old.infoString === info && old.stateString===statestr) return null;
    return {
        infoString: info,
        stateString: statestr
    }
}
class ReadyState extends React.Component{
    constructor(props) {
        super(props);
        this.state={
            ready:undefined,
            stateString:'UNKNOWN',
            infoString:undefined
        };
        this.fetcher=fetcher((sequence)=>{
            return fetchJson(UPLOADURL + "canInstall")
                .then((jsonData) => {
                    const isReady=jsonData.data.canInstall;
                    let stateString=isReady?"READY":"BUSY";
                    if (this.props.onChange){
                        stateString=this.props.onChange(isReady);
                    }
                    if (jsonData.data.error){
                        stateString="ERROR";
                    }
                    this.setState((old)=>setStv(old,stateString,jsonData.data.error));
                    return sequence;
                })
                .catch((e)=>{
                    const estr=e+"";
                    if (this.props.onChange){
                        this.props.onChange(false);
                    }
                    this.setState((old)=>setStv(old,"ERROR",estr));
                    return sequence;
                })
        },this,2000);
    }
    render(){
        const value=this.state.infoString?"":this.state.stateString;
        const icon=this.state.infoString?stateToIcon("ERROR"):true;
        return (
            <StatusLine label="Status" className={this.props.className} value={value} icon={icon}>
                {this.state.infoString?this.state.infoString+"":null}
            </StatusLine>
        );
    }
}

export default ReadyState;
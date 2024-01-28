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
import StatusLine from './StatusLine.js';
import {fetchReadyState} from "../util/Fetcher";
import {fetchJson, lifecycleTimer, nestedEquals} from "../util/Util";
import {SHOPURL} from "../util/Constants";

class DongleState extends React.Component{
    constructor(props) {
        super(props);
        this.state={
            dongle: {}
        };
        this.timer=lifecycleTimer(this,(sequence)=>{
        fetchJson(SHOPURL+"donglename")
            .then((json)=>{
                //json.data.present=true; //TEST
                //json.data.name="sgl12345678"; //TEST
                this.timer.startTimer(sequence);
                try {
                    if (nestedEquals(this.state.dongle,json.data)){
                        return;
                    }
                }catch(e){}
                this.setState({dongle:json.data});
                if (this.props.onChange) this.props.onChange(json.data);
            })
            .catch((error)=>{
                this.timer.startTimer(sequence);
                if (this.state.dongle){
                    this.setState({dongle:undefined})
                    if (this.props.onChange) this.props.onChange();
                }
                if (this.props.onError) this.props.onError(error);
            })
        },props.interval||2000,true);
    }
    render(){
        let statusText="no dongle";
        let status="INACTIVE";
        let current=this.state.dongle||{};
        if (! current.canhave ){
            status=undefined;
        }
        else{
            if (current.present && current.name !== undefined){
                status="READY";
                statusText=current.name;
            }
        }
        return (
            <React.Fragment>
            {(status !== undefined) ?
                <StatusLine label="Dongle" className={this.props.className}
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

export default DongleState;
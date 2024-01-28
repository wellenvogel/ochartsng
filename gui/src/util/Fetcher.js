/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  Fetcher Utilities

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

import {fetchJson, lifecycleTimer, nestedEquals} from "./Util";
import {STATUSURL, UPLOADURL} from "./Constants";

const fetcher=(fetchMethod,thisref,interval)=>{
    let timerData=lifecycleTimer(thisref,(sequence)=>{
        fetchMethod(sequence)
            .then((sequence)=>timerData.startTimer(sequence));
    },interval,true);
    return ()=>{
        return fetchMethod();
    }
};

const setState=(thisref,stateName,value)=>{
    let newState = {};
    newState[stateName] = value;
    thisref.setState(newState);
}
export const fetchChartList=(thisref, intervall,opt_errorcallback, opt_stateName)=>{
    if (! opt_stateName) opt_stateName="charts";
    const fetch=(sequence)=> {
        return fetchJson(STATUSURL)
            .then((jsonData) => {
                try {
                    let old = thisref.state[opt_stateName];
                    if (old !== undefined && nestedEquals(old, jsonData.data.chartManager.chartSets)) {
                        return;
                    }
                } catch (e) {
                    let x = 1;//TEST
                }
                setState(thisref,opt_stateName,jsonData.data.chartManager.chartSets||[]);
                return sequence;
            })
            .catch((error) => {
                if (opt_errorcallback) {
                    opt_errorcallback("Error fetching current charts: " + error);
                }
                try{
                    if (thisref.state[opt_stateName] === undefined){
                        return sequence;
                    }
                }catch(e){}
                setState(thisref,opt_stateName,undefined);
                return sequence;
            });
    };
    return fetcher(fetch,thisref,intervall);
}

export const fetchReadyState=(thisref,intervall,changeCallback,opt_errorcallback,opt_statename)=>{
    if (! opt_statename) opt_statename="ready";
    const fetch=(sequence)=> {
        return fetchJson(UPLOADURL + "canInstall")
            .then((jsonData) => {
                try {
                    let old = thisref.state[opt_statename];
                    if (old === jsonData.data.canInstall) {
                        return sequence;
                    }
                } catch (e) {
                }
                setState(thisref,opt_statename,jsonData.data.canInstall);
                if (changeCallback) changeCallback(jsonData.data.canInstall);
                return sequence;
            })
            .catch((error) => {
                if (opt_errorcallback) {
                    opt_errorcallback("unable to fetch ready state: " + error);
                }
                try{
                    if (thisref.state[opt_statename] === false){
                        return sequence;
                    }
                }catch(e){}
                setState(thisref,opt_statename,false);
                return sequence;
            })
    };
    return fetcher(fetch,thisref,intervall);
}
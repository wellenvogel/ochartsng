/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  Utilities

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

import Promise from 'promise';
import assign from 'object-assign';
import shallowEquals from "shallow-equals";

export const fetchJson = (url, opt_options) => {
    return new Promise((resolve, reject) => {
        let checkStatus = true;
        if (opt_options && opt_options.checkStatus !== undefined) {
            checkStatus = opt_options.checkStatus;
            delete opt_options.checkStatus;
        }
        let ro = assign({}, {credentials: 'same-origin'}, opt_options);
        fetch(url, ro)
            .then((response) => {
                if (!response.ok) {
                    throw new Error(response.statusText)
                }
                return response.json()
            })
            .then((jsonData) => {
                if (jsonData.status !== 'OK' && checkStatus) {
                    throw new Error("" + jsonData.info || jsonData.status);
                }
                resolve(jsonData);
            })
            .catch((error) => {
                reject(error);
            });
    });
};

export const postFormData = (url, formData, opt_options) => {
    let body = [];
    for (let i in formData) {
        if (formData[i] === undefined) continue;
        let k = encodeURIComponent(i);
        let v = encodeURIComponent(formData[i]);
        body.push(k + '=' + v);
    }
    let fo = assign({}, opt_options, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'
        },
        body: body.join('&')
    });
    return fetchJson(url, fo);
};
export const postJson = (url, jsonData, opt_options) => {
    let fo = assign({}, opt_options, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(jsonData)
    });
    return fetchJson(url, fo);
};
/**
 * @param url {string}
 * @param file {File}
 * @param param parameter object
 *        all handlers get the param object as first parameter
 *        starthandler: will get the xhdr as second parameter - so it can be used for interrupts
 *        progresshandler: progressfunction
 *        okhandler: called when done
 *        errorhandler: called on error
 *        see https://mobiarch.wordpress.com/2012/08/21/html5-file-upload-with-progress-bar-using-jquery/
 */
export const uploadFile = (url, file, param) => {
    let type = "application/octet-stream";
    try {
        let xhr = new XMLHttpRequest();
        xhr.open('POST', url, true);
        xhr.setRequestHeader('Content-Type', type);
        xhr.addEventListener('load', (event) => {
            if (xhr.status != 200) {
                if (param.errorhandler) param.errorhandler(xhr.statusText);
                return;
            }
            let json = undefined;
            try {
                json = JSON.parse(xhr.responseText);
                if (!json.status || json.status != 'OK') {
                    if (param.errorhandler) param.errorhandler(json.info);
                    return;
                }
            } catch (e) {
                if (param.errorhandler) param.errorhandler(e);
                return;
            }
            if (param.okhandler) param.okhandler(json);
        });
        xhr.upload.addEventListener('progress', (event) => {
            if (param.progresshandler) param.progresshandler(event);
        });
        xhr.addEventListener('error', (event) => {
            if (param.errorhandler) param.errorhandler("upload error");
        });
        if (param.starthandler) param.starthandler(xhr);
        xhr.send(file);
    } catch (e) {
        if (param.errorhandler) param.errorhandler(e);
    }
};
export const hexToBase64 = (hexstring) => {
    return btoa(hexstring.match(/\w{2}/g).map(function (a) {
        return String.fromCharCode(parseInt(a, 16));
    }).join(""));
}
export const nestedEquals = (a, b) => {
    if (a instanceof Object) {
        return shallowEquals(a, b, nestedEquals);
    }
    return a === b;
}
/**
 * will call the provided callback on mount (param: false),umount(param: true), update(optional, param false)
 * will be injected after the existing lifecycle methods
 * @param thisref
 * @param callback
 * @param opt_onUpdate
 */
export const lifecycleSupport=(thisref,callback,opt_onUpdate)=> {
    let oldmount = thisref.componentDidMount;
    let oldunmount = thisref.componentWillUnmount;
    let oldupdate = thisref.componentDidUpdate;
    const newmount = ()=> {
        if (oldmount) oldmount.apply(thisref);
        callback.apply(thisref,[false]);
    };
    const newunmount = ()=> {
        if (oldunmount) oldunmount.apply(thisref);
        callback.apply(thisref,[true]);
    };
    thisref.componentDidMount=newmount.bind(thisref);
    thisref.componentWillUnmount=newunmount.bind(thisref);
    if (opt_onUpdate){
        const newupdate = ()=> {
            if (oldupdate) oldupdate.apply(thisref);
            callback.apply(thisref,[false]);
        };
    };
};

class Callback{
    constructor(cb){
        this.cb=cb;
    }
    dataChanged(data,keys){
        this.cb(data,keys);
    }
}

/**
 * get some data from a store into our state
 * @param thisref
 * @param store
 * @param opt_stateName - if set - create a sub object in the state
 * @param storeKeys
 */
export const storeHelperState=(thisref,store,storeKeys,opt_stateName)=>{
    let cbHandler=new Callback((data)=>{
        let ns={};
        if (opt_stateName) {
            ns[opt_stateName] = store.getMultiple(storeKeys);
        }
        else{
            ns= store.getMultiple(storeKeys);
        }
        thisref.setState(ns);
    });
    if (! thisref.state) thisref.state={};
    if (opt_stateName) {
        thisref.state[opt_stateName] = store.getMultiple(storeKeys);
    }
    else{
        assign(thisref.state,store.getMultiple(storeKeys));
    }
    store.register(cbHandler,storeKeys);
    lifecycleSupport(thisref,(unmount)=>{
        if ( unmount ){
            store.deregister(cbHandler)
        }
    });
};

/**
 * set up a lifecycle controlled timer
 * @param thisref
 * @param timercallback - will be called when timer fires
 * @param interval - interval
 * @param opt_autostart - call the callback in didMount
 * @returns {{startTimer: Function, currentSequence: Function}}
 *          to start the timer again - just call startTimer on the return
 *          to get the current seqeunce - just call currentSequence (e.g. to throw away a fetch result)
 */
export const lifecycleTimer=(thisref,timercallback,interval,opt_autostart)=>{
    let timerData={
        sequence:0,
        timer:undefined,
        interval:interval,
        unmounted:false
    };
    const startTimer=(sequence)=>{
        if (sequence !== undefined && sequence != timerData.sequence) {
            return;
        }
        if (timerData.unmounted) return;
        if (timerData.timer) {
            timerData.sequence++;
            window.clearTimeout(timerData.timer);
        }
        if (! timerData.interval) return;
        //console.log("lifecycle timer start",thisref,timerData.sequence);
        let currentSequence=timerData.sequence;
        timerData.timer=window.setTimeout(()=>{
            timerData.timer=undefined;
            if (currentSequence !== timerData.sequence) return;
            timercallback.apply(thisref,[currentSequence]);
        },timerData.interval);
    };
    const setTimeout=(newInterval,opt_stop)=>{
        timerData.interval=newInterval;
        if (opt_stop){
            if (timerData.timer) window.clearTimeout(timerData.timer);
            timerData.timer=undefined;
        }
    };
    const stopTimer=(sequence)=>{
        if (sequence !== undefined && sequence !== timerData.sequence) {
            return;
        }
        if (timerData.timer) {
            timerData.sequence++;
            //console.log("lifecycle timer stop",thisref,timerData.sequence);
            window.clearTimeout(timerData.timer);
            timerData.timer=undefined;
        }
    };
    lifecycleSupport(thisref,(unmount)=>{
        timerData.sequence++;
        //console.log("lifecycle timer",thisref,unmount,timerData.sequence);
        if (unmount){
            stopTimer();
            timerData.unmounted=true;
        }
        else if(opt_autostart){
            timercallback.apply(thisref,[timerData.sequence]);
        }
    });
    return {
        startTimer:startTimer,
        setTimeout:setTimeout,
        stopTimer:stopTimer,
        currentSequence:()=>{return timerData.sequence},
        guardedCall:(sequence,callback)=>{
            //console.log("guarded call start",thisref,sequence,timerData.sequence);
            if (sequence!== undefined && sequence !== timerData.sequence) {
                return;
            }
            let rt=callback();
            //console.log("guarded call end",thisref,sequence,timerData.sequence);
            return rt;
        }
    };
};


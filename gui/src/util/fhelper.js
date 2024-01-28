/*
# Copyright (c) 2024, Andreas Vogel andreas@wellenvogel.net

#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
*/
const getParam = (key,opt_default) => {
    if (opt_default === undefined) opt_default="";
    let value = RegExp("" + key + "[^&]+").exec(window.location.search);
    // Return the unescaped value minus everything starting from the equals sign or an empty string
    return decodeURIComponent(!!value ? value.toString().replace(/^[^=]+./, "") : opt_default);
};
/**
 * add an HTML element
 * @param {*} type
 * @param {*} clazz
 * @param {*} parent
 * @param {*} text
 * @returns
 */
const addEl = (type, clazz, parent, text,attributes) => {
    let el = document.createElement(type);
    if (clazz) {
        if (!(clazz instanceof Array)) {
            clazz = clazz.split(/  */);
        }
        clazz.forEach(function (ce) {
            el.classList.add(ce);
        });
    }
    if (text !== undefined) el.textContent = text;
    if (attributes !== undefined){
        for (let k in attributes){
            el.setAttribute(k,attributes[k]);
        }
    }
    if (parent) parent.appendChild(el);
    return el;
}
/**
 * call a function for each matching element
 * @param {*} selector
 * @param {*} cb
 */
const forEachEl = (selector, cb) => {
    let arr = document.querySelectorAll(selector);
    for (let i = 0; i < arr.length; i++) {
        cb(arr[i]);
    }
}

const setButtons=(config)=>{
    for (let k in config){
        let bt=document.getElementById(k);
        if (bt){
            bt.addEventListener('click',config[k]);
        }
    }
}
const fillValues=(values,items)=>{
    items.forEach((it)=>{
        let e=document.getElementById(it);
        if (e){
            if (e.tagName == 'INPUT') values[it]=e.value;
            if (e.tagName == 'DIV' || e.tagName == 'SPAN') values [it]=e.textContent;
        }
    })
};
const setValue=(id,value)=>{
    let el=document.getElementById(id);
    if (! el) return;
    if (el.tagName == 'DIV' || el.tagName == 'SPAN' || el.tagName == 'P'){
        el.textContent=value;
        return;
    }
    if (el.tagName == 'INPUT'){
        el.value=value;
        return;
    }
    if (el.tagName.match(/^H[0-9]/)){
        el.textContent=value;
        return;
    }
    if (el.tagName == 'A'){
        el.setAttribute('href',value);
        return;
    }
}

const buildUrl=(url,pars)=>{
    let delim=(url.match("[?]"))?"&":"?";
    for (let k in pars){
        url+=delim;
        delim="&";
        url+=encodeURIComponent(k);
        url+="=";
        url+=encodeURIComponent(pars[k]);
    }
    return url;
}
const fetchJson=(url,pars)=>{
    let furl=buildUrl(url,pars);
    return fetch(furl).then((rs)=>rs.json());
}
const setVisible=(el,vis,useParent)=>{
    if (typeof(el) !== 'object') el=document.getElementById(el);
    if (! el) return;
    if (useParent) el=el.parentElement;
    if (! el) return;
    if (vis) el.classList.remove('hidden');
    else el.classList.add('hidden');
}
const enableEl=(id,en)=>{
    let el=document.getElementById(id);
    if (!el) return;
    if (en) el.disabled=false;
    else el.disabled=true;
}
const fillSelect=(el,values)=>{
    if (typeof(el) !== 'object') el=document.getElementById(el);
    if (! el) return;
    el.textContent='';
    let kf=(values instanceof Array)?(k)=>values[k]:(k)=>k;
    for (let k in values){
        let o=addEl('option','',el);
        o.setAttribute('value',kf(k));
        o.textContent=values[k];
    }
}
const readFile=(file,optAsText)=>{
    return new Promise((resolve,reject)=>{
        let reader = new FileReader();
        reader.addEventListener('load', function (e) {
            resolve(e.target.result);

        });
        reader.addEventListener('error',(e)=>reject(e));
        if (optAsText) reader.readAsText(file);
        else reader.readAsBinaryString(file);
    });
}
export { readFile, getParam, addEl, forEachEl,setButtons,fillValues, setValue,buildUrl,fetchJson,setVisible, enableEl,fillSelect }
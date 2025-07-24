/*
# Copyright (c) 2022, Andreas Vogel andreas@wellenvogel.net

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
import './style/finfo.less'
import {addEl, buildUrl, forEachEl, getParam, setButtons} from './util/fhelper';
const T_OBJECT=2;
const T_CHART=1;

//taken from AvNav
const formatLonLatsDecimal=function(coordinate,axis){
    coordinate = (coordinate+540)%360 - 180; // normalize for sphere being round

    let abscoordinate = Math.abs(coordinate);
    let coordinatedegrees = Math.floor(abscoordinate);

    let coordinateminutes = (abscoordinate - coordinatedegrees)/(1/60);
    let numdecimal=2;
    //correctly handle the toFixed(x) - will do math rounding
    if (coordinateminutes.toFixed(numdecimal) === 60){
        coordinatedegrees+=1;
        coordinateminutes=0;
    }
    if( coordinatedegrees < 10 ) {
        coordinatedegrees = "0" + coordinatedegrees;
    }
    if (coordinatedegrees < 100 && axis === 'lon'){
        coordinatedegrees = "0" + coordinatedegrees;
    }
    let str = coordinatedegrees + "\u00B0";

    if( coordinateminutes < 10 ) {
        str +="0";
    }
    str += coordinateminutes.toFixed(numdecimal) + "'";
    if (axis === "lon") {
        str += coordinate < 0 ? "W" :"E";
    } else {
        str += coordinate < 0 ? "S" :"N";
    }
    return str;
};


const formatPosition=(lon,lat)=>{
    if (isNaN(lon)|| isNaN(lat)){
        return '-----';
    }
    let ns=formatLonLatsDecimal(lat, 'lat');
    let ew=formatLonLatsDecimal(lon, 'lon');
    return ns + ', ' + ew;
};
const showHideOverlay=(id,show)=>{
    let ovl=id;
    if (typeof(id) === 'string'){
        ovl=document.getElementById(id);
    }
    if (!ovl) return;
    ovl.style.visibility=show?'unset':'hidden';
    return ovl;
}
const closeOverlayFromButton=(btEvent)=>{
    let target=btEvent.target;
    while (target && target.parentElement){
        if (target.classList.contains('overlayFrame')){
            showHideOverlay(target,false);
            return;
        }
        target=target.parentElement;
    }
}
const showDataOverlay=(error,useHtml)=>{
    forEachEl('#dialog .overlayContent',(el)=>{
        if (useHtml) {
            el.innerHTML = error;
            el.classList.remove('pre');
        }
        else{
            el.classList.add('pre');
            el.textContent=error;
        }
    });
    showHideOverlay('dialog',true);
}
const getBaseUrl=(suffix)=>{
    let url=getParam('data');
    if (! url) {
        throw new Error("Error: no data parameter in page");
    }
    url=url.replace(/\?.*/,'');
    let re=new RegExp("[0-9]*/[0-9]*/[0-9]*.png")
    url=url.replace(re,'');
    if (suffix !== undefined) url+=suffix;
    return url
}
const  showLinkedInfo=(fname,chart)=>{
    let url;
    try{
        url=getBaseUrl('info');
    }catch (e){
        showDataOverlay(e);
        return;
    }
    url=buildUrl(url, {
        txt: encodeURIComponent(fname),
        chart: chart
    });
    fetch(url)
        .then((res)=>{
            if (res.status < 300){
                return(res.arrayBuffer());
            }
            else {
                return(res.text())
                    .then((txt)=>{
                        throw new String("no info found, result "+res.status+": "+txt)
                    });
            }
        })
        .then((data)=>{
            const DECODERS=['utf-8','iso8859-1'];
            let hasShown=false;
            for (let k in DECODERS) {
                try {
                    let decoder = new TextDecoder(DECODERS[k],{fatal:true});
                    let decoded = decoder.decode(data);
                    showDataOverlay(decoded);
                    hasShown=true;
                    break;
                }catch (e){
                    console.log("decoding with "+DECODERS[k]+" failed");
                }
            }
            if (! hasShown){
                showDataOverlay("Error: unable to decode data");
            }
        })
        .catch((e)=>{
            showDataOverlay("Error: "+e);
        });
}

const row=(parent,name,value)=>{
    let rt= addEl('div','row',parent);
    if (name !== undefined) {
        addEl('div', 'aname', rt, name);
    }
    if (value !== undefined){
        addEl('div','avalue',rt,value);
    }
    return rt;
}

const showChart=(parent,chartInfo)=>{
    let frame=addEl('div',"frame",parent);
    let hdg=addEl('div','heading',frame);
    addEl('div','title',hdg,chartInfo.Chart);
    for (let k in chartInfo){
        if (k === 'type' || k === 'Chart') continue;
        let v=chartInfo[k];
        if (v === undefined || v === "") continue;
        row(frame,k,v);
    }
    let url=getBaseUrl();
    fetch(buildUrl(url+"listInfo",{chart:chartInfo.Chart}))
        .then((res)=>res.json())
        .then((json)=>{
            if (json.status === 'OK' && json.data){
                let attList=addEl('div','aframe',frame);
                json.data.forEach((att)=>{
                    let arow=row(attList);
                    addEl('div','aname',arow,'Attachment')
                    let av=addEl('div','avalue',arow,att);
                    av.classList.add('link');
                    av.addEventListener('click',(ev)=>{
                        showLinkedInfo(att,chartInfo.Chart);
                    });
                });
            }
        })
        .catch((e)=>console.log(e));
}

const showOverlay=(callback)=>{
    forEachEl('#dialog .overlayContent',(el)=>{
        el.textContent='';
        callback(el);
    })
    showHideOverlay('dialog',true);
}

const usedForRender=(chartInfo)=>{
    return chartInfo.Mode !== 'UNUSED' && chartInfo.Mode !== 'OVER'
}

const chartList={};
const showCharts=()=>{
    const charts=[]
    let renderedOnly=false;
    forEachEl('#renderedOnly',(el)=>{
        renderedOnly=el.checked;
    })
    for (let k in chartList){
        const info=chartList[k];
        if (usedForRender(info) || ! renderedOnly) {
            charts.push(info);
        }
        charts.sort((a,b)=>{
            if (a.Scale !== undefined && b.Scale !== undefined){
                return a.Scale - b.Scale
            }
            return 0;
        })
    }
    showOverlay((el)=>{
        const frame=addEl('div','chartFrame',el);
        for (let k in charts){
            showChart(frame,charts[k]);
        }
    })
}
const showObjects=(parent,features,renderedOnly)=>{
    parent.textContent='';
    features.forEach((feature)=> {
        if (feature.type != T_CHART) return;
        chartList[feature.Chart] = feature;
    });
    features.forEach((feature)=>{
        if (feature.type != T_OBJECT) return;
        let chartInfo=chartList[feature.chart];
        if (! renderedOnly|| !chartInfo || usedForRender(chartInfo) ) {
            let frame = addEl('div', "frame", parent);
            let hdl = addEl('div', 'heading', frame);
            addEl('div', 'title', hdl, feature.s57featureName);
            if (feature.lat !== undefined && feature.lon !== undefined) {
                let prow = row(frame);
                addEl('div', 'aname', prow, 'Position');
                addEl('div', 'avalue', prow, formatPosition(feature.lon, feature.lat));
            }
            if (feature.chart) {
                let crow = row(frame);
                addEl('div', 'aname', crow, 'Chart');
                let ce = addEl('div', 'avalue link', crow, feature.chart);
                ce.addEventListener('click', () => {
                    showOverlay((el) => {
                        let chartInfo = chartList[feature.chart];
                        if (chartInfo) {
                            el.textContent = '';
                            showChart(el, chartInfo);
                        } else {
                            el.textContent = `no info for ${feature.chart}`;
                        }
                    })

                })
            }
            let aframe = addEl('div', 'aframe', frame);
            if (feature.attributes) {
                feature.attributes.forEach((attribute) => {
                    let arow = row(aframe);
                    addEl('div', 'aname', arow, attribute.acronym)
                    let avalue = addEl('div', 'avalue', arow, attribute.value);
                    if (attribute.value != attribute.rawValue) {
                        addEl('div', 'arawvalue', arow, `(${attribute.rawValue})`);
                    }
                })
            }
            if (feature.expandedText) {
                addEl('div', 'atext', frame, feature.expandedText);
            }
        }
    })
}
const buttonConfigs={
    closeOverlay: (ev)=>{
        closeOverlayFromButton(ev);
    },
    chartList: (ev)=>{
        showCharts();
    }
}
let features={};
document.addEventListener('DOMContentLoaded',()=>{
    setButtons(buttonConfigs);
    forEachEl('.overlayFrame',(el)=>{
        el.addEventListener('click',(ev)=>closeOverlayFromButton(ev));
    })
    forEachEl('.overlay',(el)=>{
        el.addEventListener('click',(ev)=>ev.stopPropagation());
    })
    let renderedOnly=false;
    forEachEl('#renderedOnly',(el)=>{
        renderedOnly=el.checked;
        el.addEventListener('change',(ev)=>{
            showObjects(mainContainer,features,ev.target.checked);
        })
    })
    let mainContainer=document.getElementById("fcontent");
    let url=getParam("data");
    if (! url){
        mainContainer.textContent="Error: no data url provided";
        return;
    }
    mainContainer.textContent="loading...";
    fetch(url)
        .then((res)=>res.json())
        .then((json)=>{
            if (json.data && json.data.features){
                features=json.data.features;
                showObjects(mainContainer,features,renderedOnly);
            }
            else{
                mainContainer.textContent="Error: no features found";
            }
            //mainContainer.textContent=JSON.stringify(json,null,2)
        })
        .catch((error)=>mainContainer.textContent="Error: "+error);
})
/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  chart set status

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
import StatusItem from './StatusItem.js';
import StatusLine from './StatusLine.js';

const ChartSetStatus=(props)=>{
    let info=props.info||{};
    let cache=props.cache;
    let writer=props.cacheWriter;
    let status=props.status;
    if (!props.active && status !== "DELETED"){
        status="INACTIVE";
    }
    if (props.numValidCharts === 0 && props.errors > 0 && status === "READY"){
        status="ERROR";
    }
    if ( status === 'READY' && props.reopenErrors > 0){
        status="ERROR";
    }
    let disableInfo;
    if (props.disablingSet && props.disablingSet.info){
        disableInfo=`${props.disablingSet.info.title} [${props.disablingSet.info.version}]`;
    }
    return(
        <StatusItem title={
                <span>
                    <span className="primary" >ChartSet</span>
                    <span className="secondary">{info.title||""}</span>
                </span>
                }
            >
            <StatusLine label="Status" value={status} icon={true}>
            </StatusLine>
            {disableInfo?<StatusLine label="DisabledBy" value={disableInfo}/>:null}
            <StatusLine label="Charts" value={props.numValidCharts+"/"+(props.numValidCharts+props.numIgnoredCharts)+", minScale="+props.minScale+", maxScale="+props.maxScale}/>
            <StatusLine label="Directory" value={info.directory}/>
            <StatusLine label="Info"  value={"Version="+info.version+", ValidTo="+info.validTo}/>
            {props.showDetails &&
            <React.Fragment>
                <StatusLine label="OpenErrors"  value={props.errors}/>
                <StatusLine label="ReopenErrors"  value={props.reopenErrors}/>
                {cache?<StatusLine label="MemoryCache"
                            value={cache.memoryEntries+"/"+cache.maxMemoryEntries+" ("+(Math.floor(cache.memoryBytes/1024))+"kb)"}/>:null}
                {cache?<StatusLine label="DiskCache"
                            value={cache.diskEntries+"/"+cache.maxDiskEntries+", usedMemory="+cache.diskMemorySizeKb+"kb"}/>:null}
                {writer && props.active && <StatusLine label="CacheFile" value={(Math.floor(writer.fileSize/1024))+"kb, file="+writer.fileName}/>}
            </React.Fragment>
            }
            {props.children}
        </StatusItem>
    );
};

ChartSetStatus.propTypes={
    info: PropTypes.object,
    cache: PropTypes.object,
    disablingSet: PropTypes.object,
    cacheWriter:  PropTypes.object,
    showDetails: PropTypes.bool
}


export default ChartSetStatus;

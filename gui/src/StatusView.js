/*
 Project:   AvnavOchartsProviderNG GUI
 Function:  Status display

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
import StatusLine from './components/StatusLine.js';
import StatusItem from './components/StatusItem.js';
import ChartSetStatus from './components/ChartSetStatus.js';
import {resetError, setError} from "./components/ErrorDisplay";
import {fetchJson} from "./util/Util";
import Version from '../version.js'



const GeneralStatus=(props)=>{
    return (
            <StatusItem title="ChartManager">
                <StatusLine label="Status" value={props.state} icon={true}/>
                <StatusLine label="ReadCharts" value={props.numRead}/>
                <StatusLine label="OpenCharts" value={props.openCharts}/>
                <StatusLine label="Memory" value={props.memoryKb+"kb"}/>
            </StatusItem>
    )
};
const FillerStatus=(props)=>{
    let prefillCounts=props.prefillCounts||[];
    let firstItem=true;
    return(
        <StatusItem title="CachePrefill">
            {props.prefilling?
                <React.Fragment>
                    <StatusLine label="Status" value={props.paused?"PAUSING":"FILLING"} icon={true}/>
                    <StatusLine label="currentSet" value={props.currentSet+" ("+(props.currentSetIndex+1)+"/"+(props.numSets)+")"}/>
                    <StatusLine label="currentZoom" value={props.currentZoom+" from "+props.maxZoom}/>
                </React.Fragment>
                :
                <StatusLine label="Status" value={props.paused?"PAUSING":(props.started?"READY":"WAITING")} icon={true}/>
            }
            {prefillCounts.map((item)=>{
                let info="";
                let levels=item.levels||{};
                for (let k in levels){
                    if (info != "") info+=", ";
                    info+=k+":"+levels[k];
                }
                let rt=(
                    <React.Fragment key={item.title} >
                    {firstItem?
                        <StatusLine key={item.title} className="nobreak" label="Prefills" value={item.title}/>
                        :
                        <StatusLine key={item.title} className="nobreak nosep" label="" value={item.title}/>
                    }
                    <StatusLine key={item.title+"-l"} className="nobreak nosep" label="" value={info}/>
                    </React.Fragment>
                );
                firstItem=false;
                return rt;
            })
            }
        </StatusItem>
    );
};

const OexStatus=(props)=>{
    return(
        <StatusItem title="LicenseServer">
            <StatusLine label="Version" value={props.version}/>
            <StatusLine label="Status" value={props.state} icon={true}/>
        </StatusItem>
    );
};

const TileCacheStatus=(props)=>{
    let percent=props.maxKb?100*(props.numKb||0)/props.maxKb:0;
    return (
        <StatusItem title="TileCache">
            <StatusLine label="Number" value={props.numEntries}/>
            <StatusLine label="Memory" value={`${props.numKb}kb/${props.maxKb}kb (${percent.toFixed(1)}%)`}/>
        </StatusItem>
    )
}


const url="/status/";
class StatusView extends Component {

    constructor(props){
        super(props);
        this.state={};
        this.timer=undefined;
        this.fetchStatus=this.fetchStatus.bind(this);
    }
    fetchStatus(){
        let self=this;
        fetchJson(url)
        .then(function(jsonData){
            resetError();
            self.setState({
                    data:jsonData.data
                });
        }).catch((error)=>{
            setError(error);
        })
    }
    componentDidMount(){
        this.fetchStatus();
        this.timer=window.setInterval(this.fetchStatus,2000);
    }
    componentWillUnmount(){
        window.clearInterval(this.timer);
    }
    render() {
        let managerStatus=this.state.data?this.state.data.chartManager||{}:{};
        let fillerStatus=managerStatus.cacheFiller;
        let tileCache=this.state.data?this.state.data.tileCache:undefined;
        let chartSets=managerStatus.chartSets||[];
        let oex=this.state.data?this.state.data.oexserverd||{}:{};
        return (
            <div className="view statusView">
                {this.state.data?
                    <div className="statusList scrollList">
                        <StatusItem>
                        <StatusLine label="Version" value={Version}/>
                        </StatusItem>
                        <OexStatus {...oex}/>
                        {tileCache?<TileCacheStatus {...tileCache}/>:null}
                        <GeneralStatus {...managerStatus}/>
                        {fillerStatus?<FillerStatus {...fillerStatus}/>:null}
                        {chartSets.map((set)=>{
                            let k=set.info?set.info.name:"";
                            return(
                                <ChartSetStatus {...set} showDetails={true} key={k}/>
                            );
                        })}
                    </div>
                    :
                    <p>Loading...</p>
                }
            </div>
        );
    }

}

StatusView.propTypes={
    currentView: PropTypes.string
};


export default StatusView;

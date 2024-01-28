/*
 Project:   AvnavOchartsProvider GUI
 Function:  status line

 The MIT License (MIT)

 Copyright (c) 2020 Andreas Vogel (andreas@wellenvogel.net)

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

const yellowIcon="yellow";
const greenIcon="green";
const greyIcon="grey";
const redIcon="red";
const stateToIcon=(state)=>{
    if (state) state=state.toUpperCase();
    if (state == "FILLING") return yellowIcon;
    if (state == "YELLOW") return yellowIcon;
    if (state == "BUSY") return yellowIcon;
    if (state == "READING") return yellowIcon;
    if (state == "PARSING") return yellowIcon;
    if (state == "INITIALIZING") return yellowIcon;
    if (state == "READY") return greenIcon;
    if (state == "GREEN") return greenIcon;
    if (state == "RUNNING") return greenIcon;
    if (state == "WAITING") return greyIcon;
    if (state == "PENDING") return greyIcon;
    if (state == "PAUSING") return greyIcon;
    if (state == "DELETED") return greyIcon;
    if (state == "INIT") return greyIcon;
    if (state == "INACTIVE") return greyIcon;
    if (state == "ACTIVE") return greenIcon;
    if (state == "ERROR") return redIcon;
    if (state == "RED") return redIcon;
    if (state == "RESTARTING") return greyIcon;
};


const StatusLine=(props)=>{
    let icon=props.icon;
    if (icon === true){
        icon=stateToIcon(props.value);
    }
    return(
        <div className={"statusline "+props.className}>
            <div className="statusLabel">{props.label}</div>
            <div className="statusValue">
                {icon && <span className={"statusIcon "+icon}/>}
                <span className="valueText">{props.text||props.value}</span>
            </div>
            {props.children}
        </div>
    );
};



StatusLine.propTypes={
    value: PropTypes.any,
    label: PropTypes.any,
    icon:  PropTypes.oneOfType([PropTypes.string,PropTypes.bool])
};


export default StatusLine;

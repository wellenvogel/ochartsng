/*
 Project:   AvnavOchartsProvider GUI
 Function:  overlay dialog

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
import assign from 'object-assign';
import Promise from 'promise';
export class OverlayDialog extends React.Component {
    constructor(props) {
        super(props);
        this.updateDimensions = this.updateDimensions.bind(this);
    }

    render() {
        let Content=this.props.content;
        let className="dialog";
        if (this.props.className) className+=" "+this.props.className;
        return (
            <div ref="container" className="overlay_cover_active" onClick={this.props.closeCallback}>
                <div ref="box" className={className} onClick={
                    (ev)=>{
                    ev.stopPropagation();
                    }
                }>
                    <Content closeCallback={this.props.closeCallback} updateDimensions={this.updateDimensions}/></div>
            </div>
        );
    }

    componentDidMount() {
        this.updateDimensions();
        window.addEventListener('resize', this.updateDimensions);
    }

    componentWillUnmount() {
        window.removeEventListener('resize', this.updateDimensions);

    }

    componentDidUpdate() {
        this.updateDimensions();
    }

    updateDimensions() {
        if (!this.props.content) return;
        let props = this.props;
        assign(this.refs.container.style, {
            position: "fixed",
            top: 0,
            left: 0,
            right: 0,
            bottom: 0
        });
        let rect = this.refs.container.getBoundingClientRect();
        assign(this.refs.box.style, {
            maxWidth: rect.width + "px",
            maxHeight: rect.height + "px",
            position: 'fixed',
            opacity: 0
        });
        let self = this;
        window.setTimeout(function () {
            if (!self.refs.box) return; //could have become invisible...
            let boxRect = self.refs.box.getBoundingClientRect();
            assign(self.refs.box.style, {
                left: (rect.width - boxRect.width) / 2 + "px",
                top: (rect.height - boxRect.height) / 2 + "px",
                opacity: 1
            });
        }, 0);

    }
}

OverlayDialog.propTypes={
    closeCallback: PropTypes.func, //handed over to the child to close the dialog
    className: PropTypes.string
};

export const AlertDialog=(text,opt_title)=> {
    return (props)=> {
        return (
            <div className="dialog alert">
                {opt_title && <h3>{opt_title}</h3>}
                <div className="dialogRow">
                    <span className="text">{text}</span>
                </div>
                <div className="dialogButtons action">
                    <button className="button" onClick={props.closeCallback}>OK</button>
                </div>
            </div>
        )
    };
};
export const ConfirmDialog=(props)=>{
    return (
        <div className="dialog confirm">
            {props.title && <h3>{props.title}</h3>}
            <div>{props.text}</div>
            <div className="dialogRow dialogButtons">
                <button className="button cancel" onClick={props.closeCallback}>Cancel</button>
                <button
                    className="button"
                    onClick={()=>{props.okCallback();props.closeCallback();}}
                    >
                    OK</button>
            </div>
        </div>
    );
};


class OverlayDisplay{
    constructor(thisref,opt_statefield){
        this.thisref=thisref;
        this.timer=undefined;
        this.render=this.render.bind(this);
        this.statefield=opt_statefield||'dialog';
        this.render=this.render.bind(this);
        this.setDialog=this.setDialog.bind(this);
        this.hideDialog=this.hideDialog.bind(this);
        this.alert=this.alert.bind(this);
        this.closeCallback=undefined;
    }
    render(){
        let self=this;
        if (! this.thisref.state[this.statefield]) return null;
        return (
            <OverlayDialog
                content={this.thisref.state[this.statefield]}
                closeCallback={this.hideDialog}
                />
        );
    }
    setDialog(dialog,opt_closeCallback){
        let ns={};
        ns[this.statefield]=dialog;
        this.closeCallback=opt_closeCallback;
        this.thisref.setState(ns);
    }
    hideDialog(){
        if (this.closeCallback){
            this.closeCallback();
        }
        this.setDialog();
    }
    alert(text,opt_title){
        this.setDialog(AlertDialog(text,opt_title));
    }
    confirm(text,opt_title){
        let self=this;
        let resolved=false;
        return new Promise((resolve,reject)=>{
            let cf=(props)=> {
                return <ConfirmDialog
                    closeCallback={()=>{
                        if (!resolved) {
                            self.setDialog();
                            reject(0);
                            }
                        }
                    }
                    text={text}
                    title={opt_title}
                    okCallback={()=>{
                        resolved=true;
                        self.setDialog();
                        resolve(1);
                    }}
                    />
            };
            self.setDialog(cf,()=>{reject(0)});
       });
    }
};


export default OverlayDisplay;

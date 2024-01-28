/*
 Project:   AvnavOchartsProvider GUI
 Function:  tab bar

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
import { Button, Wrapper, Menu, MenuItem } from 'react-aria-menubutton';


/*
{tabList.map((item)=>{
                    let className="tabItem";
                    if (item.name == this.props.activeTab) className+=" active";
                    return (<div className={className} key={item.name} onClick={()=>{
                        this.props.tabChanged(item.name);
                    }}>{item.value||item.name} </div>);
                })}
 */

class TabMenu extends Component {

    constructor(props){
        super(props);
    }
    render() {
        let tabList=this.props.tabList||[];
        let active=0;
        for (let idx=0;idx<tabList.length;idx++){
            if (tabList[idx].name === this.props.activeTab){
                active=idx;
            }
        }
        const menuItems=tabList.map((item,i)=>{
            let className="tabMenuItem";
            if (item.name === this.props.activeTab){
                className+= " active";
            }
            return(
                <div key={{i}}>
                    <MenuItem className={className} value={item.name}>
                        {item.value||item.name}
                    </MenuItem>
                </div>
            )
        });
        return (
            <div className="tabMenuDiv">
                <Wrapper
                    className="tabDropdown"
                    onSelection={(val)=>{
                        this.props.tabChanged(val);
                    }}>
                <Button className="tabMenuButton" tag="div">
                    <span className={"tagMenuTitle"}>
                        {this.props.title}
                    </span>
                    <span className="tagMenuIcon"></span>
                </Button>
                <Menu className="tabMenu">
                    {menuItems}
                </Menu>
                </Wrapper>
            </div>
        );
    }
}

TabMenu.propTypes={
    tabList: PropTypes.array,
    activeTab: PropTypes.string,
    title: PropTypes.string.isRequired,
    tabChanged: PropTypes.func.isRequired
};


export default TabMenu;

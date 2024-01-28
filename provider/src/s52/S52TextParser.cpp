/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S52 Text parsing
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2022 by Andreas Vogel   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 */

#include "S52TextParser.h"
#include "Exception.h"
#include "map"
#include "generated/S57AttributeIds.h"
namespace s52
{
    //some hardcoded attribute translates for NATSUR (113)
    //from expectedinput
    static std::map<int,String> natsur({
        {1,"mud"},
        {2,"clay"},
        {3,"silt"},
        {4,"sand"},
        {5,"stone"},
        {6,"gravel"},
        {7,"pebbles"},
        {8,"cobbles"},
        {9,"rock"},
        {11,"lava"},
        {14,"coral"},
        {17,"shells"},
        {18,"boulder"},
        {56,"Bo"},
        {51,"Wd"}
    });
    static Attribute::Type atypeFromFormat(const char fmtType)
    {
        switch (fmtType)
        {
        case 'I':
        case 'i':
        case 'x':
        case 'X':
        case 'u':
        case 'd':
        case 'o':
            return Attribute::T_INT;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            return Attribute::T_DOUBLE;
        }
        return Attribute::T_STRING;
    }

    static int getValueAt(const StringVector &data, int idx, int defaultv=0){
        if (idx < 0 || idx >= data.size()) return defaultv;
        return ::atoi(data[idx].c_str());
    }
    static String getStrValueAt(const StringVector &data, int idx){
        if (idx < 0 || idx >= data.size()) return String();
        return data[idx];
    }


    StringOptions S52TextParser::parseStringOptions(const S52Data* s52data,const StringVector &parts,int start){
        StringOptions rt;
        rt.hjust=getValueAt(parts,start);
        rt.vjust=getValueAt(parts,start+1);
        rt.space=getValueAt(parts,start+2);
        String chars=getStrValueAt(parts,start+3);
        if (chars.size()>=4){
            rt.style=chars[0];
            rt.weight=chars[1]-'0';
            rt.width=chars[2];
            rt.fontSize=atoi(chars.c_str()+3); //font size
        }
        rt.xoffs=getValueAt(parts,start+4);
        rt.yoffs=getValueAt(parts,start+5);
        String color=getStrValueAt(parts,start+6);
        rt.color=s52data->getColor(color);
        rt.grp=getValueAt(parts,start+7);
        return rt;
    }
    static void computeDisplayPositions(DisplayString &data,const StringOptions &options,FontManager *fm){
        int height=fm->getAscend();
        int ycorrection=height/2; //to position texts the same way as the plugin
        int width=fm->getTextWidth(StringHelper::toUnicode(data.value));
        data.relativeExtent.ymin=-fm->getAscend();
        data.relativeExtent.ymax=-fm->getDescend();
        data.relativeExtent.xmin=0;
        data.relativeExtent.xmax=width;
        data.relativeExtent.valid=true;
        //now shift according to options,
        //afterwards correct the relative extent
        data.pivotY=options.yoffs*height+ycorrection;
        data.pivotX=options.xoffs*fm->getCharWidth();
        switch (options.hjust)
        {
        case 1: // centered
            data.pivotX -= width / 2;
            break;
        case 2: // right
            data.pivotX -= width;
            break;
        case 3: // left (default)
        default:
            break;
        }
        switch (options.vjust)
        {
        case 3: // top
            data.pivotY += height;
            break;
        case 2: // centered
            data.pivotY += height / 2;
            break;
        case 1: // bottom (default)
        default:
            break;
        }
        data.relativeExtent.xmin+=data.pivotX;
        data.relativeExtent.xmax+=data.pivotX;
        data.relativeExtent.ymin+=data.pivotY;
        data.relativeExtent.ymax+=data.pivotY;
        data.color=DrawingContext::convertColor(options.color.R, options.color.G,options.color.B);
    }
    DisplayString S52TextParser::parseTE(ocalloc::PoolRef pool,const S52Data *s52data, const String &param,const StringOptions &options, const Attribute::Map *attributes)
    {
        DisplayString rt(pool);
        StringVector parts=StringHelper::split(param,",");
        if (parts.size() < 2) return rt;
        StringHelper::trimI(parts[0],'\'');
        StringHelper::trimI(parts[1],'\'');
        std::stringstream stream;
        StringHelper::StreamArrayFormat(stream,parts[0],StringHelper::split(parts[1],","),
        [s52data,attributes](std::ostream &stream,const String &v, const char type){
            String aname=v;
            String defv;
            size_t spl;
            if ((spl=v.find('=')) != String::npos && spl < (v.size()-1)){
                aname=v.substr(0,spl);
                defv=v.substr(spl+1);
            }
            uint16_t aid=s52data->getAttributeCode(aname);
            //and attribute conversion - currently only NATSUR (113)    
            Attribute::Type atype=atypeFromFormat(type);
            if (! attributes->hasAttr(aid,atype)){
                auto it=attributes->find(aid);
                if (it == attributes->end()){
                    if (!defv.empty()){
                        StringHelper::DefaultArgCallback(stream,defv,type);
                        return;
                    }    
                    return;
                }
                LOG_DEBUG("attribute type mismatch for %s, fmt %c , type %d",aname,type,it->second.getType());
                return;
            }
            switch(atype){
                case Attribute::T_DOUBLE:
                    stream << s52data->convertSounding(attributes->getDouble(aid),aid);
                    return;
                case Attribute::T_INT:
                    stream << attributes->getInt(aid);
                    return;
                case Attribute::T_STRING:
                    stream << attributes->getString(aid);
                    return;        
            }
        }
        );
        String s=stream.str();
        rt.value.assign(s.c_str());
        rt.valid=!rt.value.empty();
        if (rt.valid){
            //for now only one font...
            FontManager *fm=s52data->getFontManager(s52::S52Data::FONT_TXT, options.fontSize).get();
            computeDisplayPositions(rt,options,fm);
        }
        return rt;
    }

    DisplayString S52TextParser::parseTX(ocalloc::PoolRef pool,const S52Data *s52data, const String &param,const StringOptions &options, const Attribute::Map *attributes)
    {
        DisplayString rt(pool);
        StringVector parts=StringHelper::split(param,",");
        if (parts.size() < 1) return rt;
        if (parts[0].find_first_of('\'') == 0){
            //fixed text
            StringHelper::trimI(parts[0],'\'');
            rt.value.assign(parts[0].c_str());
        }
        else
        {
            StringHelper::trimI(parts[0], '\'');
            if (parts[0] == "OBJNAM" && s52data->getSettings()->bShowNationalText)
            {
                try
                {
                    String s=attributes->getString(S57AttrIds::NOBJNM);// NOBJNAM
                    rt.value.assign(s.c_str()); 
                }
                catch (AvException &e)
                {
                }
            }
            if (rt.value.empty())
            {
                uint16_t aid = s52data->getAttributeCode(parts[0]);
                if (!attributes->hasAttr(aid))
                {
                    return rt; // invalid
                }
                if (aid == S57AttrIds::NATSUR)
                { // NATSUR, TODO: make this generic
                    StringVector parts=StringHelper::split(attributes->getString(aid),",");
                    for (auto it=parts.begin();it != parts.end();it++){
                        if (it->empty()) continue;
                        int idx = ::atoi(it->c_str());
                        auto nsi = natsur.find(idx);
                        if (nsi != natsur.end())
                        {
                            if (!rt.value.empty()) rt.value.append(1,',');
                            rt.value.append(nsi->second.c_str());
                        }
                    }
                    if (rt.value.empty()) rt.value.assign("unk");
                }
                else
                {   
                    String s;
                    auto it=attributes->find(aid);
                    if (it != attributes->end()){
                        if (it->second.getType() == s52::Attribute::T_DOUBLE){
                            s=std::to_string(s52data->convertSounding(attributes->getDouble(aid)));
                        }
                        else{
                            s=attributes->getString(aid, true);
                        }
                    }
                    rt.value.assign(s.c_str());
                }
            }
        }
        if (rt.value.empty()) return rt; //invalid
        rt.valid=true;
        FontManager *fm=s52data->getFontManager(s52::S52Data::FONT_TXT,options.fontSize).get();
        computeDisplayPositions(rt,options,fm);
        return rt;
    }
}

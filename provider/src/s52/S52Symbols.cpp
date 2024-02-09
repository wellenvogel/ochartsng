/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S52 chart symbols
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
#include "S52Symbols.h"
#include "Exception.h"
#include "FileHelper.h"
#include "StringHelper.h"
#include "Logger.h"
#include <memory>
namespace s52
{
    static int getIntAttr(tinyxml2::XMLElement *el,const char *name){
        if ( ! el->FindAttribute(name)) throw AvException(FMT("missing attribute %s for element %s in line %d",name, el->Name(),el->GetLineNum()));
        return el->IntAttribute(name);
    };
    static const char * getAttr(tinyxml2::XMLElement *el,const char *name){
        if ( ! el->FindAttribute(name)) throw AvException(FMT("missing attribute %s for element %s in line %d",name, el->Name(),el->GetLineNum()));
        return el->Attribute(name);
    };
    S52Symbols::S52Symbols(S52Data* sd) : s52data(sd) {}
    void S52Symbols::parseSymbols(const String &fileName)
    {
        if (! FileHelper::canRead(fileName)) throw FileException(fileName,"S52Symbols: cannot read");
        std::unique_ptr<tinyxml2::XMLDocument> doc = std::make_unique<tinyxml2::XMLDocument>();
        tinyxml2::XMLError rt = doc->LoadFile(fileName.c_str());
        if (rt) {
            throw FileException(fileName,FMT("S52Symbols: unable to parse XML: %s",doc->ErrorStr()));
        }
        LOG_DEBUG("S52Symbols: loading from %s",fileName);
        XMLEl *symbols=doc->FirstChildElement("chartsymbols");
        if (symbols)
        {
            for (XMLEl *element = symbols->FirstChildElement(); element != NULL; element = element->NextSiblingElement())
            {
                if (!strcmp(element->Name(), "color-tables")){
                    ProcessColorTables(element);
                    s52data->timerAdd("parseColorTables");
                }
                else if (!strcmp(element->Name(), "lookups")){
                    ProcessLookups(element);
                    s52data->timerAdd("parseLookups");
                }
                else if (!strcmp(element->Name(), "line-styles"))
                    ProcessLinestyles(element);
                else if (!strcmp(element->Name(), "patterns"))
                    ProcessPatterns(element);
                else if (!strcmp(element->Name(), "symbols")){
                    ProcessSymbols(element);
                    s52data->timerAdd("parseSymbols");
                }
            }
        }
        else{
            throw FileException(fileName,"no chart symbols found");
        }
    }
    void S52Symbols::ProcessColorTables(XMLEl *element){
        for (XMLEl *table=element->FirstChildElement();table != NULL;table=table->NextSiblingElement()){
            String tableName(table->Attribute("name"));
            if (tableName.empty()){
                LOG_ERROR("S52Symbols: color table with no name at %d",table->GetLineNum());
                continue;
            }
            LOG_DEBUG("parsing color table %s at %d",tableName,table->GetLineNum());
            ColorTable colorTable(tableName);
            for (XMLEl *entry=table->FirstChildElement();entry != NULL;entry=entry->NextSiblingElement()){
                String type(entry->Name());
                if (type == "graphics-file"){
                    colorTable.rasterFileName=entry->Attribute("name");
                    LOG_DEBUG("S52Symbols: color table %s rasterFile %s",colorTable.tableName,colorTable.rasterFileName);
                }
                else if (type == "reference"){
                    colorTable.referenceName=entry->Attribute("name");
                }
                else if (type == "color"){
                    RGBColor color;
                    color.R=getIntAttr(entry,"r");
                    color.G=getIntAttr(entry,"g");
                    color.B=getIntAttr(entry,"b");
                    String colorName(getAttr(entry,"name"));
                    if (colorName.empty()){
                        LOG_ERROR("S52Symbols: color without name at %d",entry->GetLineNum());
                    }
                    colorTable.colors[colorName]=color;
                }
            }
            LOG_DEBUG("S52Symbols: adding color table %s with %d entries",colorTable.tableName,colorTable.colors.size());
            s52data->addColorTable(colorTable);
        }    
    }
    static RenderStep tableNameToStep(const LUPname &n){
        switch(n){
            case PLAIN_BOUNDARIES:
            case SYMBOLIZED_BOUNDARIES:
                return RS_AREAS2;
            case LINES:
                return RS_LINES;    
        }
        return RS_POINTS;
    }
    void S52Symbols::ProcessLookups(XMLEl *element)
    {
        for (XMLEl *lookup = element->FirstChildElement("lookup"); lookup != NULL; lookup = lookup->NextSiblingElement())
        {
            LUPrec record;
            bool shouldAdd=true;
            record.RCID = getIntAttr(lookup, "RCID");
            record.OBCL = getAttr(lookup, "name");
            record.nSequence = getIntAttr(lookup, "id");
            for (XMLEl *subNode = lookup->FirstChildElement(); subNode != NULL; subNode = subNode->NextSiblingElement())
            {
                String nodeType(subNode->Name());
                const char *nt=subNode->GetText();
                if (nt == NULL){
                    //LOG_DEBUG("missing node text in line %d",subNode->GetLineNum());
                    continue;
                }
                String nodeText(nt);

                if (nodeType == "type")
                {

                    if (nodeText == "Area")
                        record.FTYP = AREAS_T;
                    else if (nodeText == "Line")
                        record.FTYP = LINES_T;
                    else
                        record.FTYP = POINT_T;
                }

                if (nodeType == "disp-prio")
                {
                    record.DPRI = PRIO_NODATA;
                    if (nodeText == "Group 1")
                        record.DPRI = PRIO_GROUP1;
                    else if (nodeText == "Area 1")
                        record.DPRI = PRIO_AREA_1;
                    else if (nodeText == "Area 2")
                        record.DPRI = PRIO_AREA_2;
                    else if (nodeText == "Point Symbol")
                        record.DPRI = PRIO_SYMB_POINT;
                    else if (nodeText == "Line Symbol")
                        record.DPRI = PRIO_SYMB_LINE;
                    else if (nodeText == "Area Symbol")
                        record.DPRI = PRIO_SYMB_AREA;
                    else if (nodeText == "Routing")
                        record.DPRI = PRIO_ROUTEING;
                    else if (nodeText == "Hazards")
                        record.DPRI = PRIO_HAZARDS;
                    else if (nodeText == "Mariners")
                        record.DPRI = PRIO_MARINERS;
                }
                if (nodeType == "radar-prio")
                {
                    if (nodeText == "On Top")
                        record.RPRI = RAD_OVER;
                    else
                        record.RPRI = RAD_SUPP;
                }
                if (nodeType == "table-name")
                {
                    if (nodeText == "Simplified")
                        record.TNAM = SIMPLIFIED;
                    else if (nodeText == "Lines")
                        record.TNAM = LINES;
                    else if (nodeText == "Plain")
                        record.TNAM = PLAIN_BOUNDARIES;
                    else if (nodeText == "Symbolized")
                        record.TNAM = SYMBOLIZED_BOUNDARIES;
                    else
                        record.TNAM = PAPER_CHART;
                }
                if (nodeType == "display-cat")
                {
                    if (nodeText == "Displaybase")
                        record.DISC = DISPLAYBASE;
                    else if (nodeText == "Standard")
                        record.DISC = STANDARD;
                    else if (nodeText == "Other")
                        record.DISC = OTHER;
                    else if (nodeText == "Mariners")
                        record.DISC = MARINERS_STANDARD;
                    else
                        record.DISC = OTHER;
                }
                if (nodeType == "comment")
                {
                    record.LUCM = ::atol(nodeText.c_str());
                }

                if (nodeType == "instruction")
                {
                    record.INST = nodeText;
                    record.INST.push_back('\037');
                }
                if (nodeType == "attrib-code")
                {
                    if (nodeText.size() < 6)
                    {
                        throw AvException(FMT("invalid LUP attribute %s at line %d", nodeText, subNode->GetLineNum()));
                    }
                    String attrName = nodeText.substr(0, 6);
                    try{
                        uint16_t attrId=s52data->getAttributeCode(attrName);
                        String attrValue = nodeText.substr(6);
                        record.ATTCArray[attrId] = attrValue;
                    }catch (AvException &e){
                        LOG_ERROR("unknown attribute in Rule %d: %s, ignore rule",record.RCID,attrName);
                        shouldAdd=false;
                    }
                }
            }
            if (shouldAdd) {
                record.step=tableNameToStep(record.TNAM);
                s52data->addLUP(record);
            }
        }
    }
    void S52Symbols::ProcessLinestyles(XMLEl *element) {
        for (XMLEl *symbol = element->FirstChildElement("line-style"); symbol != NULL; symbol = symbol->NextSiblingElement())
        {
            ProcessVectorSymbol(symbol,LS_PREFIX);
        }
    }
    void S52Symbols::ProcessVectorSymbol(XMLEl *symbol, String prefix)
    {
        VectorSymbol symbolData;
        const char *RCID = symbol->Attribute("RCID");
        if (! RCID){
            LOG_ERROR("invalid symbol without RCID in line %d",symbol->GetLineNum());
            return;
        }
        String name;
        bool hasDescription = false;
        for (XMLEl *subNode = symbol->FirstChildElement(); subNode != NULL; subNode = subNode->NextSiblingElement())
        {
            String nodeType(subNode->Name());
            const char *nt = subNode->GetText();
            if (nodeType == "name")
            {
                name = nt;
            }
            if (nodeType == "vector")
            {
                hasDescription = true;
                symbolData.bnbox_h = subNode->IntAttribute("height");
                symbolData.bnbox_w = subNode->IntAttribute("width");
                for (XMLEl *sub2Node = subNode->FirstChildElement(); sub2Node != NULL; sub2Node = sub2Node->NextSiblingElement())
                {
                    String node2Type(sub2Node->Name());
                    const char *nt2 = sub2Node->GetText();
                    if (node2Type == "distance")
                    {
                        symbolData.minDist = sub2Node->IntAttribute("min");
                        symbolData.maxDist = sub2Node->IntAttribute("max");
                    }
                    if (node2Type == "pivot")
                    {
                        symbolData.pivot_x = sub2Node->IntAttribute("x");
                        symbolData.pivot_y = sub2Node->IntAttribute("y");
                    }
                    if (node2Type == "origin")
                    {
                        symbolData.glx = sub2Node->IntAttribute("x");
                        symbolData.gly = sub2Node->IntAttribute("y");
                    }
                }
            }
            if (nodeType == "HPGL")
            {
                symbolData.hpgl = nt;
            }
            if (nodeType == "color-ref")
            {
                symbolData.colorRef = nt;
            }
            if (nodeType == "filltype"){
                if (String("S") != subNode->GetText()){
                    symbolData.stagger=false;
                }
            }
        }
        if (hasDescription)
        {
            s52data->addVectorSymbol(prefix + name, symbolData);
        }
    }
    void S52Symbols::ProcessPatterns(XMLEl *element) {
        for (XMLEl *symbol = element->FirstChildElement("pattern"); symbol != NULL; symbol = symbol->NextSiblingElement())
        {
            bool useVector=true;
            XMLEl *def=symbol->FirstChildElement("definition");
            if (def){
                if (String("V") != def->GetText()){
                    useVector=false;
                }
            }
            if (useVector){
                ProcessVectorSymbol(symbol,PT_PREFIX);
            }
            else{
                ProcessBitmapSymbol(symbol,PT_PREFIX);
            }
        }        
    }
    void S52Symbols::ProcessSymbols(XMLEl *element) {
        for (XMLEl *symbol = element->FirstChildElement("symbol"); symbol != NULL; symbol = symbol->NextSiblingElement())
        {
            ProcessBitmapSymbol(symbol,"");
        }
    }
    void S52Symbols::ProcessBitmapSymbol(XMLEl *symbol, String prefix)
    {
        SymbolPosition symbolData;
        const char *RCID = symbol->Attribute("RCID");
        if (! RCID){
            LOG_ERROR("invalid symbol without RCID in line %d",symbol->GetLineNum());
            return;
        }
        String name;
        bool hasBitmap = false;
        for (XMLEl *subNode = symbol->FirstChildElement(); subNode != NULL; subNode = subNode->NextSiblingElement())
        {
            String nodeType(subNode->Name());
            const char *nt = subNode->GetText();
            if (nodeType == "name")
            {
                name = nt;
            }
            else if (nodeType == "bitmap")
            {
                hasBitmap = true;
                symbolData.bnbox_w = subNode->IntAttribute("width");
                symbolData.bnbox_h = subNode->IntAttribute("height");
                XMLEl *v = subNode->FirstChildElement("distance");
                if (v)
                {
                    symbolData.minDist = v->IntAttribute("min");
                    symbolData.maxDist = v->IntAttribute("max");
                }
                v = subNode->FirstChildElement("pivot");
                if (v)
                {
                    symbolData.pivot_x = v->IntAttribute("x");
                    symbolData.pivot_y = v->IntAttribute("y");
                }
                else
                    hasBitmap = false;
                v = subNode->FirstChildElement("origin");
                if (v)
                {
                    symbolData.bnbox_x = v->IntAttribute("x");
                    symbolData.bnbox_y = v->IntAttribute("y");
                }
                v = subNode->FirstChildElement("graphics-location");
                if (v)
                {
                    symbolData.glx = v->IntAttribute("x");
                    symbolData.gly = v->IntAttribute("y");
                }
                else
                {
                    hasBitmap = false;
                }
                if (!hasBitmap)
                {
                    LOG_DEBUG("%s: incomplete bitmap config in line %d", name, subNode->GetLineNum());
                }
            }
            else if (nodeType == "filltype"){
                if (String("S") != subNode->GetText()){
                    symbolData.stagger=false;
                }
            }
            else
            {
            }
        }
        if (!hasBitmap || name.empty())
        {
            LOG_DEBUG("ignore symbol %s in line %d - no bitmap", name, symbol->GetLineNum());
        }
        else
        {
            s52data->addSymbol(prefix+name, symbolData);
        }
    }
}

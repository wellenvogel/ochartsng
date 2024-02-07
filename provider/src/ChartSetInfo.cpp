/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Set Info handler
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2020 by Andreas Vogel   *
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


#include <vector>
#include <fstream>
#include <algorithm>
#include "tinyxml2.h"

#include "ChartSetInfo.h"
#include "Logger.h"
#include "StringHelper.h"
#include "FileHelper.h"

/** base charts from shop ChartList.XML
<Chart>
    <Name></Name>
    <ID>OC-358-AA55S5</ID>
    <SE>4</SE>
    <RE>1</RE>
    <ED>2022-01-11</ED>
    <Scale>12000</Scale>
  </Chart>
  <Edition>2022/10-25</Edition>
</chartList>
*/
/** update charts from shop ChartList.XML
 <Chart>
    <Name></Name>
    <ID>OC-358-831S95</ID>
    <SE>5</SE>
    <RE>3</RE>
    <ED>2022-08-29</ED>
    <Scale>12000</Scale>
  </Chart>
  <Edition>2022/10-21</Edition>
</chartList>
*/
/** base charts from offline download ChartList.XML
<Chart>
    <Name></Name>
    <ID>OC-358-AA55S5</ID>
    <SE>4</SE>
    <RE>1</RE>
    <ED>2022-01-11</ED>
    <Scale>12000</Scale>
  </Chart>
  <Edition>2022/10-25</Edition>
</chartList>
!!! older lists do not have SE/RE
*/

/** keys from full offline download xxx.XML, similar from base/update download
 <Chart>
    <Name></Name>
    <FileName>OC-358-LM1JE4</FileName>
    <ID>FI4EJ1ML</ID>
    <RInstallKey>VerySecretKey</RInstallKey>
  </Chart>
  <ChartInfo>Finnland 2022</ChartInfo>
  <Edition>2022/10-25</Edition>
  <ExpirationDate>1693649994</ExpirationDate>
  <ChartInfoShow>session</ChartInfoShow>
  <EULAShow>once</EULAShow>
  <DisappearingDate>none</DisappearingDate>
</keyList>
*/





ChartSetInfo::Ptr ChartSetInfo::ParseChartInfo(String chartSetDirectory,String key){
    if (! FileHelper::exists(chartSetDirectory,true)){
        throw AvException(FMT("chart directory %s not found, cannot parse info",chartSetDirectory));
    }
    LOG_INFO("parse chart info for %s",chartSetDirectory);
    ChartSetInfo::Ptr parsedInfo=std::make_shared<ChartSetInfo>();
    parsedInfo->dirname=chartSetDirectory;
    parsedInfo->name=key;
    String shortDir=FileHelper::fileName(chartSetDirectory);
    static StringVector prefixes={
        "oeuSENC","oeuRNC"
    };
    bool hasPrefix=false;
    StringVector nameParts=StringHelper::split(shortDir,"-");
    if (nameParts.size() > 1){
        for (auto it=prefixes.begin();it != prefixes.end();it++){
            if (StringHelper::startsWith(shortDir,*it)){
                parsedInfo->chartSetId=nameParts[1];
            }
        } 
    }
    parsedInfo->eulaFiles=FileHelper::listDir(chartSetDirectory,"*eula*.html",false);
    //try xml files for chartinfo and keys
    StringVector xmlFiles=FileHelper::listDir(chartSetDirectory,"*.XML");
    StringVector lowerXml=FileHelper::listDir(chartSetDirectory,"*.xml");
    std::move(lowerXml.begin(),lowerXml.end(),std::back_inserter(xmlFiles));
    for (auto xml=xmlFiles.begin();xml != xmlFiles.end();xml++){
        typedef tinyxml2::XMLElement XMLEl;
        std::unique_ptr<tinyxml2::XMLDocument> doc=std::make_unique<tinyxml2::XMLDocument>();
        tinyxml2::XMLError rt=doc->LoadFile(xml->c_str());
        if (rt){
            LOG_ERROR("unable to parse xml file %s",*xml);
            continue;
        }
        XMLEl *keylist=doc->FirstChildElement("keyList");
        if (keylist){
            long mtime=FileHelper::fileTime(*xml);
            if (mtime > parsedInfo->mtime) parsedInfo->mtime=mtime;
            LOG_DEBUG("reading keylist from %s",*xml);
            //parsing keylist file
            XMLEl *chart=NULL;
            int old=parsedInfo->chartKeys.size();
            for (chart=keylist->FirstChildElement("Chart");chart != NULL;chart=chart->NextSiblingElement()){
                XMLEl *name=chart->FirstChildElement("FileName");
                XMLEl *key=chart->FirstChildElement("RInstallKey");
                if (name != NULL && key != NULL){
                    const char *nv=name->GetText();
                    const char *kv=key->GetText();
                    if (nv != NULL && *nv != 0 && kv != NULL)
                    parsedInfo->chartKeys[nv]=kv;
                }
            }
            LOG_DEBUG("parsed %d keys",parsedInfo->chartKeys.size()-old);
            XMLEl *v=keylist->FirstChildElement("ChartInfo");
            if (v != NULL && v->GetText() != NULL) parsedInfo->title=v->GetText();
            v=keylist->FirstChildElement("Edition");
            if (v != NULL && v->GetText() != NULL) parsedInfo->edition=v->GetText();
            v=keylist->FirstChildElement("ChartInfoShow");
            if (v != NULL && v->GetText() != NULL) parsedInfo->SetChartInfoMode(v->GetText());
            v=keylist->FirstChildElement("EULAShow");
            if (v != NULL && v->GetText() != NULL) parsedInfo->SetEulaMode(v->GetText());
            v=keylist->FirstChildElement("ExpirationDate");
            if (v != NULL && v->GetText() != NULL) {
                //ascii time_t
                time_t tv=std::stoll(v->GetText());
                char buffer[12];
                strftime(buffer,11,"%Y-%m-%d",localtime(&tv));
                buffer[11]=0;
                parsedInfo->validTo=buffer;
            }

        }
        else{
            //should be chartinfo
            //we can only get Edition from here - only try if not set
            if (parsedInfo->edition.empty()){
                long mtime=FileHelper::fileTime(*xml);
                if (mtime > parsedInfo->mtime) parsedInfo->mtime=mtime;
                XMLEl *chartlist=doc->FirstChildElement("chartList");
                if (chartlist != NULL){
                    XMLEl *v=chartlist->FirstChildElement("Edition");
                    if (v && v->GetText()) parsedInfo->edition=v->GetText();
                }

            }
        }
    }
    String infoFileName=FileHelper::concatPath(chartSetDirectory,LEGACY_CHARTINFO);
    if (FileHelper::exists(infoFileName))
    {
        long mtime = FileHelper::fileTime(infoFileName);
        if (mtime > parsedInfo->mtime)
            parsedInfo->mtime = mtime;
        std::ifstream infoFile(infoFileName);
        if (infoFile.is_open())
        {
            String line;
            while (std::getline(infoFile, line))
            {
                StringVector parts = StringHelper::split(line, ":");
                if (parts.size() < 2)
                    continue;
                // oesencEULAFile:EN_rrc_eula_ChartSetsForOpenCPN.html
                if (parts[0] == String("oesencEULAFile") || parts[0] == String("ochartsEULAFile"))
                {
                    String eulaFile = StringHelper::rtrim(parts[1]);
                    parsedInfo->eulaFiles.push_back(eulaFile);
                }
                // oesencEULAShow:once
                else if (parts[0] == String("oesencEULAShow") || parts[0] == String("ochartsEULAShow"))
                {
                    parsedInfo->SetEulaMode(parts[1], false);
                }
                // ChartInfo:Deutsche Gewässer 2020;2020-13;2021-03-28
                else if (parts[0] == String("ChartInfo"))
                {
                    StringVector infoParts = StringHelper::split(parts[1], ";");
                    if (infoParts.size() >= 1 && parsedInfo->title.empty())
                    {
                        parsedInfo->title = infoParts[0];
                    }
                    if (infoParts.size() >= 2 && parsedInfo->edition.empty())
                    {
                        // convert edition 2021-21 to 2021/01-21
                        StringVector editionParts = StringHelper::split(infoParts[1], "-");
                        if (editionParts.size() == 2)
                        {
                            parsedInfo->edition = StringHelper::format("%s/01-%s", editionParts[0], editionParts[1]);
                        }
                    }
                    if (infoParts.size() >= 3 && parsedInfo->validTo.empty())
                    {
                        parsedInfo->validTo = infoParts[2];
                    }
                }
                // ChartInfoShow:Session
                else if (parts[0] == String("ChartInfoShow"))
                {
                    parsedInfo->SetChartInfoMode(parts[1], false);
                }
                // UserKey:xxxxxx
                else if (parts[0] == String("UserKey"))
                {
                    parsedInfo->userKey = StringHelper::rtrim(parts[1]);
                }
            }
            infoFile.close();
        }
        else
        {
            LOG_ERROR("unable to open chartinfo in %s for reading", infoFileName);
        }
    }
    parsedInfo->infoParsed=true;
    if (parsedInfo->eulaMode != SHOW_NEVER && parsedInfo->eulaFiles.size() <1){
        LOG_ERROR("eula required for %s but no eula files",chartSetDirectory.c_str());
        parsedInfo->eulaMode =SHOW_NEVER;
    }
    LOG_INFO("%s",parsedInfo->ToString().c_str());
    return parsedInfo;
}

void ChartSetInfo::SetEulaMode(const String & mode,bool overwrite){
    if (eulaMode != UNKNOWN && ! overwrite) return;
    String umode = StringHelper::toUpper(StringHelper::rtrim(mode));
    if (umode == String("ONCE")) {
        eulaMode=SHOW_ONCE;
    }
    else if (umode == String("ALWAYS") || umode == String("SESSION")){
        eulaMode=SHOW_SESSION;
    }
}
void ChartSetInfo::SetChartInfoMode(const String &mode,bool overwrite){
    if (chartInfoMode != UNKNOWN && ! overwrite) return;
    String umode = StringHelper::toUpper(StringHelper::rtrim(mode));
    if (umode == String("ONCE")) {
        chartInfoMode=SHOW_ONCE;
    }
    else if (umode == String("ALWAYS")  || umode == String("SESSION")){
        chartInfoMode=SHOW_SESSION;
    }
}

String ChartSetInfo::ToString(){
    return StringHelper::format("Chart Set: parsed=%s,dir=%s, name=%s, title=%s, version=%s, to=%s, eulaMode=%d, infoMode=%d, numEula=%d",
            (infoParsed?"true":"false"),
            dirname,
            name,
            title,
            edition,
            validTo,
            (int)eulaMode,
            (int)chartInfoMode,
            (int)eulaFiles.size()
            );
}
void ChartSetInfo::ToJson(StatusStream &stream){
    stream["name"]=name;
    stream["directory"]=dirname;
    stream["version"]=edition;
    stream["validTo"]=validTo;
    stream["title"]=getTitle();
    stream["id"]=chartSetId;
}
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Request Handler
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
#ifndef LISTREQUESTHANDLER_
#define LISTREQUESTHANDLER_
#include "RequestHandler.h"
#include "Logger.h"
#include "ChartSetInfo.h"
#include "ChartManager.h"
#include <vector>
#include "StringHelper.h"

class ListRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/list";
private:
    ChartManager::Ptr    manager;
public:
    /**
     * create a request handler
     * @param chartList
     * @param name url compatible name
     */
    ListRequestHandler(ChartManager::Ptr manager){
        this->manager=manager;
    }
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
       String entries("");
       RenderSettings::ConstPtr renderSettings=manager->GetS52Data()->getSettings();
       ChartSetInfoList map=manager->ListChartSets();
       json::JSON items=json::Array();
       for (auto it=map.begin();it!=map.end();it++){
           const ChartSetInfo *info=it->get();
           String title=info->getTitle();
           if (!info->edition.empty()){
               title.append("[").append(info->edition).append("]");
           }
           ChartSet::Ptr set=manager->GetChartSet(info->name);
           if (! set ) continue;
           String chartInfo=FMT("%s version %s valid to %s",info->getTitle(),info->edition,info->validTo);
           json::JSON obj;
           obj["name"]=title;
           obj["chartKey"]=info->name;
           obj["url"]=FMT("http://%s:%d/charts/%s",request->serverIp,request->serverPort,info->name);
           obj["icon"]=FMT("http://%s:%d/static/icon.png",request->serverIp,request->serverPort);
           obj["time"]=info->mtime;
           obj["canDelete"]=false;
           obj["canDownload"]=false;
           obj["eulaMode"]=(int)info->eulaMode;
           obj["infoMode"]=(int)info->chartInfoMode;
           obj["version"]=info->edition;
           obj["validTo"]=info->validTo;
           obj["info"]=chartInfo;
           obj["sequence"]=manager->GetChartSetSequence(info->name);
           String host=StringHelper::trim(request->header["host"],' ');
           obj["tokenUrl"]=FMT("http://%s/tokens/?request=script",host);
           obj["tokenFunction"]="ochartsProvider";
           obj["hasFeatureInfo"]=true;
           items.append(obj);
       }
       HTTPStringResponse *rt=new HTTPJsonResponse(items);
       return rt;       
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+"*";
    }

};

#endif /* LISTREQUESTHANDLER_ */


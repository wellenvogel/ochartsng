/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Settings Request Handler
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2024 by Andreas Vogel   *
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
#ifndef SETTINGSREQUESTHANDLER_
#define SETTINGSREQUESTHANDLER_
#include "RequestHandler.h"
#include "Logger.h"
#include "ChartManager.h"
#include <vector>
#include "StringHelper.h"
#include "SettingsManager.h"




class SettingsRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/settings";
private:
    SettingsManager::Ptr manager;
    
public:
    /**
     * create a request handler
     */
    SettingsRequestHandler(SettingsManager::Ptr manager){
        this->manager=manager;
    }  
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
        String url = request->url.substr(URL_PREFIX.length());
        StringHelper::replaceInline(url,"//","/");
        if (StringHelper::startsWith(url,"/")){
            url=url.substr(1);
        }
        if (url == "get"){
            return new HTTPJsonResponse(manager->getSettings(),true);
        }
        if (url == "ready"){
            json::JSON rt;
            JSONOK(rt);
            rt["ready"]=manager->getAllowChanges()?"READY":"INITIALIZING";
            return new HTTPJsonResponse(rt,false);
        }
        //TODO: check encoding?
        if (url == "set" ){
            std::shared_ptr<std::ostringstream> data=std::make_shared<std::ostringstream>();
            if (!manager->getAllowChanges()){
                return new HTTPJsonErrorResponse("still initializing");
            }
            String lenPar;
            GET_HEADER(lenPar,"content-length");            
            size_t uploadSize = std::atoll(lenPar.c_str());
            size_t wr=WriteFromInput(request,data,uploadSize);
            if (wr != uploadSize){
                return new HTTPJsonErrorResponse("not all bytes uploaded");
            }
            json::JSON jdata=json::JSON::Load(data->str());
            try{
                manager->updateSettings(jdata);
                return new HTTPJsonOk();
            }catch (AvException &ex){
                return new HTTPJsonErrorResponse(ex.msg());
            }
            
        }       
        if (StringHelper::startsWith(url,"enable")){
            String key;
            GET_QUERY(key,"chartSet");
            String enableV;
            GET_QUERY(enableV,"enable");           
            bool enable= enableV == "1";
            //TODO: check for existance
            bool rs=false;
            try{
                rs=manager->enableChartSet(key,enable);
            }catch (AvException &e){
                return new HTTPJsonErrorResponse(FMT("unable to change set: %s",e.msg()));
            }
            json::JSON result;
            JSONOK(result);
            result["changed"]=rs;
            return new HTTPJsonResponse(result);
        }
        return new HTTPResponse();  
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+"*";
    }

};

#endif /* SETTINGSREQUESTHANDLER_ */


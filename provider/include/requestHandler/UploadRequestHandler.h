/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Upload Request Handler
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
#ifndef UPLOADREQUESTHANDLER_
#define UPLOADREQUESTHANDLER_
#include "RequestHandler.h"
#include "Logger.h"
#include "ChartInstaller.h"
#include "StringHelper.h"


class UploadRequestHandler : public RequestHandler {
    const unsigned long long MAXUPLOAD=3LL*1024LL*1024LL*1024LL; //3GB
public:
    const String  URL_PREFIX="/upload";
private:
    ChartInstaller::Ptr   installer;
    
    
public:
    UploadRequestHandler(ChartInstaller::Ptr installer){
        this->installer=installer;
    }
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
        String url = request->url.substr(URL_PREFIX.size());
        StringHelper::replaceInline(url,"//","/");
        if (StringHelper::startsWith(url,"/")){
            url=url.substr(1);
        }
        if (!installer->IsActive()){
            return new HTTPJsonErrorResponse("not ready");
        }
        if (StringHelper::startsWith(url,"uploadzip")){
            String lenPar;
            GET_HEADER(lenPar,"content-length");            
            size_t uploadSize = std::atoll(lenPar.c_str());
            if (uploadSize > MAXUPLOAD){
                return new HTTPJsonErrorResponse(FMT("upload to big, allowed: %ld",MAXUPLOAD));                
            }
            ChartInstaller::Request installRequest;
            try{
                installRequest=installer->StartRequestForUpload();
                if (! installRequest.stream){
                    throw AvException("no stream for upload");
                }
                int lastProgress=0;
                size_t written=WriteFromInput(request,installRequest.stream,uploadSize,
                    [&lastProgress,installRequest,this](int percent){
                        if (lastProgress != percent){
                            this->installer->UpdateProgress(installRequest.id,percent,true);
                            lastProgress=percent;
                        }
                    });
                if (written != uploadSize){
                    throw AvException(FMT("only written %d from %d bytes",written,uploadSize));
                }
                installer->FinishUpload(installRequest.id);
            }catch (Exception &e){
                if (installRequest.id >= 0){
                    try{
                        installer->FinishUpload(installRequest.id,e.what());
                    }catch (Exception &x){}
                }
                return new HTTPJsonErrorResponse(e.what());
            }
            json::JSON obj;
            obj["request"]=installRequest.id;
            return new HTTPJsonResponse(obj);  
        }
        if (StringHelper::startsWith(url,"requestStatus")){
            String requestId=GetQueryValue(request,"requestId");
            int id=::atoi(requestId.c_str());
            try{
                ChartInstaller::Request rt;
                if (id > 0){
                    rt=installer->GetRequest(id);
                }
                else{
                    rt=installer->CurrentRequest();
                }
                json::JSON obj;
                rt.ToJson(obj);
                return new HTTPJsonResponse(obj);
            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"cancelRequest")){
            String requestId;
            GET_QUERY(requestId,"requestId");
            int id=::atoi(requestId.c_str());
            try{
                bool res=installer->InterruptRequest(id);
                if (! res){
                    throw AvException(FMT("request %d not found",id));
                }
                json::JSON obj;
                return new HTTPJsonResponse(obj);
            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"downloadShop")){
            String chartUrl;
            String keyUrl;
            GET_QUERY(chartUrl,"chartUrl");
            GET_QUERY(keyUrl,"keyUrl");
            if (keyUrl.empty()){
                return new HTTPJsonErrorResponse("keyUrl must not be empty");
            }
            try{
                ChartInstaller::Request installRequest=installer->StartRequest(chartUrl,keyUrl);
                json::JSON obj;
                obj["request"]=installRequest.id;
                return new HTTPJsonResponse(obj); 
            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"canInstall")){
            bool ready=installer->CanInstall();
            json::JSON obj;
            obj["canInstall"]=ready;
            return new HTTPJsonResponse(obj);
        }
        if (StringHelper::startsWith(url,"deleteSet")){
            String name;
            GET_QUERY(name,"name");
            try{
                installer->DeleteChartSet(name);
                return new HTTPJsonOk();
            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"parseDir")){
            String name;
            GET_QUERY(name,"name");
            try{
                json::JSON rt;
                int num=installer->ParseChartDir(name);
                rt["num"]=num;
                return new HTTPJsonResponse(rt);
            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        return new HTTPResponse();  
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+String("*");
    }

};

#endif /* UPLOADREQUESTHANDLER_ */


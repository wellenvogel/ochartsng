/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Static Request Handler
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2020 by Andreas Vogel   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later versi<?xml version="1.0" encoding="utf-8"?><response><result>1</result><key>8300da1466cdb5487e83536dfcd23bb3</key></response>on.                                   *
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
#ifndef SHOPREQUESTHANDLER_H
#define SHOPREQUESTHANDLER_H
#include "RequestHandler.h"
#include "Logger.h"
#include "ItemStatus.h"
#include "StringHelper.h"
#include "OexControl.h"
#include <curl/curl.h>
#include "json.hpp"

static const char *ERROR_FMT="<?xml version=\"1.0\" encoding=\"utf-8\"?><response><result>%s</result><info>%s</info></response>";
static const String ERR_NOCURL="AV1";
static const String ERR_CURL="AV2";
static const String ERR_INVALID_URL="AV3";
static const String ERR_CONNECT="AV4";
static const long SHOP_TIMEOUT=120; //sec
static const long SHOP_CONNECT_TIMEOUT=60;


class ShopError : public HTTPStringResponse{
    public:
        ShopError(const String &code, const String &info=""):
            HTTPStringResponse("text/xml",
                StringHelper::format(ERROR_FMT,code.c_str(),info.c_str())){                    
        }
};

class ShopResponse : public HTTPDataResponse{
    public:
    ShopResponse(): HTTPDataResponse("application/octet-stream",std::make_shared<DataVector>()){
    }
    int write(void *src,size_t length){
        data->insert(data->end(),(uint8_t *)src,(uint8_t *)src+length);
        return 0;
    }
};
static size_t write_function(void *data, size_t size, size_t nmemb, void *userp){
    ShopResponse *response=(ShopResponse*)userp;
    response->write(data,size*nmemb);
    return size*nmemb;
}


class ShopRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/shop";
    const int prefixLen=URL_PREFIX.length();
private:
    String shopUrl;
    String predefinedSystemName;
public:
   
    ShopRequestHandler(String shopUrl, String predefinedSystemName){
        curl_global_init(CURL_GLOBAL_SSL);
        this->shopUrl=shopUrl;
        this->predefinedSystemName=predefinedSystemName;
        LOG_INFO("shop request handler for %s",shopUrl.c_str());
    }
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
        String url = request->url.substr(prefixLen);
        StringHelper::replaceInline(url,"//","/");
        if (url[0] == '/'){
            url=url.substr(1);
        }
        if (StringHelper::startsWith(url,"fpr")){
            String forDongleP=GetQueryValue(request,"dongle");
            bool forDongle=StringHelper::toLower(forDongleP) == "true";
            String alternativeP=GetQueryValue(request,"alternative");
            bool alternative=StringHelper::toLower(alternativeP) == "true";
            bool forDownload=StringHelper::toLower(GetQueryValue(request,"forDownload")) == "true";
            if (forDongle){
                if (! OexControl::Instance()->DonglePresent()){
                    if (forDownload) return new HTTPErrorResponse(500,"no dongle");
                    return new HTTPJsonErrorResponse("no dongle present");
                }
            }
            try{
                OexControl::FPR fpr=OexControl::Instance()->GetFpr(SHOP_CONNECT_TIMEOUT*1000,forDongle,alternative);
                if (forDownload){
                    HTTPStringResponse *rt= new HTTPStringResponse("application/octet-stream",fpr.value);
                    rt->responseHeaders["Content-Disposition"]=FMT("attachment; filename=\"%s\"", StringHelper::SanitizeString(fpr.name));
                    return rt;
                }
                json::JSON obj;
                obj["name"]=fpr.name;
                obj["value"]=fpr.value;
                obj["forDongle"]=fpr.forDongle;
                obj["alternative"]=alternative;
                return new HTTPJsonResponse(obj);

            }catch (Exception &e){
                if (forDownload) return new HTTPErrorResponse(500,e.what());
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"donglepresent")){
            try{
                bool present=OexControl::Instance()->DonglePresent(SHOP_CONNECT_TIMEOUT*1000);
                json::JSON obj;
                obj["present"]=present;
                return new HTTPJsonResponse(obj);

            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"donglename")){
            try{
                bool present=OexControl::Instance()->DonglePresent(SHOP_CONNECT_TIMEOUT*1000);
                String name="";
                if (present){
                    name=OexControl::Instance()->GetDongleName(SHOP_CONNECT_TIMEOUT*1000);
                }
                json::JSON obj;
                obj["present"]=present;
                obj["name"]=name;
                obj["canhave"]=OexControl::Instance()->CanHaveDongle();
                return new HTTPJsonResponse(obj);

            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"predefinedname")){
            try{
                json::JSON obj;
                obj["name"]=predefinedSystemName;
                return new HTTPJsonResponse(obj);

            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"canhavedongle")){
            try{
                json::JSON obj;
                obj["canhave"]=OexControl::Instance()->CanHaveDongle();
                return new HTTPJsonResponse(obj);

            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url,"ochartsversion")){
            try{
                json::JSON obj;
                obj["version"]=OexControl::Instance()->GetOchartsVersion();
                return new HTTPJsonResponse(obj);

            }catch (Exception &e){
                return new HTTPJsonErrorResponse(e.what());
            }
        }
        if (StringHelper::startsWith(url, "api"))
        {
            String shopUrl = this->shopUrl + String("?fc=module&module=occharts&controller=apioesu");
            CURL *curl = curl_easy_init();
            if (!curl)
                return new ShopError(ERR_NOCURL);
            curl_easy_setopt(curl, CURLOPT_URL, shopUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request->rawQuery.size());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->rawQuery.c_str());
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,SHOP_CONNECT_TIMEOUT);
            curl_easy_setopt(curl,CURLOPT_TIMEOUT,SHOP_TIMEOUT);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
            ShopResponse *response = new ShopResponse();
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
            CURLcode ret = curl_easy_perform(curl);
            if (ret != CURLE_OK)
            {
                delete response;
                LOG_ERROR("shop request failed: %d:%s", ret,curl_easy_strerror(ret));
                HTTPResponse *rt=nullptr;
                if (ret == CURLE_COULDNT_RESOLVE_HOST || ret == CURLE_COULDNT_CONNECT){
                    rt=new ShopError(ERR_CONNECT,curl_easy_strerror(ret));
                }
                else{
                    rt=new ShopError(ERR_CURL,curl_easy_strerror(ret));
                }
                curl_easy_cleanup(curl);
                return rt;
            }
            char *contentType = NULL;
            CURLcode ctres = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
            if (ctres == CURLE_OK && contentType)
            {
                response->mimeType = String(contentType);
            }
            curl_easy_cleanup(curl);
            return response;
        }
        return new ShopError(ERR_INVALID_URL,String("invalid url: ")+url);
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+String("*");
    }

};

#endif /* SHOPREQUESTHANDLER_H */


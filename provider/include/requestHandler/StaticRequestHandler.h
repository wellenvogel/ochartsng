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
#ifndef STATICREQUESTHANDLER_
#define STATICREQUESTHANDLER_
#include "RequestHandler.h"
#include "Logger.h"
#include "ItemStatus.h"
#include "StringHelper.h"

class StaticRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/static";
    const int prefixLen=URL_PREFIX.length();
private:
    String baseDir;
    String fontDir;
public:
   
    StaticRequestHandler(const String &baseD, const String &fontD):baseDir(baseD),fontDir(fontD){
        LOG_INFO("Static request handler for: %s, fonts: %s",baseDir,fontDir);
    }
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
        String url = request->url.substr(prefixLen);
        StringHelper::replaceInline(url,"//","/");
        if (url[0] == '/'){
            url=url.substr(1);
        }
        url=StringHelper::SanitizeString(StringHelper::beforeFirst(url,"?",true));
        LOG_DEBUG("Static request for %s",url.c_str());
        String name=FileHelper::concatPath(baseDir,url);
        if (! FileHelper::exists(name) && StringHelper::endsWith(url,".ttf")){
            name=FileHelper::concatPath(fontDir,url);
        }
        if (FileHelper::exists(name)){
            String mimeType="application/octet-string";
            if (StringHelper::endsWith(url,".js")) mimeType="text/javascript";
            if (StringHelper::endsWith(url,".html")) mimeType="text/html";
            if (StringHelper::endsWith(url,".css")) mimeType="text/css";
            if (StringHelper::endsWith(url,".png")) mimeType="image/png";
            if (StringHelper::endsWith(url,".svg")) mimeType="image/svg+xml";
            if (StringHelper::endsWith(url,".ttf")) mimeType="font/ttf";
            return handleGetFile(request,mimeType,name,true);
        }
        HTTPResponse *rt=new HTTPResponse();
        rt->responseHeaders["Access-Control-Allow-Origin"]=corsOrigin(request);
        return rt;       
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+String("*");
    }

};

#endif /* STATICREQUESTHANDLER_ */


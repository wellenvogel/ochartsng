/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Token Request Handler
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
#ifndef TOKENREQUESTHANDLER_H
#define TOKENREQUESTHANDLER_H
#include "RequestHandler.h"
#include "Logger.h"
#include "TokenHandler.h"
#include "StringHelper.h"



class TokenRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/tokens";
    const String JSFILE="tokenHandler.js";
private:
    TokenHandler::Ptr handler;
    String baseDir;
    HTTPResponse *handleGetLocalFile(HTTPRequest *request,String mimeType,String fileName){
        String name=FileHelper::concatPath(baseDir,fileName);
        return handleGetFile(request,mimeType,name,true);
    }
    
public:
    /**
     * create a request handler
     * @param chartList
     * @param name url compatible name
     */
    TokenRequestHandler(TokenHandler::Ptr handler, const String &baseDir){
        this->baseDir=baseDir;
        this->handler=handler;
    }
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
        if (request->method == "OPTIONS"){
            HTTPResponse *resp=new HTTPResponse();
            resp->valid=true;
            resp->code=204;
            resp->responseHeaders["Access-Control-Allow-Origin"]=corsOrigin(request);
            resp->responseHeaders["Access-Control-Max-Age"]="86400";
            resp->responseHeaders["Access-Control-Allow-Methods","GET, OPTIONS"];
            NameValueMap::iterator it=request->header.find("access-control-request-headers");
            if (it != request->header.end()){
                resp->responseHeaders["Access-Control-Allow-Headers"]=it->second;
            }
            return resp;
        }
        long start = Logger::MicroSeconds100();
        String url = request->url.substr(URL_PREFIX.length());
        StringHelper::replaceInline(url,"//","/");
        if (StringHelper::startsWith(url,"/")){
            url=url.substr(1);
        }
        String action=GetQueryValue(request,"request");
        if (action.empty()){
            LOG_DEBUG("empty token request type");
            return new HTTPErrorResponse(400,"missing parameter request");
        }
        if (action == "script"){
            LOG_DEBUG("download script request");
            return handleGetLocalFile(request,"application/javascript",JSFILE);
        }
        if (action == "testHTML"){
            LOG_DEBUG("download test request");
            return handleGetLocalFile(request,"text/html","tktest.html");
        }
        bool isTest=false;
        if (action != "key" && action != "test"){
            LOG_DEBUG("invalid token request %s",action);
            return new HTTPErrorResponse(400,FMT("invalid token action %s",action));
        }
        if (action == "test") isTest=true;
        String sessionId=GetQueryValue(request,"sessionId");
        TokenResult rt;
        if (sessionId.empty()){
            //new session
            LOG_DEBUG("new session");
            rt=handler->NewToken();
        }
        else{
            LOG_DEBUG("next token for %s",sessionId);
            rt=handler->NewToken(sessionId);
        }
        if (isTest){
            String turl=GetQueryValue(request,"url");
            if (! turl.empty()){
                DecryptResult result=handler->DecryptUrl(turl);
            }
        }
        if (rt.state == TokenResult::RES_OK){
            json::JSON data;
            data["key"]=rt.key;
            data["sequence"]=rt.sequence;
            data["sessionId"]=rt.sessionId;
            HTTPResponse *ret= new HTTPJsonResponse(data);
            ret->responseHeaders["Access-Control-Allow-Origin"]=corsOrigin(request);
            return ret;
        }
        if (rt.state == TokenResult::RES_TOO_MANY){
            HTTPResponse *ret=new HTTPJsonErrorResponse("too many clients");
            ret->responseHeaders["Access-Control-Allow-Origin"]=corsOrigin(request);
            return ret; 
        }
        return new HTTPJsonErrorResponse("unknown decrypt error"); //TODO: detailed error
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+"*";
    }

};

#endif /* CHARTREQUESTHANDLER_H */


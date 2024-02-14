/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  HTTP worker thread
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
#include <regex>
#include <sstream>
#include "Worker.h"
#include "Logger.h"
#include "RequestHandler.h"
#include "SocketHelper.h"

Worker::Worker(AcceptInterface *accepter,HandlerMap *handlers) : Thread(){
    this->accepter=accepter;
    this->handlers=handlers;
}
Worker::~Worker(){}
void Worker::run(){
    LOG_INFO("HTTP worker Thread started");
    int socket=-1;
    while (! shouldStop()){
        socket=accepter->Accept();
        if (socket >= 0){
            LOG_DEBUG("start processing on socket %d",socket);
            SocketHelper::SetNonBlocking(socket);
            HandleRequest(socket);
            LOG_DEBUG("finished request on socket %d",socket);
            close(socket);
        }
        else{
            if (! shouldStop()) Timer::microSleep(100); //avoid CPU peak in start phase
        }
    }
    LOG_INFO("HTTP worker thread stopping");
}

void Worker::HandleRequest(int socket){
    SocketAddress local=SocketHelper::GetLocalAddress(socket);
    SocketAddress peer=SocketHelper::GetRemoteAddress(socket);
    String localIP=SocketHelper::GetAddress(local);
    int localPort=SocketHelper::GetPort(local);
    String remoteIP=SocketHelper::GetAddress(peer);
    int remotePort=SocketHelper::GetPort(peer);
    LOG_DEBUG("request local %s:%d, remote %s:%d",
            localIP.c_str(),localPort,remoteIP.c_str(),remotePort);
    StringVector requestArray;
    std::stringstream line;
    bool headerDone=false;
    Timer::SteadyTimePoint start = Timer::steadyNow();
    unsigned long timeout=5000;
    while (!headerDone && !shouldStop() && ! Timer::steadyPassedMillis(start,timeout)) {
        char ch;
        if (SocketHelper::Read(socket, &ch, sizeof (ch), Timer::remainMillis(start,timeout)) < 1) {
            LOG_DEBUG("no header data from socket");
            return;
        }
        if (ch != '\n') {
            line << ch;
        } else {
            String hdr=StringHelper::rtrim(line.str());
            LOG_DEBUG("header on socket %d: %s",socket,hdr.c_str());
            if (hdr.empty()) {
                headerDone = true;
            } else {
                requestArray.push_back(hdr);
                line.str("");
            }
        }
    }

    if (!headerDone){
        LOG_DEBUG("no header received");
        return;
    }
    HTTPRequest request;
    request.serverPort=localPort;
    request.serverIp=localIP;
    request.socket=socket;
    //TODO: read data for POST
    ParseAndExecute(socket,requestArray,&request);
    
}

static String unescape(String encoded){
    std::stringstream rt;
    char ch;
    int i, ii;
    for (i=0; i<encoded.length(); i++) {
        if (int(encoded[i])=='%') {
            sscanf(encoded.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
            rt << ch;
            i=i+2;
        } else {
            if (encoded[i] == '+') rt << ' ';
            else rt << encoded[i];
        }
    }
    return (rt.str());
}

#define MAXBODY 100000
void Worker::ParseAndExecute(int socket,StringVector header,HTTPRequest *request){
    LOG_DEBUG("found %ld header lines",header.size());
    if (header.size() < 1) return;
    StringVector firstLine=StringHelper::split(header[0]," ");
    if (firstLine.size()<2){
        LOG_DEBUG("invalid request line %s",header[0].c_str());
        return;
    }
    request->url=StringHelper::unescapeUri(firstLine[1]);
    size_t pos;
    request->method=StringHelper::toUpper(firstLine[0]);
    if ((pos = request->url.find("?")) != String::npos) {
        request->rawQuery  = request->url.substr(pos + 1);
        request->url   = request->url.substr(0, pos);

        LOG_DEBUG("query string = %s", request->rawQuery.c_str());
        StringVector queryParam=StringHelper::split(request->rawQuery,"&");
        for (auto it=queryParam.begin();it != queryParam.end();it++){
            String pair=*it;
            String id, val;
            if ((pos = pair.find("=")) != String::npos) {
                id  = pair.substr(0, pos);
                val = pair.substr(pos + 1);
                LOG_DEBUG("query id %s val %s",id.c_str(),val.c_str());
                request->query[id]=val;
            }
        }
    }
    for (size_t hdr_line = 1 ; hdr_line < header.size() ; hdr_line++)
    {
        String hdrName, hdrValue,line;
        int pos = -1;

        line = header[hdr_line];

        /* Find first ':' and parse header-name and value. */
        if ((pos = line.find(":")) != String::npos) {
            hdrName  = line.substr(0, pos);
            hdrValue = StringHelper::rtrim(line.substr(pos + 1));

            if (StringHelper::toUpper(hdrValue) != String("COOKIE")){
                LOG_DEBUG("Header [%s] Value [%s]",hdrName.c_str(),
                                                      hdrValue.c_str());
                request->header[StringHelper::toLower(hdrName)]=hdrValue;
            } else {
                StringVector cookies=StringHelper::split(hdrValue,"=;");
                String cookieID, cookieVal;
                for (auto it=cookies.begin();it != cookies.end();it+=2){
                    
                    cookieID = StringHelper::rtrim(*it);
                    cookieVal = StringHelper::rtrim(*(it+1));
                    LOG_DEBUG("cookie id [%s] value [%s]",cookieID.c_str(), 
                        cookieVal.c_str());
                    /* Add the cookie to the request cookie-array */
                    request->cookies[cookieID]=cookieVal;
                }
            }
        }
    }
    if (request->method == String("POST") || request->method == String("PUT")) {
        NameValueMap::iterator it = request->header.find("content-type");
        if (it == request->header.end() || (StringHelper::toLower(it->second).find("application/x-www-form-urlencoded") != 0)) {
            LOG_INFO("can only handle POST variables with application/x-www-form-urlencoded by default");
        } else {
            int postSize = 0;
            it = request->header.find("content-length");
            if (it == request->header.end()) {
                ReturnError(socket, 500, "missing content-length");
                return;
            }
            postSize = std::atoi(it->second.c_str());
            if (postSize < 0 || postSize > MAXBODY) {
                ReturnError(socket, 500, "invalid content-length");
                return;
            }
            char buffer[postSize + 1];
            int rd = 0;
            Timer::SteadyTimePoint start =Timer::steadyNow();
            long timeout=10000;
            while (rd < postSize && ! Timer::steadyPassedMillis(start,timeout)) {
                int cur = SocketHelper::Read(socket, buffer + rd, postSize - rd, Timer::remainMillis(start,timeout));
                if (cur < 0) {
                    ReturnError(socket, 500, "unexpected end of input");
                    return;
                }
                rd += cur;
            }
            if (rd < postSize) {
                ReturnError(socket, 500, "unexpected end of input");
                return;
            }
            buffer[postSize] = 0;
            String body(buffer, postSize);
            StringVector postPar=StringHelper::split(body,"&");
            for(auto it=postPar.begin(); it != postPar.end();it++) {
                String pair = *it;
                if ((pos = pair.find("=")) != String::npos) {
                    String id = unescape(pair.substr(0, pos));
                    String val = unescape(pair.substr(pos + 1));
                    LOG_DEBUG("query id %s val %s", id.c_str(), val.c_str());
                    request->query[id] = val;
                }
            }
        }
    }
    //now the request is parsed, start processing
    RequestHandler *handler = handlers->GetHandler(request->url);
    if (!handler) {
        ReturnError(socket, 404, "not found");
        return;
    }
    HTTPResponse *response=nullptr;
    try{
    response = handler->HandleRequest(request);
    if (response->valid) {
        SendData(socket, response, request);
    } else {
        ReturnError(socket, 404, "not found");
    }
    }catch (Exception &e){
        LOG_DEBUG("Exception while handling HTTP request %s: %s",header[0],e.what());
        ReturnError(socket,500,e.what());
    }
    if (response  != nullptr) delete response;
}


void Worker::ReturnError(int socket, int code, const char *description) {
    LOG_DEBUG("HTTPdWorker::ReturnError(%d, %d, %s)", socket, code, description);
    char response[700];

    snprintf(response,699, "HTTP/1.1 %d %s\r\nserver: AvNav-Provider\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "content-type: text/plain\r\n"
            "content-length: %ld\r\n\r\n%s",
            code, description, (long)strlen(description), description);
    response[699]=0;
    SocketHelper::WriteAll(socket,response, strlen(response),5000);
    return;
}
static const String sHTMLEol("\r\n");
void Worker::WriteHeadersAndCookies(int socket, HTTPResponse* response, HTTPRequest* request){
    std::stringstream data;
    if (request->cookies.size() > 0) {
        NameValueMap::iterator it;
        for (it=request->cookies.begin();it != request->cookies.end();it++) {
            data << StringHelper::format("Set-Cookie: %s=%s%s",it->first.c_str(),
                it->second.c_str(),sHTMLEol.c_str());
        }
    }
    if (response->responseHeaders.size()>0){
        NameValueMap::iterator it;
        for (it=response->responseHeaders.begin();it != response->responseHeaders.end();it++){
            data << it->first;
            data << ": ";
            data << it->second;
            data << sHTMLEol;
        }
    }
    data << sHTMLEol;
    String out=data.str();
    SocketHelper::WriteAll(socket, out.c_str(), out.length(),1000 );
}
void Worker::SendData(int socket,HTTPResponse *response,HTTPRequest *request){
    int code=response->code;
    String phrase="OK";
    if (code >= 400) phrase="ERROR";
    std::stringstream sHTTP;
    sHTTP << StringHelper::format("HTTP/1.1 %d %s%s",code,phrase.c_str(),sHTMLEol.c_str());
    sHTTP << "Server: AvNav-Provider" << sHTMLEol;
    sHTTP << "Content-Type: " <<  response->mimeType  << sHTMLEol;
    sHTTP << "Cache-Control: no-store, no-cache, must-revalidate, max-age=0" << sHTMLEol;
    sHTTP << "Connection: Close" << sHTMLEol;
    String hdr=sHTTP.str();
    SocketHelper::WriteAll(socket, hdr.c_str(), hdr.length(),1000 );
    response->SetContentLength();
    WriteHeadersAndCookies(socket,response,request);
    if (response->useCallback())
    {
        response->callback(socket);
    }
    else
    {
        unsigned long maxLen = 0;
        if (response->SupportsChunked())
        {
            maxLen = 10000;
            const char *data;
            while (maxLen > 0)
            {
                data = response->GetData(maxLen);
                if (maxLen <= 0)
                    return;
                int written = SocketHelper::WriteAll(socket, data, maxLen, 10000);
                if (written < 0 || (unsigned long)written != maxLen)
                {
                    LOG_ERROR("unable to write all data to socket %d, expected %ld, written %d", socket, maxLen, written);
                }
                maxLen = 10000;
            }
        }
        else
        {
            const char *data = response->GetData(maxLen);
            SocketHelper::WriteAll(socket, data, maxLen, 10000);
        }
    }
}


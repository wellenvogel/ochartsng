/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Request Handler
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
#ifndef REQUESTHANDLER_H
#define REQUESTHANDLER_H
#include "Timer.h"
#include "SimpleThread.h"
#include "Types.h"
#include "StringHelper.h"
#include "FileHelper.h"
#include "Logger.h"
#include "SocketHelper.h"
#include <vector>
#include <map>
#include <fstream>
#include <memory>
#include "json.hpp"

class HTTPResponse{
public:
    bool valid;
    String mimeType;
    NameValueMap responseHeaders;
    int code=0;
    HTTPResponse(String mimeType){
        this->mimeType=mimeType;
        this->valid=true;
        this->code=200;
    }REQUESTHANDLER_H
    HTTPResponse(){
        this->valid=false;
        this->code=401;
    }
    virtual ~HTTPResponse(){
    }
    String GetMimeType(){return mimeType;}
    virtual bool SupportsChunked(){return false;}
    virtual void SetContentLength(){
        responseHeaders["Content-Length"]=std::to_string(GetLength());
    }
    virtual const char * GetData(/*inout*/unsigned long &maxLen){return NULL;}
    virtual bool useCallback() const {return false;}
    virtual bool callback(int socketFd){return false;}
    protected:
    virtual unsigned long GetLength(){return 0;}
};

class CallbackHTTPResponse : public HTTPResponse{
    public:
    using WriteCallback=std::function<void(int socketFd,CallbackHTTPResponse *r)>;
    protected:
    WriteCallback writer;
    public:
    CallbackHTTPResponse(WriteCallback cb, const String &mimeType, bool chunked=true):writer(cb),HTTPResponse(mimeType){
        if (chunked){
            responseHeaders["Transfer-Encoding"]="chunked";
        }
    }
    virtual bool useCallback() const {return true;}
    virtual bool callback(int socketFd){
        if (!  writer) return false;
        writer(socketFd,this);
        return true;
    }
    virtual void SetContentLength(){}
    static int writeChunk(int socket,const void *buf, int len, long timeout=0l){
        char hbuf[20];
        snprintf(hbuf,19,"%X\r\n",len);
        hbuf[19]=0;
        int plen=strlen(hbuf);
        Timer::SteadyTimePoint start=Timer::steadyNow();
        int rd=SocketHelper::WriteAll(socket,hbuf,plen,timeout);
        if (rd != plen){
            return -1;
        }
        long remain=timeout>0?Timer::remainMillis(start,timeout):timeout;
        if (len > 0){
            rd=SocketHelper::WriteAll(socket,buf,len,remain);
            if (rd != len) return rd;
        }
        hbuf[0]='\r';
        hbuf[1]='\n';
        remain=timeout>0?Timer::remainMillis(start,timeout):timeout;
        rd=SocketHelper::WriteAll(socket,hbuf,2,timeout);
        if (rd != 2) return -1;
        return len;
    }
    static int writeLastChunk(int socket, long timeout=0L){
        return writeChunk(socket,nullptr,0,timeout);
    }
};

class HTTPStringResponse : public HTTPResponse{
protected:
    String data;
public:
    HTTPStringResponse(String mimeType,String data):
        HTTPResponse(mimeType){
        this->data=data;
    }
    HTTPStringResponse(String mimeType):
        HTTPResponse(mimeType){
        this->data=String();
    }    
    virtual ~HTTPStringResponse(){
    }
    virtual unsigned long GetLength(){return data.length();}
    virtual const char * GetData(unsigned long &maxLen){ return data.c_str();}
};

class HTTPErrorResponse : public HTTPStringResponse{
    public:
    HTTPErrorResponse(int code, const String info=""):HTTPStringResponse("text/plain",info){
        this->code=code;
    }
};


class HTTPDataResponse : public HTTPResponse{
protected:
    DataPtr data;
public:
    HTTPDataResponse(String mimeType,DataPtr data):
        HTTPResponse(mimeType){
        this->data=data;
    }    
    virtual ~HTTPDataResponse(){
    }
    virtual unsigned long GetLength(){return data->size();}
    virtual const char * GetData(unsigned long &maxLen){ return (const char *)data->data();}
};

class HTTPJsonErrorResponse : public HTTPStringResponse{
public:    
    HTTPJsonErrorResponse(String errors): HTTPStringResponse("application/json"){
        LOG_DEBUG("json error response: %s",errors);
        json::JSON obj;
        obj["status"]="ERROR";
        obj["info"]=errors;
        data=obj.dump(0,"");
    }
    virtual ~HTTPJsonErrorResponse(){
    }    
};

class HTTPJsonResponse : public HTTPStringResponse{
public:    
    HTTPJsonResponse(const json::JSON &obj, bool wrap=true): HTTPStringResponse("application/json"){
        if (wrap){
            json::JSON rt;
            JSONOK(rt);
            rt["data"]=obj;
            data=rt.dump(0,"");
        }
        else{
            data=obj.dump(0,"");
        }
        LOG_DEBUG("json response: %s",data);
    }
    virtual ~HTTPJsonResponse(){
    }    
};

class HTTPJsonOk : public HTTPJsonResponse{
    public:
    HTTPJsonOk():HTTPJsonResponse(json::JSON()){}
};
class HTTPStreamResponse: public HTTPResponse{
private:
    std::istream *stream;
    char *buffer;
    unsigned long bufferLen;
    unsigned long len;
    unsigned long alreadyRead;
public:
    HTTPStreamResponse(String mimeType,std::istream *stream,unsigned long len):
        HTTPResponse(mimeType){
            this->stream=stream;
            this->len=len;
            this->buffer=NULL;
            this->alreadyRead=0;
            this->bufferLen=0;
    }
    virtual ~HTTPStreamResponse(){
        delete stream;
        if (buffer != NULL) delete []buffer;
    }    
    virtual bool SupportsChunked(){return true;}
    virtual unsigned long GetLength(){return len;}
    virtual const char * GetData(/*inout*/unsigned long &maxLen){
        unsigned long toRead=maxLen;
        if ((alreadyRead + toRead) > len){
            toRead=len-alreadyRead;
        }
        if (toRead == 0){
            maxLen=0;
            return NULL;
        }
        if (bufferLen < toRead){
            if (buffer) delete [] buffer;
            buffer= new char[toRead];
            bufferLen=toRead;
        }
        stream->read(buffer,toRead);
        maxLen=stream->gcount();
        alreadyRead+=maxLen;
        return buffer;
    }    
};

class HTTPRequest {
public:
    NameValueMap    query;
    NameValueMap    header;
    NameValueMap    cookies;
    String          url;
    int             serverPort;
    String          serverIp;
    String          method;
    int             socket;
    String          rawQuery;
};

class RequestHandler{
public:
    virtual HTTPResponse *HandleRequest(HTTPRequest *request)=0;
    virtual String GetUrlPattern()=0;
    virtual ~RequestHandler(){};
    String corsOrigin(HTTPRequest *request){
        NameValueMap::iterator it=request->header.find("Origin");
        if (it==request->header.end()) return "*";
        return it->second;
    }
    HTTPResponse *handleGetFile(HTTPRequest *request,String mimeType,String name, bool cors=true){
        std::ifstream  *stream=new std::ifstream(name.c_str(),std::ios::in|std::ios::binary);
        if (!stream->is_open() ){
            delete stream;
            return new HTTPResponse();
        }
        HTTPResponse *rt=new HTTPStreamResponse(mimeType,stream,(unsigned long)FileHelper::fileSize(name));
        if (cors){
            rt->responseHeaders["Access-Control-Allow-Origin"]=corsOrigin(request);
            rt->responseHeaders["Access-Control-Allow-Headers"]="*";
        }
        return rt;
    }
    
protected:
    
    String    GetHeaderValue(HTTPRequest *request,String name){
        NameValueMap::iterator it = request->header.find(name);
        if (it == request->header.end()) {
            return String();
        }
        return it->second;
    }
    String    GetQueryValue(HTTPRequest *request,String name){
        NameValueMap::iterator it = request->query.find(name);
        if (it == request->query.end()) {
            return String();
        }
        return it->second;
    }
    size_t WriteFromInput(HTTPRequest *request, std::shared_ptr<std::ostream> openOutput,
                          size_t len, long chunkTimeOut=20000)
    {
        size_t bRead = 0;
        static constexpr const int BUFSIZE = 10000;
        char buffer[BUFSIZE];
        while (bRead < len)
        {
            size_t rdLen = (len - bRead);
            if (rdLen > BUFSIZE)
                rdLen = BUFSIZE;
            int rd = SocketHelper::Read(request->socket, buffer, (int)rdLen, chunkTimeOut);
            if (rd <= 0)
            {
                throw AvException(FMT("unable to read %ld bytes from stream", len));
            }
            openOutput->write(buffer, rd);
            if (openOutput->bad())
            {
                throw AvException(FMT("unable to write %d bytes", rd));
            }
            bRead += rd;
        }
        return bRead;
    }
};

#define GET_HEADER(var,name) {\
    var=GetHeaderValue(request,name);\
    if (var == String()) return new HTTPJsonErrorResponse("missing header " name);\
    }
#define GET_QUERY(var,name) {\
    var=GetQueryValue(request,name); \
    if (var == String()) return new HTTPJsonErrorResponse("missing parameter " name);\
    }

#endif /* REQUESTHANDLER_H */


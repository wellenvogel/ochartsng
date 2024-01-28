/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test Request Handler
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
#ifndef _TESTREQUESTHANDLER_H
#define _TESTREQUESTHANDLER_H
#include "RequestHandler.h"
#include "Logger.h"

#include "SimpleThread.h"
#include "StringHelper.h"
#include "FileHelper.h"
#include "Tiles.h"
#include "DrawingContext.h"
#include "Renderer.h"


class TestRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/test/";
    const int tileBufferSize=Coord::TILE_SIZE*Coord::TILE_SIZE*4;
private:
    String name;
    String urlPrefix;
    Renderer::Ptr renderer;
    Renderer::RenderInfo info;   
    
    HTTPResponse *tryOpenFile(String base,String file,String mimeType){
        String fileName=FileHelper::concatPath(base,file);
        if (!FileHelper::exists(fileName)){
            return NULL;
        }
        LOG_DEBUG("open file %s",fileName.c_str());
        std::ifstream  *stream=new std::ifstream(name.c_str(),std::ios::in|std::ios::binary);
        if (!stream->is_open() ){
            delete stream;
            return NULL;
        }
        HTTPResponse *rt=new HTTPStreamResponse(mimeType,stream,(unsigned long)FileHelper::fileSize(name));
        rt->responseHeaders["Access-Control-Allow-Origin"]="*";
        return rt;
    }
    
    HTTPResponse *handleSequenceRequest(){
        json::JSON data;
        data["sequence"]=0;
        HTTPResponse *rt=new HTTPJsonResponse(data);
        rt->responseHeaders["Access-Control-Allow-Origin"]="*";
        return rt;
    }
public:
    /**
     * create a request handler
     * @param chartList
     * @param name url compatible name
     */
    TestRequestHandler(Renderer::Ptr renderer, String name){
        this->name=name;
        this->renderer=renderer;
        urlPrefix=URL_PREFIX+name+String("/");
        info.pngType=name;
        
    }
    
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
    
        long start = Logger::MicroSeconds100();
        String url = request->url.substr(urlPrefix.length());
        StringHelper::replaceInline(url,"//","/");
        if (StringHelper::startsWith(url,"/")){
            url=url.substr(1);
        }
    
        if (StringHelper::startsWith(url,"sequence")){
            return handleSequenceRequest();
        }
        TileInfo tile(url, name);
        if (!tile.valid) {
            LOG_DEBUG("invalid url %s", url);
            return new HTTPResponse();
        }
        Renderer::RenderResult result;
        try{
            renderer->renderTile(tile,info,result);
        }catch (Exception &e){
            return new HTTPResponse();
        }
        DataPtr png=result.getResult();
        HTTPResponse *response = new HTTPDataResponse("image/png", png);
        LOG_DEBUG("http render: %s, sz=%lld",
                    result.timer.toString(),
                    (long long)png->size()
                    );
        return response;
    }
    virtual String GetUrlPattern() {
        return urlPrefix+String("*");
    }

};

#endif /* CHARTREQUESTHANDLER_H */


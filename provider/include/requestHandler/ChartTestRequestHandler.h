/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Test Request Handler
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
#ifndef CHARTTESTREQUESTHANDLER_H
#define CHARTTESTREQUESTHANDLER_H
#include "RequestHandler.h"
#include "Logger.h"
#include "ChartManager.h"
#include "SimpleThread.h"
#include "StringHelper.h"
#include "s52/S52Data.h"
#include "DrawingContext.h"
#include "PngHandler.h"



class ChartTestRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/charttest/";
    
private:
    ChartManager::Ptr chartManager;
public:
    /**
     * create a request handler
     */
    ChartTestRequestHandler(ChartManager::Ptr chartManager){
        this->chartManager=chartManager;
    }
    
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
    
        long start = Logger::MicroSeconds100();
        String url = request->url.substr(URL_PREFIX.length());
        StringHelper::replaceInline(url,"//","/");
        if (StringHelper::startsWith(url,"/")){
            url=url.substr(1);
        }
        StringVector parts=StringHelper::split(url,"/",2);
        if (parts.size() != 3){
            LOG_DEBUG("missing chartSet in %s",url);
            return new HTTPJsonErrorResponse("not enough parameters");
        }
        String chartSetKey=parts[1];
        String chart=parts[2];
        if (parts[0] == "open")
        {
            String renderStr=GetQueryValue(request,"render");
            bool render=StringHelper::toLower(renderStr) == "true";
            try
            {
                bool withOpeners=chartManager->HasOpeners();
                auto chartPtr=chartManager->OpenChart(chartSetKey, chart,!withOpeners);
                if (withOpeners && ! chartPtr){
                    chartPtr=chartManager->OpenChart(chartSetKey, chart, true);
                }
        
                long done = Logger::MicroSeconds100();
                LOG_DEBUG("chartTest load: %s %s, time=%ld",
                      chartSetKey, chart,
                      done - start);
                if (render){
                    //render some tile from the middle of the chart
                    ZoomLevelScales scales(1);
                    int zoom=scales.FindZoomForScale(chartPtr->GetNativeScale());
                    Coord::Extent ce=chartPtr->GetChartExtent();
                    Coord::WorldXy mp=ce.midPoint();
                    Coord::TileInfoBase tile=Coord::worldPointToTile(mp,zoom);
                    std::unique_ptr<DrawingContext> drawing(DrawingContext::create(Coord::TILE_SIZE, Coord::TILE_SIZE));
                    std::unique_ptr<PngEncoder> encoder(PngEncoder::createEncoder("spng"));
                    RenderContext context;
                    context.s52Data=chartManager->GetS52Data();
                    Coord::TileBox box = Coord::tileToBox(tile.zoom, tile.x, tile.y);
                    for (int i=0;i<chartPtr->getRenderPasses();i++){
                        chartPtr->Render(i,context, *drawing, box);
                    }
                    encoder->setContext(drawing.get());
                    DataPtr res=std::make_shared<DataVector>();
                    encoder->encode(res);
                    long rdone=Logger::MicroSeconds100();
                    LOG_DEBUG("chartTest render: %s %s, time=%ld",
                      chartSetKey, chart,
                      rdone-done );
                    LOG_DEBUG("%s",drawing->getStatistics());
                    json::JSON rt;
                    rt["url"]=FMT("%d/%d/%d",tile.zoom,tile.x,tile.y);
                    return new HTTPJsonResponse(rt,true);
                }
                HTTPResponse *response = new HTTPJsonOk();
                return response;

            }
            catch (AvException &e)
            {
                return new HTTPJsonErrorResponse(e.msg());
            }
        }
        if (parts[0] == "close"){
            try
            {
                bool closed = chartManager->CloseChart(chartSetKey, chart);
                if (closed)
                {
                    return new HTTPJsonOk();
                }
                return new HTTPJsonErrorResponse("nothing closed");
            }
            catch (AvException &e)
            {
                return new HTTPJsonErrorResponse(e.msg());
            }
        }
        return new HTTPJsonErrorResponse("unknown mode");
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+String("*");
    }

};

#endif /* CHARTREQUESTHANDLER_H */


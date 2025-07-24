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
#ifndef _TESTDRAWINGREQUESTHANDLER_H
#define _TESTDRAWINGREQUESTHANDLER_H
#include "RequestHandler.h"
#include "Logger.h"

#include "SimpleThread.h"
#include "StringHelper.h"
#include "FileHelper.h"
#include "Tiles.h"
#include "DrawingContext.h"
#include "Renderer.h"
#include "PngHandler.h"
#include "RenderHelper.h"


class TestDrawingRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/drawing/";
    const int tileBufferSize=Coord::TILE_SIZE*Coord::TILE_SIZE*4;
private:
    String name;
    String urlPrefix;
    Renderer::RenderInfo info;
    ChartManager::Ptr manager;
    
public:
    /**
     * create a request handler
     * @param chartList
     * @param name url compatible name
     */
    TestDrawingRequestHandler(ChartManager::Ptr m,String n):manager(m),name(n){
        urlPrefix=URL_PREFIX+name+String("/");
    }
    
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
        s52::S52Data::ConstPtr s52data=manager->GetS52Data();
        long start = Logger::MicroSeconds100();
        String url = request->url.substr(urlPrefix.length());
        StringHelper::replaceInline(url,"//","/");
        if (StringHelper::startsWith(url,"/")){
            url=url.substr(1);
        }
        String noborder=GetQueryValue(request,"noborder");
        String type=GetQueryValue(request,"type");
        String spoint=GetQueryValue(request,"points");
        std::vector<Coord::PixelXy> points;
        StringVector parr=StringHelper::split(spoint,";");
        for (int i=0;i<parr.size();i++){
            int x=-1,y=-1;
            if (sscanf(parr[i].c_str(),"%d,%d",&x,&y) == 2){
                points.push_back(Coord::PixelXy(x,y));
            }
        }
        int repeat=atoi(GetQueryValue(request,"repeat").c_str());
        if (repeat <= 0) repeat=1;
        int width=atoi(GetQueryValue(request,"width").c_str());
        if (width < 1) width=1;
        String scolor=GetQueryValue(request,"color");
        DrawingContext::ColorAndAlpha color=DrawingContext::convertColor(0, 0, 0);
        if (! scolor.empty()){
            color=s52data->convertColor(s52data->getColor(scolor));
        }
        String symbolName=GetQueryValue(request,"symbol");
        double symbolScale=atof(GetQueryValue(request,"scale").c_str());
        if (symbolScale <= 0) symbolScale=1;
        int symbolRotation=atoi(GetQueryValue(request,"rotation").c_str());
        String tileUrl=GetQueryValue(request,"url");
        s52::SymbolPtr symbol;
        if (StringHelper::startsWith(type,"sym")){
            symbol=s52data->getSymbol(symbolName,symbolRotation,symbolScale);
            if (! symbol){
                throw AvException("symbol not found");
            }
        }
        Timer::Measure timer;
        std::unique_ptr<DrawingContext> drawing(DrawingContext::create(Coord::TILE_SIZE, Coord::TILE_SIZE));
        std::unique_ptr<PngEncoder> encoder(PngEncoder::createEncoder(info.pngType));
        timer.add("create");
        drawing->extThickLine=GetQueryValue(request,"exthick") != "0";
        int numItems=0;
        for (int r=0;r<repeat;r++){
            if (StringHelper::startsWith(type,"line")){
                for(int i=0;i<points.size()-1;i+=2){
                    numItems++;
                    if (type == "line"){
                        drawing->drawLine(points[i],points[i+1],color);
                    }
                    if (type == "linea"){
                        drawing->drawLine(points[i],points[i+1],color,true);
                    }
                    if (type == "lineaa"){
                        drawing->drawAaLine(points[i],points[i+1],color);
                    }
                    if (type == "lined"){
                        DrawingContext::Dash d;
                        d.draw=4;
                        d.gap=4;
                        drawing->drawLine(points[i],points[i+1],color,false,&d);
                    }
                    if (type == "linew"){
                        drawing->drawThickLine(points[i],points[i+1],color,false,nullptr,width);
                    }
                }
            }
            if (StringHelper::startsWith(type, "symbol"))
            {
                for (int i = 0; i < points.size(); i++)
                {
                    numItems++;
                    Coord::PixelXy draw = points[i];
                    draw.x -= symbol->pivot_x;
                    draw.y -= symbol->pivot_y;
                    drawing->drawSymbol(draw, symbol->width, symbol->height, symbol->buffer->data());
                }
            }
            if (StringHelper::startsWith(type, "symline") && points.size() > 1)
            {
                Coord::PixelXy last=points[0];
                for (int i = 1; i < points.size(); i++)
                {
                    numItems++;
                    RenderHelper::renderSymbolLine(s52data,*drawing,last,points[i],symbol->name);
                    last=points[i];
                }
            }
            if (StringHelper::startsWith(type,"area") && points.size()>=3){
                for (int i=0;i<(points.size()-2);i+=3){
                    drawing->drawTriangle(points[i],points[i+1],points[i+2],color);
                }
            }
            if (StringHelper::startsWith(type,"symarea") && points.size()>=3){
                if (symbol){
                    DrawingContext::PatternSpec pattern(symbol->buffer.get()->data(),symbol->width,symbol->height);
                    pattern.distance=symbol->minDist;
                    pattern.stagger=symbol->stagger;
                    for (int i=0;i<(points.size()-2);i+=3){
                        drawing->drawTriangle(points[i],points[i+1],points[i+2],color,&pattern);
                    }
                }

            }
            if (type == "tile"){
                TileInfo tile(tileUrl,"test");
                StringVector result({tileUrl,"Invalid url"});
                if (tile.valid) {
                    Coord::TileBox ext=Coord::tileToBox(tile.zoom,tile.x,tile.y);
                    result[1]=FMT("xmin=%d",ext.xmin);
                    result.push_back(FMT("xmax=%d",ext.xmax));
                    result.push_back(FMT("ymin=%d",ext.ymin));
                    result.push_back(FMT("ymax=%d",ext.ymax));
                }
                Coord::PixelXy point=points.size()?points[0]:Coord::PixelXy(30,30);
                int fontsize=14;
                FontManager::Ptr fm=s52data->getFontManager(s52::S52Data::FONT_TXT,fontsize);
                for (auto it=result.begin();it != result.end();it++){
                    RenderHelper::drawText(fm,*drawing,*it,point,color);
                    point.shift(0,fontsize+5);
                }
            }
            //TODO: render drawings
        }
        if (noborder.empty()){
            DrawingContext::ColorAndAlpha c = DrawingContext::convertColor(255, 0, 0);
            drawing->drawHLine(0, 0, Coord::TILE_SIZE - 1, c);
            drawing->drawHLine(Coord::TILE_SIZE - 1, 0, Coord::TILE_SIZE - 1, c);
            drawing->drawVLine(0, 0, Coord::TILE_SIZE - 1, c);
            drawing->drawVLine(Coord::TILE_SIZE - 1, 0, Coord::TILE_SIZE - 1, c);
        }
        timer.add("draw");
        encoder->setContext(drawing.get());
        DataPtr png=std::make_shared<DataVector>();
        bool rt = encoder->encode(png);
        timer.add("png");
        HTTPResponse *response = new HTTPDataResponse("image/png", png);
        LOG_DEBUG("draw test: type=%s, repeat=%d, points=%d, items=%d, %s, sz=%lld",
                    type.c_str(),
                    repeat,
                    points.size(),
                    numItems,
                    timer.toString(),
                    (long long)png->size()
                    );
        return response;
    }
    virtual String GetUrlPattern() {
        return urlPrefix+String("*");
    }

};

#endif /* CHARTREQUESTHANDLER_H */


/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Request Handler
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
#ifndef CHARTREQUESTHANDLER_H
#define CHARTREQUESTHANDLER_H
#include "RequestHandler.h"
#include "Logger.h"

#include "SimpleThread.h"
#include "StringHelper.h"
#include "FileHelper.h"
#include "Tiles.h"
#include "DrawingContext.h"
#include "Renderer.h"
#include "Coordinates.h"
#include "TokenHandler.h"





static const char* AVNAV_FORMAT="<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
 "<TileMapService version=\"1.0.0\" >"
   "<Title>%s</Title>"
   "<TileMaps>"
     "<TileMap" 
       " title=\"%s\"" 
       " href=\"http://%s:%d%s%s\""
       " minzoom=\"%d\""
       " maxzoom=\"%d\""
       " projection=\"EPSG:4326\">"
	     "<BoundingBox minlon=\"%f\" minlat=\"%f\" maxlon=\"%f\" maxlat=\"%f\" title=\"layer\"/>"
       "<TileFormat width=\"256\" height=\"256\" mime-type=\"x-png\" extension=\"png\" />"
    "</TileMap>"
   "</TileMaps>"
 "</TileMapService>";

static String DEFAULT_EULA="<html>"
    "<body>"
    "<p>No license file found</p>"
    "<p>Refer to <a href=\"https://o-charts.org/\">o-charts</a> for license info.</p>"
    "</body"
    "</html>";
class ChartRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/charts/";
    const String avnavXml="avnav.xml";
    const int tileBufferSize=Coord::TILE_SIZE*Coord::TILE_SIZE*4;
private:
    String urlPrefix;
    Renderer::Ptr renderer;
    Renderer::RenderInfo info;  
    TokenHandler::Ptr tokenHandler; 
    
    HTTPResponse *tryOpenFile(String base,String file,String mimeType){
        String fileName=FileHelper::concatPath(base,file);
        if (!FileHelper::exists(fileName)){
            return NULL;
        }
        LOG_DEBUG("open file %s",fileName);
        std::ifstream  *stream=new std::ifstream(fileName,std::ios::in|std::ios::binary);
        if (!stream->is_open() ){
            delete stream;
            return NULL;
        }
        HTTPResponse *rt=new HTTPStreamResponse(mimeType,stream,(unsigned long)FileHelper::fileSize(fileName));
        rt->responseHeaders["Access-Control-Allow-Origin"]="*";
        return rt;
    }

    HTTPResponse *handleEulaRequest(const String &chartSetKey,HTTPRequest *request){
        //Accept-Language: de-DE,de;q=0.9,en-US;q=0.8,en;q=0.7,es;q=0.6
        String languageHeader="en";
        auto it=request->header.find("accept-language");
        if (it != request->header.end()){
            languageHeader=it->second+","+languageHeader;
        }
        LOG_DEBUG("EULA request for %s, languages=%s",chartSetKey,languageHeader);
        HTTPResponse *rt=NULL;
        ChartSet::ConstPtr set=renderer->getManager()->GetChartSet(chartSetKey);
        if (! set){
            rt=new HTTPStringResponse("text/html",DEFAULT_EULA);
            rt->responseHeaders["Access-Control-Allow-Origin"]="*";
            return rt;
        }
        const ChartSetInfo *info=set->info.get();
        StringVector languages=StringHelper::split(languageHeader,",");
        for (auto it=languages.begin();it!=languages.end();it++){
            *it=StringHelper::toUpper(StringHelper::beforeFirst(*it,";",true))+"_";
            StringHelper::trimI(*it,' ');
            for(auto fit=info->eulaFiles.begin();fit!=info->eulaFiles.end();fit++){
                if (StringHelper::startsWith(*fit , *it)){
                    rt=tryOpenFile(info->dirname,*fit,"text/html");
                    if (rt != NULL){
                        LOG_DEBUG("found existing eula file %s for %s",*fit,*it);
                        return rt;
                    }
                }
            }
        }
        //did not find any eula file matching our languages, try any...
        for(auto fit=info->eulaFiles.begin();fit!=info->eulaFiles.end();fit++){
            rt=tryOpenFile(info->dirname,*fit,"text/html");
                    if (rt != NULL){
                        LOG_DEBUG("found existing eula file %s for any",*fit);
                        return rt;
                    }
        }
        //no eula file - use default
        rt=new HTTPStringResponse("text/html",DEFAULT_EULA);
        rt->responseHeaders["Access-Control-Allow-Origin"]="*";
        return rt;
    }
    
    HTTPResponse *handleSequenceRequest(String chartSetKey){
        json::JSON obj;
        JSONOK(obj);
        obj["sequence"]=renderer->getManager()->GetChartSetSequence(chartSetKey);
        HTTPResponse *rt=new HTTPJsonResponse(obj,false);
        rt->responseHeaders["Access-Control-Allow-Origin"]="*";
        return rt;
    }
    HTTPResponse *handleAvNavXmlRequest(String chartSetKey, HTTPRequest *request){
        ChartSet::Ptr set=renderer->getManager()->GetChartSet(chartSetKey);
        HTTPResponse *rt=nullptr;
        if (! set){
            LOG_ERROR("unknown chart set %s",chartSetKey);
            rt=new HTTPResponse();
            rt->responseHeaders["Access-Control-Allow-Origin"]="*";
            return rt;
        }
        ChartSet::ExtentInfo ext=set->GetExtent();
        Coord::LLBox llExt=Coord::LLBox::fromWorld(ext.extent);
        RenderSettings::ConstPtr settings=renderer->getManager()->GetS52Data()->getSettings();
        ZoomLevelScales scales(settings->scale);
        int minZoom=scales.FindZoomForScale(ext.maxScale);
        int maxZoom=scales.FindZoomForScale(ext.minScale);
        String name=StringHelper::safeHtmlString(set->info->name);
        String data=FMT(AVNAV_FORMAT,
            name,
            name,
            request->serverIp,
            request->serverPort,
            urlPrefix,
            chartSetKey,
            minZoom,
            maxZoom,
            llExt.w_lon,
            llExt.s_lat,
            llExt.e_lon,
            llExt.n_lat
        );
        rt=new HTTPStringResponse("application/xml",data);
        rt->responseHeaders["Access-Control-Allow-Origin"]="*";
        return rt;
    }

public:
    /**
     * create a request handler
     */
    ChartRequestHandler(Renderer::Ptr renderer,TokenHandler::Ptr tokenHandler, String pngType,const String &addPrefix=""){
        this->renderer=renderer;
        this->tokenHandler=tokenHandler;
        if (addPrefix.empty()){
            urlPrefix=URL_PREFIX;
        }
        else{
            urlPrefix=addPrefix+URL_PREFIX;
        }
        info.pngType=pngType;
        
    }
    static bool odSort(const ObjectDescription::Ptr &a, const ObjectDescription::Ptr &b){
        return (*a) < (*b);
    }
    HTTPResponse *handleListInfoRequest(const String &chartSetKey,HTTPRequest* request ){
        String chart=GetQueryValue(request,"chart");
        if (chart.empty()) return new HTTPJsonErrorResponse("missing parameter chart");
        ChartSet::ConstPtr cset=renderer->getManager()->GetChartSet(chartSetKey);
        if (! cset){
            return new HTTPJsonErrorResponse("chart set "+chartSetKey+" not found");
        }
        String chartBase=cset->info->dirname;
        chartBase=FileHelper::concatPath(chartBase, FileHelper::fileName(chart,true));
        if (! FileHelper::exists(chartBase,true)){
            return new HTTPJsonErrorResponse("base dir "+chartBase+" not found");
        }
        json::JSON files=json::Array();
        for (auto &fi : FileHelper::listDir(chartBase,"*.TXT",false)){
            files.append(fi);
        }
        for (auto &fi : FileHelper::listDir(chartBase,"*.txt",false)){
            files.append(fi);
        }
        return new HTTPJsonResponse(files,true);
    }
    HTTPResponse *handleInfoRequest(const String &chartSetKey,HTTPRequest* request ){
        String chart=GetQueryValue(request,"chart");
        if (chart.empty()) return new HTTPErrorResponse(400,"missing parameter chart");
        String fname=GetQueryValue(request,"txt");
        if (fname.empty()) return new HTTPErrorResponse(400,"missing parameter txt");
        ChartSet::ConstPtr cset=renderer->getManager()->GetChartSet(chartSetKey);
        if (! cset){
            return new HTTPErrorResponse(404,"chart set "+chartSetKey+" not found");
        }
        String chartBase=cset->info->dirname;
        chartBase=FileHelper::concatPath(chartBase, FileHelper::fileName(chart,true));
        if (! FileHelper::exists(chartBase,true)){
            return new HTTPErrorResponse(404,"base dir "+chartBase+" not found");
        }
        HTTPResponse *rt=tryOpenFile(chartBase,StringHelper::SanitizeString(fname),"text/plain");
        if (rt == nullptr){
            return new HTTPErrorResponse(404,"File "+fname+" not found in "+chartBase);
        }
        return rt;
    }
    HTTPResponse * handleFeatureInfoRequest(const String &urlBase,const String &chartUrl,const TileInfo &tile, const String &lat,const String &lon, const String &tolerance, bool overview=true){
        //compute a world box from the lat/lon/tolerance
        //basically tolerance should be dirtectly a pixel tolerance
        //but AvNav computes it from the longitude of 2 points
        //so we do the invers
        Coord::LLXy llcenter(::atof(lon.c_str()),::atof(lat.c_str()));
        Coord::LLXy tolpoint=llcenter.getShifted(::atof(tolerance.c_str()),0);
        Coord::WorldXy center=Coord::latLonToWorld(llcenter);
        Coord::WorldXy tolworld=Coord::latLonToWorld(tolpoint);
        Coord::World wtolerance=std::abs(tolworld.x-center.x);
        Coord::TileBox tileBox;
        tileBox.zoom=tile.zoom;
        tileBox.xmin=center.x-wtolerance;
        tileBox.ymin=center.y-wtolerance;
        tileBox.xmax=center.x+wtolerance;
        tileBox.ymax=center.y+wtolerance;
        LOG_DEBUG("featureInfo, tile=%s,center=%.8f,%.8f, box=%s",tile.ToString(),llcenter.x,llcenter.y,tileBox.toString());
        ObjectList objects;
        try{
            objects=renderer->featureInfo(tile, tileBox);
        }catch (AvException &e){
            return new HTTPJsonErrorResponse(e.msg());
        }
        std::sort(objects.begin(),objects.end(),odSort);
        json::JSON rt;
        struct OEqual{
            bool operator()(const ObjectDescription* const &p1,const ObjectDescription* const &p2) const{
                return *p1 == *p2;
            }
        };
        struct OHash{
            size_t operator()(const ObjectDescription* const &o) const{
                return o->md5.hash();
            }
        };
        std::unordered_set<const ObjectDescription*, OHash,OEqual> alreadyHandled;
        if (overview){
            bool hasObjects=false;
            ObjectDescription::Ptr first;
            Coord::WorldXy firstPoint;
            for (const auto &it : objects){
                if (it->type != ObjectDescription::T_OBJECT) continue;
                hasObjects=true;
                if (!it->isPoint()) continue;
                if (alreadyHandled.find(it.get()) != alreadyHandled.end()){
                    continue;
                }
                alreadyHandled.insert(it.get());
                if (!first){
                    first=it;
                    firstPoint=it->point;
                }
                else{
                    if (it->point != firstPoint) continue;
                }
                it->jsonOverview(rt);
            }
            if (hasObjects){
                String targetUrl=chartUrl+"?featureDetails=1&lat="+lat+"&lon="+lon+"&tolerance="+tolerance;
                rt["link"]=urlBase+"/static/finfo.html?data="+StringHelper::urlEncode(targetUrl);
                if (! rt.hasKey("name")){
                    rt["name"]="featureInfo";
                }
            }
        }
        else{
            json::JSON jobjects=json::Array();
            for (const auto &it : objects){
                if (alreadyHandled.find(it.get()) != alreadyHandled.end()){
                    continue;
                }
                alreadyHandled.insert(it.get());
                json::JSON jobject;
                it->toJson(jobject);
                jobjects.append(jobject);
            } 
            rt["features"]=jobjects;
        }
        alreadyHandled.clear();
        return new HTTPJsonResponse(rt);
    }
    
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
    
        long start = Logger::MicroSeconds100();
        String url = request->url.substr(urlPrefix.length());
        StringHelper::replaceInline(url,"//","/");
        if (StringHelper::startsWith(url,"/")){
            url=url.substr(1);
        }
        StringVector parts=StringHelper::split(url,"/",1);
        if (parts.size() != 2){
            LOG_DEBUG("missing chartSet in %s",url);
            return new HTTPResponse();
        }
        String chartSetKey=parts[0];
        url=parts[1];
        if (url[0] >= 'A') //fast check to avoid many compares for each tile
        {
            if (StringHelper::startsWith(url, "sequence"))
            {
                return handleSequenceRequest(chartSetKey);
            }
            if (StringHelper::startsWith(url, avnavXml))
            {
                return handleAvNavXmlRequest(chartSetKey, request);
            }
            if (StringHelper::startsWith(url, "eula"))
            {
                return handleEulaRequest(chartSetKey, request);
            }
            if (StringHelper::startsWith(url, "info"))
            {
                return handleInfoRequest(chartSetKey, request);
            }
            if (StringHelper::startsWith(url, "listInfo"))
            {
                return handleListInfoRequest(chartSetKey, request);
            }
        }
        String fi=GetQueryValue(request,"featureInfo");
        String fdetails=GetQueryValue(request,"featureDetails");
        bool isFeatureInfo=(!fi.empty() || !fdetails.empty());
        String chartUrl=url;
        if (fdetails.empty()){
            if (StringHelper::startsWith(url,"encrypted/")){
                size_t p=url.find('/');
                if (p == String::npos || p >= (url.size()-1) ){
                    return new HTTPErrorResponse(400,"malformed crypted url");
                }
                p++;
                size_t e=url.find('?');
                if (e == String::npos) e=url.length();
                e-=p;
                String encrypted=url.substr(p,e);
                DecryptResult res=tokenHandler->DecryptUrl(encrypted);
                if (res.url.empty()){
                    return new HTTPErrorResponse(500,"unable to decrypt url "+encrypted+", error: "+res.error);
                }
                chartUrl=res.url;
            }
            else{
                return new HTTPErrorResponse(400,"unencryped url "+url);
            }
        }
        TileInfo tile(chartUrl, chartSetKey);
        if (!tile.valid) {
            LOG_DEBUG("invalid url %s", chartUrl);
            return new HTTPErrorResponse(400,"invalid chart url "+chartUrl);
        }
        if (isFeatureInfo){
            //feature info request
            String lat=GetQueryValue(request,"lat");
            if (lat.empty()) return new HTTPJsonErrorResponse("missing parameter lat");
            String lon=GetQueryValue(request,"lon");
            if (lon.empty()) return new HTTPJsonErrorResponse("missing parameter lon");
            String tolerance=GetQueryValue(request,"tolerance");
            if (tolerance.empty()) return new HTTPJsonErrorResponse("missing parameter tolerance");
            String host=StringHelper::trim(request->header["host"],' ');
            String urlBase="http://"+host;
            HTTPResponse *response=nullptr;
            if (request->method != "GET"){
                response=new HTTPJsonOk();
            }
            else{
                response=handleFeatureInfoRequest(urlBase,urlPrefix+chartSetKey+"/"+chartUrl,tile,lat,lon,tolerance,fdetails.empty());
            }
            response->responseHeaders["Access-Control-Allow-Origin"]="*";
            return response;
        }

        Renderer::RenderResult result;
        try{
            renderer->renderTile(tile,info,result);
        }catch (Exception &e){
            LOG_DEBUG("render exception: %s",e.what());
            return new HTTPErrorResponse(404,String("Render error: ")+e.what());
        }
        DataPtr png=result.getResult();
        HTTPResponse *response = new HTTPDataResponse("image/png", png);
        LOG_DEBUG("http render: %s %s, sz=%lld",
                    tile.ToString(true),
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


/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Rdnerer
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
#include "stdlib.h"
#include "Renderer.h"
#include "RenderHelper.h"
#include "PngHandler.h"
#include "Logger.h"
#include "Coordinates.h"

class ChartIdx{
    public:
    Chart::ConstPtr chart;
    int idx;
    bool valid=false;
    ChartIdx(Chart::ConstPtr c, int i)
        :chart(c),idx(i),valid(true){}
    ChartIdx(){}
    class List: public std::vector<ChartIdx>{
        int maxPasses=0;
        public:
        int getMaxPasses(){return maxPasses;}
        using std::vector<ChartIdx>::vector;
        bool addIfSame(const ChartIdx &next){
            if (size() < 1 || back().chart->GetNativeScale() == next.chart->GetNativeScale()){
                push_back(next);
                int p=next.chart->getRenderPasses();
                if (p > maxPasses) maxPasses=p;
                return true;
            }
            return false;
        }
    };
};
void Renderer::renderTile(const TileInfo &tile, const RenderInfo &info, RenderResult &result)
{
    std::unique_ptr<PngEncoder> encoder(PngEncoder::createEncoder(info.pngType));
    if (!encoder)
    {
        throw RenderException(tile, FMT("no encoder for %s", info.pngType));
    }
    //the render context has TileBounds with the min being relative
    //to the xmin/ymin of the tileBox translated to pixels by zoom level
    //so a world coordinate of tileExtent.xmin/tileExtent.ymin will translate to 0/0
    //see worldToPixel in TileExtent
    RenderContext context;
    context.s52Data=chartManager->GetS52Data();
    RenderSettings::ConstPtr renderSettings=context.s52Data->getSettings();
    result.timer.add("settings");
    ChartManager::ExtentList extents=chartManager->GetChartSetExtents(tile.chartSetKey,true);
    if (extents.size() < 1){
        throw RenderException(tile,"internal error: no chart set extent");
    }
    Coord::TileBox tileBox=Coord::tileToBox(tile);
    WeightedChartList renderCharts = chartManager->FindChartsForTile(renderSettings, tile);
    result.timer.add("find");
    if (renderCharts.size() < 1)
    {
        if (! tileBox.intersects(extents[0])){
            throw NoChartsException(tile, "no charts to render");
        }
    }
    std::unique_ptr<DrawingContext> drawing(DrawingContext::create(Coord::TILE_SIZE, Coord::TILE_SIZE));
    ZoomLevelScales scales(renderSettings->scale);
    context.scale = scales.GetScaleForZoom(tile.zoom);
    bool hasRendered = false;
    int idx=0;
    Timer::SteadyTimePoint startRender=result.timer.current();
    std::map<int,Chart::ConstPtr> openCharts;
    if (chartManager->HasOpeners())
    {
        for (auto it = renderCharts.begin(); it != renderCharts.end(); it++, idx++)
        {
            Chart::ConstPtr chart = chartManager->OpenChart(context.s52Data, it->info,false);
            if (chart)
            {
                openCharts[idx] = chart;
                result.timer.add("open1st", idx);
            }
        }
    }
    std::vector<ChartRenderContext::Ptr> chartContexts(renderCharts.size(),ChartRenderContext::Ptr());    
    idx=0;
    ChartIdx currentChart;
    while(idx < renderCharts.size() || currentChart.valid)
    {
        try
        {
            //build a vector of all charts of the same scale
            //and render all passes for them together
            int maxSteps=0;
            ChartIdx::List scaleCharts;
            if (currentChart.valid){
                //must always succeed
                scaleCharts.addIfSame(currentChart);
                currentChart.valid=false;
            }
            if (idx < renderCharts.size())
            {
                bool added = false;
                do
                {
                    int currentIdx=idx;
                    idx++; //go to the next chart in case of errors
                    Chart::ConstPtr chart;
                    auto cit = openCharts.find(currentIdx);
                    if (cit != openCharts.end())
                    {
                        chart = cit->second;
                    }
                    else
                    {
                        try{
                            chart = chartManager->OpenChart(context.s52Data, renderCharts[currentIdx].info);
                            result.timer.add("open", currentIdx);
                        }
                        catch(RecurringException &e){
                            LOG_DEBUG("%s:%s",tile.ToString(),e.msg());
                        }
                        catch(AvException &e){
                            LOG_ERROR("%s:%s",tile.ToString(),e.msg());
                        }
                    }
                    if (chart){
                        currentChart = ChartIdx(chart, currentIdx);
                        added = scaleCharts.addIfSame(currentChart);
                        if (added)
                        {
                            currentChart.valid = false;
                        }
                    }
                } while (added && idx < renderCharts.size());
            }
            for (int pass = 0; pass < scaleCharts.getMaxPasses(); pass++)
            {
                for (auto scaleChart = scaleCharts.begin(); scaleChart != scaleCharts.end(); scaleChart++)
                {
                    context.chartContext = chartContexts[scaleChart->idx];
                    scaleChart->chart->Render(pass, context, *drawing,  renderCharts[scaleChart->idx].tile);
                    hasRendered = true;
                    chartContexts[scaleChart->idx] = context.chartContext;
                    context.chartContext.reset();
                }
            }
            result.timer.add("draw");
        }
        catch (AvException &r)
        {
            LOG_ERROR("%s:%s",tile.ToString(), r.msg());
        }
        catch (Exception &e)
        {
            LOG_ERROR("%s: %s", tile.ToString(), e.what());
        }
        context.textBoxes.clear(); //declutter only per chart???
    }
    result.timer.set(startRender,"render");
    chartContexts.clear(); //we must ensure to release all contexts before we release the charts
    renderCharts.clear();
    DrawingContext::ColorAndAlpha boundingColor = context.s52Data->convertColor(context.s52Data->getColor("XACBND"));
    if (renderSettings->showChartBounds){
        for (const auto &extent: extents){
            Coord::PixelBox pixelExtent=Coord::worldExtentToPixel(extent, tileBox);
            drawing->drawHLine(pixelExtent.ymin,pixelExtent.xmin,pixelExtent.xmax,boundingColor);
            drawing->drawHLine(pixelExtent.ymax,pixelExtent.xmin,pixelExtent.xmax,boundingColor);
            drawing->drawVLine(pixelExtent.xmin,pixelExtent.ymin,pixelExtent.ymax,boundingColor);
            drawing->drawVLine(pixelExtent.xmax,pixelExtent.ymin,pixelExtent.ymax,boundingColor);
        }
    }
    if (renderDebug)
    {
        DrawingContext::ColorAndAlpha c = DrawingContext::convertColor(255, 0, 0);
        drawing->drawHLine(0, 0, Coord::TILE_SIZE - 1, c);
        drawing->drawHLine(Coord::TILE_SIZE - 1, 0, Coord::TILE_SIZE - 1, c);
        drawing->drawVLine(0, 0, Coord::TILE_SIZE - 1, c);
        drawing->drawVLine(Coord::TILE_SIZE - 1, 0, Coord::TILE_SIZE - 1, c);
        DrawingContext *ctx = drawing.get();
        FontManager::Ptr fontManager=context.s52Data->getFontManager(s52::S52Data::FONT_TXT);
        RenderHelper::drawText(fontManager,*ctx, FMT("%d/%d/%d", tile.zoom, tile.x, tile.y), Coord::PixelXy(20, Coord::TILE_SIZE - 3), c);
    }
    encoder->setContext(drawing.get());
    bool rt = encoder->encode(result.result);
    result.timer.add("png");
    LOG_DEBUG("%s",drawing->getStatistics());
}

ObjectList Renderer::featureInfo( const TileInfo &info, const Coord::TileBox &box, bool overview){
    //compute a pixel box from the tile box
    Coord::TileBounds pixelExtent=box.getPixelBounds();
    s52::S52Data::ConstPtr s52data=chartManager->GetS52Data();
    LOG_DEBUG("featureInfo: %s , %s",info.ToString(),box.toString());
    ObjectList rt;
    WeightedChartList renderCharts = chartManager->FindChartsForTile(s52data->getSettings(), info,true);
    if (renderCharts.size() < 1){
        return rt;
    }
    LOG_DEBUG("featureInfo found %d charts",renderCharts.size());
    RenderContext context(pixelExtent);
    ZoomLevelScales scales(s52data->getSettings()->scale);
    context.scale = scales.GetScaleForZoom(info.zoom);
    context.s52Data=s52data;
    std::unique_ptr<DrawingContext> drawing(DrawingContext::create(context.tileExtent.xmax,context.tileExtent.ymax));
    drawing->setCheckOnly(true);
    //for feature info we start with the smallest scale charts
    //they are at the end - so we iterate backwards
    for (auto it=renderCharts.rbegin();it != renderCharts.rend();it++){
        Chart::ConstPtr chart;
        try{
            chart=chartManager->OpenChart(s52data,it->info);
        }catch (AvException &e){
            LOG_ERROR("featureInfo: Unable to open chart %s:%s",it->info->GetFileName(),e.msg());
        }
        if (chart){
            drawing->resetDrawn();
            ObjectList objects=chart->FeatureInfo(context,*drawing,box,overview);
            if (objects.size() > 0){
                rt.insert(rt.end(),objects.begin(),objects.end());
            }
        }
    }
    return rt;
}
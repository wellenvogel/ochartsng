/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Render functions
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
#include "RenderHelper.h"
#include <math.h>

template <typename T>
static Coord::PixelXy getPivot(const T &t){return Coord::PixelXy(t->pivot_x,t->pivot_y);}
RenderHelper::StrSounding RenderHelper::valToSounding(float v, s52::S52Data::ConstPtr s52data){
    StrSounding rt;
    if (v < 0) return rt;
    if (v > 40000) v=99999.0;
    v=s52data->convertSounding(v,0);
    //if we show feet - always not decimals
    bool inFeet=s52data->getSettings()->S52_DEPTH_UNIT_SHOW == s52::DepthUnit::FEET;
    float decimal;
    if (!inFeet) decimal=floor(v);
    else decimal=round(v);
    rt.decimal = std::to_string((int)decimal);
    if (!inFeet && v < 31)
    {
        float fract = v - decimal;
        int fractInt = (int)::round(fract * 10.0);
        if (fractInt != 0)
        {
            rt.fract = std::to_string(fractInt);
        }
    }
    return rt;
};

void RenderHelper::renderArc(s52::S52Data::ConstPtr s52Data, DrawingContext &ctx, const s52::Arc &arc, Coord::PixelXy point)
{
    Coord::Pixel arcInner = arc.arcRadius;
    Coord::Pixel arcOuter = arc.arcRadius;
    if (arc.outlineWidth >= 2)
    {
            arcInner = arc.arcRadius - arc.outlineWidth / 2;
            arcOuter = arcInner + arc.outlineWidth;
            ctx.drawArc(point, arc.cOutline, arcOuter, arcInner, arc.sectr1, arc.sectr2);
    }
    else
    {
            ctx.drawArc(point, arc.cOutline, arcOuter, -1, arc.sectr1, arc.sectr2);
    }
    if (arc.arcWidth > 0)
    {
        if (arc.arcWidth >= 2)
        {
            arcInner = arc.arcRadius - arc.arcWidth / 2;
            arcOuter = arcInner + arc.arcWidth ;
            ctx.drawArc(point, arc.cArc, arcOuter, arcInner, arc.sectr1, arc.sectr2);
        }
        else
        {
            ctx.drawArc(point, arc.cArc, arc.arcRadius, -1, arc.sectr1, arc.sectr2);
        }
    }
    if (arc.sectorRadius > 0)
    {
        DrawingContext::ColorAndAlpha color = s52Data->convertColor(
            s52Data->getColor("CHBLK"));
        DrawingContext::Dash dash;
        dash.draw = 8;
        dash.gap = 2;
        float a = (arc.sectr1 - 90) * M_PI / 180;
        Coord::PixelXy dst(point.x + (int)(arc.sectorRadius * cosf(a)),
                           point.y + (int)(arc.sectorRadius * sinf(a)));
        ctx.drawAaLine(point, dst, color,&dash);
        a = (arc.sectr2 - 90) * M_PI / 180.;
        dst.x = point.x + (int)(arc.sectorRadius * cosf(a));
        dst.y = point.y + (int)(arc.sectorRadius * sinf(a));
        ctx.drawAaLine(point, dst, color,&dash);
    }
}

void RenderHelper::renderLine(DrawingContext &ctx,DrawingContext::ColorAndAlpha c,
        const Coord::PixelXy &start, const Coord::PixelXy &end, int width, DrawingContext::Dash *dash){
            if (width <= 1){
                ctx.drawLine(start,end,c,false,dash);
            }
            else{
                ctx.drawThickLine(start,end,c,false,dash,width);
            }
        }
void RenderHelper::renderLine(DrawingContext &ctx,Coord::TileBox const &tile,DrawingContext::ColorAndAlpha c,
    const Coord::WorldXy &start, const Coord::WorldXy &end, int width, DrawingContext::Dash *dash){
        Coord::PixelXy pstart=tile.worldToPixel(start);
        Coord::PixelXy pend=tile.worldToPixel(end);
        renderLine(ctx,c,pstart,pend,width,dash);
    }
/**
 * symbol drawing on a line:
 * (1) the symbol has a pivot - that must be substracted from the "nominal" drawing position
 * (2) the symbol has width/height - we must ensure that the real drawn symbol (width/height) will not 
 *     extend befor the start / after the end of the line
 *     we always just consider the major axis (LineProgressX: consider width, LineProgressY: consider height)
 * (3) additionally we check that we have enough distance to the last real drawing position (lastDraw)
*/
class LineProgress{
    public:
    float m=0;
    Coord::Pixel delta=0;
    Coord::PixelXy start;
    Coord::PixelXy end;
    Coord::PixelXy current;
    LineProgress(Coord::PixelXy cstart, Coord::PixelXy cend, float cm,Coord::Pixel cdelta):
        m(cm),delta(cdelta),start(cstart),end(cend){
            current=start;
        }
    virtual bool fits()=0;
    virtual bool step()=0;
    virtual bool endReached()=0;
    bool cmpe(Coord::Pixel cur, Coord::Pixel cmp) const
    {
        if (delta == 0)
            return false;
        if (delta > 0)
        {
            return cur <= cmp;
        }
        else
        {
            return cur >= cmp;
        }
    }
    virtual ~LineProgress(){}
};
class ProgressX: public LineProgress{
    public:
    using LineProgress::LineProgress;
    virtual bool fits(){
        return cmpe(current.x+delta,end.x);
    }
    virtual bool endReached(){
        return !cmpe(current.x,end.x);
    }
    virtual bool step(){
        current.x+=delta;
        current.y=start.y+m*(current.x-start.x);
        return endReached();
    }
};
class ProgressY: public LineProgress{
    public:
    using LineProgress::LineProgress;
    virtual bool endReached(){
        return !cmpe(current.y,end.y);
    }
    virtual bool step(){
        current.y+=delta;
        current.x=start.x+m*(current.y-start.y);
        return endReached();
    }
    virtual bool fits(){
        return cmpe(current.y+delta,end.y);
    }
};

static LineProgress * createProgress(const Coord::PixelXy &start, const Coord::PixelXy &end, int width, int height){
    Coord::Pixel dx=end.x-start.x;
    Coord::Pixel dy=end.y-start.y;
    Coord::Pixel adx=dx>=0?dx:-dx;
    Coord::Pixel ady=dy>=0?dy:-dy;
    if (ady > adx){
        float f = ady>0?((float)dx / float(dy)):0;
        return new ProgressY(start,end,f,(dy>0)?height:-height);
    }
    else{
        float f = adx>0?((float)dy / float(dx)):0;
        return new ProgressX(start,end,f,(dx>0)?width:-width);
    }
}

void RenderHelper::renderSymbolLine(s52::S52Data::ConstPtr s52Data,DrawingContext &ctx,Coord::TileBox const &tile,
    const Coord::WorldXy &start, const Coord::WorldXy &end, 
    const String &symbol){
        renderSymbolLine(s52Data,ctx,
            tile.worldToPixel(start),
            tile.worldToPixel(end),
            symbol
            );
    }

void RenderHelper::renderSymbolLine(s52::S52Data::ConstPtr s52Data,DrawingContext &ctx,
    const Coord::PixelXy &pstart, const Coord::PixelXy &pend, 
    const String &symbolName){
        if (symbolName.empty()) return;
        Coord::Pixel dx=pend.x-pstart.x;
        Coord::Pixel dy=pend.y-pstart.y;
        int rotation=0;
        double rotrad=atan2((double)dy,(double)dx);
        rotation=round(rotrad/M_PI*180.0);
        //the rotation computed here has 0° when going east (dy==0,dx>0)
        //so to be inline with symbol rotations (0° == north up) we add 90°
        rotation+=90;
        s52::SymbolPtr symbol=s52Data->getSymbol(symbolName,rotation);
        if (! symbol) return;
        Coord::Pixel dw=symbol->width;
        Coord::Pixel dh=symbol->height;
        Coord::PixelXy pivoti=getPivot(symbol).getInverted();
        std::unique_ptr<LineProgress> progress(createProgress(pstart.getShifted(pivoti),pend.getShifted(pivoti),dw,dh));
        DrawingContext::ColorAndAlpha defaultColor=symbol->defaultColor;
        if (! progress->fits()){
            ctx.drawLine(pstart,pend,defaultColor);
        }
        else{
            while (true){
                ctx.drawSymbol(progress->current,symbol->width,symbol->height,symbol->buffer->data());
                progress->step();
                if (! progress->fits()) break;
            }
            if (! progress->endReached()){
                ctx.drawLine(progress->current.getShifted(pivoti.getInverted()),pend,defaultColor);
            }
        }
    }

DrawingContext::PatternSpec *RenderHelper::createPatternSpec(s52::SymbolPtr symbol, const Coord::TileBox &tile)
    {
        if (!symbol)
            return nullptr;
        DrawingContext::PatternSpec *pattern = new DrawingContext::PatternSpec(
            symbol->buffer->data(),
            symbol->width,
            symbol->height);
        pattern->distance = symbol->minDist;
        pattern->stagger = symbol->stagger;
        Coord::World xraster = pattern->width + pattern->distance;
        Coord::World yraster = pattern->height + pattern->distance;
        Coord::World xoffset = Coord::worldToPixel(tile.xmin,tile.zoom) % xraster;
        Coord::World yoffset = Coord::worldToPixel(tile.ymin,tile.zoom) % yraster;
        pattern->xoffset = xoffset;
        pattern->yoffset = yoffset;
        return pattern;
    }

    Coord::Pixel RenderHelper::drawText(FontManager::Ptr fontManager, DrawingContext &ctx, const String &str, const Coord::PixelXy &point, const DrawingContext::ColorAndAlpha &color)
    {
        Coord::PixelXy pp(point);
        std::u32string display = StringHelper::toUnicode(str);
        for (auto si = display.begin(); si != display.end(); si++)
        {
            FontManager::Glyph::ConstPtr glyph = fontManager->getGlyph(*si);
            Coord::PixelXy dp = pp;
            dp.x += glyph->pivotX;
            dp.y += glyph->pivotY;
            ctx.drawGlyph(dp, glyph->width, glyph->height, glyph->buffer, color);
            pp.x += glyph->advanceX;
            if (si < (display.end() - 1))
            {
                pp.x += fontManager->getKernX(*si, *(si + 1));
            }
        }
        return pp.x - point.x;
    }
    Coord::PixelBox RenderHelper::drawText(FontManager::Ptr fontManager, DrawingContext &ctx, const s52::DisplayString &str, const Coord::PixelXy &point)
    {
        Coord::PixelBox rt = str.relativeExtent.getShifted(point);
        Coord::PixelXy pp(point);
        pp.shift(str.pivotX, str.pivotY);
        std::u32string display = StringHelper::toUnicode(str.value);
        for (auto si = display.begin(); si != display.end(); si++)
        {
            FontManager::Glyph::ConstPtr glyph = fontManager->getGlyph(*si);
            Coord::PixelXy dp = pp;
            dp.x += glyph->pivotX;
            dp.y += glyph->pivotY;
            ctx.drawGlyph(dp, glyph->width, glyph->height, glyph->buffer, str.color);
            pp.x += glyph->advanceX;
            if (si < (display.end() - 1))
            {
                pp.x += fontManager->getKernX(*si, *(si + 1));
            }
        }
        return rt;
    }
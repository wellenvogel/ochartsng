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
#ifndef _RENDERHELPER_H
#define _RENDERHELPER_H
#include "Types.h"
#include "DrawingContext.h"
#include "S52Data.h"
class RenderHelper
{
public:
    class StrSounding
    {
    public:
        String decimal;
        String fract;
        bool negative=0;
    };
    
    static StrSounding valToSounding(float v, s52::S52Data::ConstPtr s52data);
    static void renderArc(s52::S52Data::ConstPtr s52Data, DrawingContext &ctx, const s52::Arc &arc, Coord::PixelXy point);
    static void renderLine(DrawingContext &ctx,Coord::TileBox const &tile,DrawingContext::ColorAndAlpha c,
        const Coord::WorldXy &start, const Coord::WorldXy &end, int width, DrawingContext::Dash *dash);
    static void renderLine(DrawingContext &ctx,DrawingContext::ColorAndAlpha c,
        const Coord::PixelXy &start, const Coord::PixelXy &end, int width, DrawingContext::Dash *dash);    
    static void renderSymbolLine(s52::S52Data::ConstPtr s52Data,DrawingContext &ctx,Coord::TileBox const &tile,
        const Coord::WorldXy &start, const Coord::WorldXy &end, 
        const String &symbol); 
    static void renderSymbolLine(s52::S52Data::ConstPtr s52Data,DrawingContext &ctx,
        const Coord::PixelXy &start, const Coord::PixelXy &end, 
        const String &symbol);
    static DrawingContext::PatternSpec *createPatternSpec(s52::SymbolPtr symbol,const Coord::TileBox &tile);       
    template <typename LType>
    static void renderSounding(s52::S52Data::ConstPtr s52Data, DrawingContext &ctx, Coord::TileBox const &tile, const LType *soundings)
    {
        // for now simplified approach:
        // just render a text
        // still misses special symbols!
        FontManager::Ptr fm = s52Data->getFontManager(s52::S52Data::FONT_SOUND);
        DrawingContext::ColorAndAlpha cDeep = s52Data->convertColor(s52Data->getColor("SNDG1"));
        DrawingContext::ColorAndAlpha cShallow = s52Data->convertColor(s52Data->getColor("SNDG2"));
        double safetyDepth = s52Data->getSettings()->S52_MAR_SAFETY_CONTOUR;
        int asc = fm->getAscend();
        int cwidth = fm->getCharWidth('0');
        int shift_y = +asc / 2; // text is above point
        int shift_fract = asc / 2;
        for (auto it = soundings->begin(); it != soundings->end(); it++)
        {
            StrSounding st = valToSounding(it->depth, s52Data);
            size_t dlen = st.decimal.size();
            if (dlen < 1)
            {
                continue;
            }
            size_t flen = st.fract.empty() ? 0 : 1;
            int width = cwidth * (dlen); // TODO: really compute size?
            int shift_x = -width + cwidth / 4;
            Coord::PixelXy dp = tile.worldToPixel(*it);
            dp.shift(shift_x, shift_y);
            Coord::Pixel x = drawText(fm, ctx,
                                                st.decimal,
                                                dp,
                                                it->depth >= safetyDepth ? cDeep : cShallow);
            if (st.negative){
                //draw underscore
                Coord::PixelXy up=dp.getShifted(x-cwidth,asc/7);
                drawText(fm,ctx,"_",up,it->depth >= safetyDepth ? cDeep : cShallow);
            }
            if (flen)
            {
                dp.shift(x, shift_fract);
                drawText(fm,ctx,
                                  st.fract,
                                  dp,
                                  it->depth >= safetyDepth ? cDeep : cShallow);
            }
        }
    }
    static Coord::PixelBox drawText(FontManager::Ptr fm, DrawingContext &ctx,const s52::DisplayString &str, const Coord::PixelXy &point) ;
    static Coord::Pixel drawText(FontManager::Ptr fm,DrawingContext &ctx,const String &str, const Coord::PixelXy &point, const DrawingContext::ColorAndAlpha &color);
};
#endif
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test TestRenderer
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
#include "PngHandler.h"
#include "Logger.h"
#include "Coordinates.h"


static DrawingContext::ColorAndAlpha getColor(){
    return DrawingContext::convertColor(random()%256,random()%256,random()%256);
}

static void renderPlain(DrawingContext *ctx,const TileInfo &tile, FontManager::Ptr fontManager){
    int boxc = 64;
    LOG_DEBUG("rect count=%d", boxc * boxc);
    int sz = ((Coord::TILE_SIZE - 20) << 16) / boxc;
    /*for (int x = 0; x < boxc; x++)
    {
        for (int y = 0; y < boxc; y++)
        {
            ctx->drawRect(10 + ((x * sz) >> 16), 10 + ((y * sz) >> 16), 10 + (((x + 1) * sz) >> 16) - 1, 10 + (((y + 1) * sz) >> 16) - 1, getColor());
        }
    }*/
    Coord::PixelXy xy({20,20});
    String t=FMT("TÄIlgj %d/%d/%d",tile.zoom,tile.y,tile.x);
    std::u32string unicode=StringHelper::toUnicode(t);
    DrawingContext::ColorAndAlpha r=DrawingContext::convertColor(0,0,0);
    for (auto it=unicode.begin();it != unicode.end();it++){
        FontManager::Glyph::ConstPtr g=fontManager->getGlyph(*it);
        Coord::PixelXy dp=xy;
        dp.x+=g->pivotX;
        dp.y+=g->pivotY;
        ctx->drawGlyph(dp,g->width,g->height,g->buffer,r);
        xy.x+=g->advanceX;
        if ((it+1) != unicode.end()){
            xy.x+=fontManager->getKernX(*it,*(it+1));
        }
    }
}



void TestRenderer::renderTile(const TileInfo &tile, const RenderInfo &info, RenderResult &result)
{
    PngEncoder *encoder = PngEncoder::createEncoder(info.pngType);
    if (!encoder)
    {
        throw RenderException(tile, StringHelper::format("no encoder for %s", info.pngType.c_str()));
        return;
    }
    std::unique_ptr<DrawingContext> drawing(DrawingContext::create(Coord::TILE_SIZE, Coord::TILE_SIZE));
    renderPlain(drawing.get(), tile,chartManager->GetS52Data()->getFontManager(s52::S52Data::FONT_TXT));
    result.timer.add("draw");
    encoder->setContext(drawing.get());
    bool rt = encoder->encode(result.result);
    result.timer.add("png");
    delete encoder;
}

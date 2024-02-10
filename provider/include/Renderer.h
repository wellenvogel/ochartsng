/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Renderer
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

#ifndef _RENDERERH
#define _RENDERERH
#include "Types.h"
#include "Tiles.h"
#include "DrawingContext.h"
#include "Logger.h"
#include "ChartManager.h"
#include "Timer.h"
#include "S52Data.h"
#include "FontManager.h"
#include <memory>
#include "TileCache.h"
class Renderer
{
public:
    typedef std::shared_ptr<Renderer> Ptr;
    typedef std::shared_ptr<const Renderer> ConstPtr;

    DECL_EXC(RenderException,NoChartsException);

    class RenderResult
    {
        public:
        DataPtr result;
        Timer::Measure timer;
        RenderResult(){
            result=std::make_shared<DataVector>();
        }
        DataPtr getResult(){
            return result;
        }
        ~RenderResult(){
        }
    };
    class RenderInfo{
        public:
        String pngType="fpng";
    };
    Renderer(ChartManager::Ptr m,TileCache::Ptr tc, bool debug){
        chartManager=m;
        renderDebug=debug;
        cache=tc;
    }
    virtual ~Renderer(){}
    virtual void renderTile(const TileInfo &tile,const RenderInfo &info,RenderResult &result);
    virtual ObjectList featureInfo(const TileInfo &info, const Coord::TileBox &box, bool overview);
    virtual ChartManager::Ptr getManager(){return chartManager;}
    protected:
        ChartManager::Ptr chartManager;
        TileCache::Ptr cache;
        bool renderDebug=false;

};
class TestRenderer : public Renderer{
    using Renderer::Renderer;
    virtual void renderTile(const TileInfo &tile,const RenderInfo &info,RenderResult &result);
};
#endif

/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Render Context
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
#ifndef _RENDERCONTEXT_H
#define _RENDERCONTEXT_H
#include "S52Data.h"
#include "Coordinates.h"
#include "memory"

class ChartRenderContext{
    public:
    virtual ~ChartRenderContext(){}
    using Ptr=std::shared_ptr<ChartRenderContext>;
};
class RenderContext
{
public:
    int scale;
    s52::S52Data::ConstPtr s52Data;
    std::vector<Coord::PixelBox> textBoxes;
    const Coord::TileBounds tileExtent;
    ChartRenderContext::Ptr chartContext;
    Coord::Pixel boundary=50; //pixels that we add to an extent to check which charts/objects we need to render 
    RenderContext(){}
    RenderContext(const Coord::TileBounds &tb):tileExtent(tb){}
};

#endif
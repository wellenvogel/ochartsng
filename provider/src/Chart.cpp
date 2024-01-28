/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2010 by Andreas Vogel   *
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
#include "Chart.h"
#include "FileHelper.h"
#include <algorithm>
#include <cmath>
#include "json.hpp"
#include "S57AttributeIds.h"


int Chart::GetNativeScale() const { 
    return nativeScale; 
}
Coord::Extent Chart::GetChartExtent() const { 
    return extent; 
}
Chart::RenderResult Chart::Render(int pass,RenderContext & context,DrawingContext &out, const Coord::TileBox &box) const
{
    return RFAIL;
};
ObjectList Chart::FeatureInfo(RenderContext & context, DrawingContext &drawing, const Coord::TileBox &box) const
{
    return ObjectList();
};

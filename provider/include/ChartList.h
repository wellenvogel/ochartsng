/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Info
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
#ifndef _CHARTLIST_H
#define _CHARTLIST_H

#include "Tiles.h"
#include "ChartInfo.h"
#include "ItemStatus.h"
#include <memory>


class ChartList : public ItemStatus{
    Coord::Extent       boundings;
public:
    typedef std::vector<ChartInfo::Ptr> InfoList;
    typedef std::shared_ptr<ChartList> Ptr;
    class ChartCounts{
        public:
        int valid=0;
        int ignored=0;
    } ;
    ChartList();
    ~ChartList();
    bool                AddChart(ChartInfo::Ptr chart);
    WeightedChartList   FindChartForTile(const Coord::Extent &tileExtent);
    int                 GetSize() const{return chartList.size();}
    Coord::Extent       GetBoundings() const {return boundings;}
    int                 GetMinScale() const {return minScale;}
    int                 GetMaxScale() const {return maxScale;}
    virtual void        ToJson(StatusStream &stream);
    ChartCounts           NumValidCharts();
    ChartInfo::Ptr      FindInfo(const String &fileName);
    StringVector        GetFailedChartNames(int maxErrors=-1);
    MD5Name             GetHash() const {return hash;}
    int                 RemoveUnverified();
    size_t              NumCharts() const { return chartList.size();}
    void                FillChartExtents(std::vector<Coord::Extent> &extents) const; 
private:
    void                computeHash();
    InfoList            chartList;
    int                 minScale=-1;
    int                 maxScale=-1;
    MD5Name             hash;
    
};

#endif
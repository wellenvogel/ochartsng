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
#ifndef _CHARTINFO_H
#define _CHARTINFO_H
#include <vector>
#include "Tiles.h"
#include "Types.h"
#include "Chart.h"
#include "DrawingContext.h"
#include "Timer.h"
#include <map>
#include <memory>
#include "SimpleThread.h"
#include "MD5.h"
#include "Coordinates.h"

/**
 * definition of the max. zoom level we compute or scales for
 */
#define MAX_ZOOM Coord::COORD_ZOOM_LEVEL

class ZoomLevelScales{
private:
    double zoomMpp[MAX_ZOOM+1];
    double zoomScales[MAX_ZOOM+1];
    const double BASE_MPP=0.264583333 / 1000; //meters/pixel for 96dpi
    //the factor here affects which details we will see at which zoom levels
        
public:
    typedef std::shared_ptr<ZoomLevelScales> Ptr;
    /**
     * set up our list of scales
     * @param scaleLevel - factor to multiply with BASE_MPP
     */
    ZoomLevelScales(double scaleLevel);
    double GetScaleForZoom(int zoom) const;
    double GetMppForZoom(int zoom) const;
    int FindZoomForScale(double scale) const;
};



class ChartInfo{
public:
    typedef std::shared_ptr<ChartInfo> Ptr;
    typedef enum{
        UNKNOWN,
        READY,
        NEEDS_VER //need verification as data only from cache
    } State;
    //create from loaded chart, compute fileHash, state=READY
    ChartInfo(const Chart::ChartType &type, const String &fileName, int nativeScale,Coord::Extent extent, bool ignore=false);
    //create from cache, state=NEEDS_VER
    ChartInfo(const Chart::ChartType &type,const String &fileName, int nativeScale,Coord::Extent extent,int64_t fileSize, int64_t fileTime, bool ignore=false);
    //unable to read chart header
    ChartInfo(const Chart::ChartType &type,const String &fileName);
    ~ChartInfo();
    bool        VerifyChartFileName(const String &fileName);
    String      ToString() const;
    int         GetNativeScale()const {return nativeScale;}
    /**
     * get a weight value on how good a tile fits
     * 100 means complete coverage
     * @param northwest
     * @param southeast
     * @return 
     */
    int         HasTile(const Coord::Extent &tileExt) const;
    String      GetFileName() const {return filename;}
    Coord::Extent GetExtent() const{return extent;}
    bool        IsValid()const {return state==READY;}
    bool        IsRaster() const;
    bool        IsOverlay() const;
    int         GetNumErrors() const { return numErrors;}
    void        AddErrors(int num);
    String      GetChartSetKey()const {return chartSetKey;}
    void        SetChartSetKey(const String &key){chartSetKey=key;}
    int64_t     GetFileTime()const {return fileTime;}
    int64_t     GetFileSize()const {return fileSize;}
    State       GetState() const {return state;}
    bool        IsIgnored() const { return ignore;}
    Chart::ChartType GetType() const { return type;}

    private:
    String          filename;
    String          chartSetKey;
    Coord::Extent   extent;
    int             nativeScale;
    State           state=UNKNOWN;
    bool            isOverlay=false;
    void            CheckOverlay();
    int64_t         fileSize=-1; 
    int64_t         fileTime=-1;
    int             numErrors=0; //count tries
    bool            checkOrSetFileData();
    bool            ignore=false;
    Chart::ChartType type;
};

class ChartInfoWithScale{
public:
    ChartInfo::Ptr info;
    int scale;
    bool softUnder=false;
    ChartInfoWithScale(int weight, ChartInfo::Ptr info){
        this->scale=weight;
        this->info=info;
    }
    Coord::TileBox tile;
};

class WeightedChartList : public std::vector<ChartInfoWithScale> {
    public:
    using VType=ChartInfoWithScale;
    using BaseType=std::vector<VType>;
    using BaseType::vector;
    std::unordered_set<String> names;
    bool add(const ChartInfoWithScale &i){
        if (names.find(i.info->GetFileName()) != names.end()) return false;
        BaseType::push_back(i);
        names.insert(i.info->GetFileName());
        return true;
    }
};




#endif 
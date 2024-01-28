/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Info
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
#ifndef _TILES_H
#define _TILES_H

#include <sys/time.h>
#include "MD5.h"
#include <math.h>
#include "StringHelper.h"
#include "Coordinates.h"
#include "Exception.h"




class LatLon{
public:
    Coord::LatLon lat;
    Coord::LatLon lon;
    LatLon(Coord::LatLon lat,Coord::LatLon lon){
        this->lat=lat;
        this->lon=lon;
    }
    LatLon(const LatLon &other){
        this->lat=other.lat;
        this->lon=other.lon;
    }
};

class TileInfo : public Coord::TileInfoBase{
public:
    String chartSetKey;
    MD5Name  cacheKey;
    TileInfo(){}
    TileInfo(String url,String chartSetKey){
        if (sscanf(url.c_str(), "%d/%d/%d", &zoom, &x, &y) != 3) {
            valid=false;
        }
        else{
            valid=true;
        }
        this->chartSetKey=chartSetKey;       
    }
    TileInfo(int zoom, int x, int y,String chartSetKey){
        valid=true;
        this->zoom=zoom;
        this->x=x;
        this->y=y;
        this->chartSetKey=chartSetKey;
    }
    TileInfo(const TileInfo &other){
        this->zoom=other.zoom;
        this->x=other.x;
        this->y=other.y;
        this->chartSetKey=other.chartSetKey;
        this->valid=other.valid;
        this->cacheKey=other.cacheKey;
    }
    bool operator==(const TileInfo &other){
        if (other.chartSetKey != chartSetKey) return false;
        if (other.zoom != zoom) return false;
        if (other.x != x) return false;
        if (other.y != y) return false;
        return true;
    }
    String ToString(bool omitName=false)const {
        if (! omitName){
            return StringHelper::format("tile %s,z=%d,x=%d,y=%d",chartSetKey,zoom,x,y);
        }
        else{
            return StringHelper::format("tile z=%d,x=%d,y=%d",zoom,x,y);
        }
    }
    MD5Name GetCacheKey(){
        return cacheKey;
    }
};

class RenderException: public AvException{
        protected:
            const TileInfo tile;
        public:
        using AvException::AvException;
        RenderException(const TileInfo &t, const String &error): tile(t),AvException(error){
        }
        virtual const char *type() const noexcept override {return "RenderException";}
        virtual const String msg() const noexcept override{
            return StringHelper::format("%s:(%s) %s",type(),tile.ToString(),reason);
        }    
    };

#endif 

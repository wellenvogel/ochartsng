/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test Coordinates
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
#include <gtest/gtest.h>
#include "SimpleThread.h"
#include "TestHelper.h"
#include "Coordinates.h"

//get coordinates from https://developers.google.com/maps/documentation/javascript/coordinates
typedef std::function<Coord::LatLon(Coord::World)> Conv;
static void printRange(const String &prfx, int32_t range,Coord::World base, Conv conv){
    int op=std::cerr.precision();
    std::cerr.precision(20);
    for (int32_t x=-range;x<=range;x++){
        Coord::World yn=base+x;
        std::cerr << prfx << "(" << x << ": "<< yn<<"): "<< conv(yn) << std::endl;
    }
    std::cerr.precision(op);
}
//convert an tile coordinate on zoom level COORD_ZOOM_LEVEL to a world coordinate
/*
import math
def deg2num(lat_deg, lon_deg, zoom):
  lat_rad = math.radians(lat_deg)
  n = 1 << zoom
  xtile = int((lon_deg + 180.0) / 360.0 * n)
  ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
  return xtile, ytile
*/
static const Coord::World halfShift=Coord::worldLimits.halfshift();
Coord::World tcToWorld(Coord::World tc){ return (tc << ((Coord::TILE_SIZE_BITS) + Coord::SUB_PIXEL_BITS)) + halfShift;}
/*
x=268988,y=389836,z=20
NW: lat=41.85012765, lon=-87.65029907
SE: lat=41.84987191, lon=-87.64995575
CT: lat=41.84999978, lon=-87.65012741
*/
Coord::LatLon lat=41.85012765;
Coord::LatLon lon=-87.65029907;
uint32_t tiley=389836;
uint32_t tilex=268988;
TEST(Coordinates,LatToWorldy){
    Coord::World y=tcToWorld(tiley); 
    Coord::World ty=Coord::latToWorldy(lat);
    Coord::LatLon tlat=Coord::worldyToLat(ty);
    std::cerr.precision(20);
    std::cerr << "Factor: " << Coord::COORD_FACTOR << std::endl;
    std::cerr << "tlat: " << tlat << std::endl;
    printRange("tlatr",20,y,Coord::worldyToLat);
    IN_RANGE(ty,y,0.0001);
    IN_RANGE(tlat,lat,0.0001);
}
Coord::LatLon worldxToLon(const Coord::World x){
    return Coord::worldxToLon(x);
}
TEST(Coordinates,LonToWorldx){
    Coord::World x=tcToWorld(tilex); 
    Coord::World tx=Coord::lonToWorldx(lon);
    Coord::LatLon tlon=Coord::worldxToLon(tx);
    std::cerr.precision(20);
    std::cerr << "x: " << x << " tx: " << tx << " tlon: " << tlon << std::endl;
    printRange("tlonr",20,x,worldxToLon); 
    IN_RANGE(tx,x,0.0001);
    IN_RANGE(tlon,lon,0.0001);
}

TEST(Coordinates,PixelToWorld){
    Coord::PixelXy pp(10,20);
    Coord::TileInfoBase tinfo(2,4,6);
    Coord::TileBox tbox=Coord::tileToBox(tinfo);
    std::cerr << "tile: " << tbox.toString() << std::endl;
    Coord::WorldXy absPoint=tbox.relPixelToWorld(pp);
    std::cerr << "world: x=" << absPoint.x << ", y=" << absPoint.y << std::endl;
    Coord::PixelXy tp=tbox.worldToPixel(absPoint);
    EXPECT_EQ(pp,tp);
    Coord::TileInfoBase tinfo2(1,2,5);
    Coord::TileBox tbox2=Coord::tileToBox(tinfo2);
    std::cerr << "tile2: " << tbox2.toString() << std::endl;
    //we expect to double the rel values
    //on the next lower zoom level
    Coord::PixelXy tp2=tbox2.worldToPixel(absPoint);
    EXPECT_EQ(tp2.x*2,pp.x);
    EXPECT_EQ(tp2.y*2,pp.y);

}

TEST(Coordinates,ShiftedPlus){
    Coord::WorldXy p(1000,2000);
    Coord::WorldXy t=p.getShifted(10,20);
    EXPECT_EQ(t.x,1010);
    EXPECT_EQ(t.y,2020);
}
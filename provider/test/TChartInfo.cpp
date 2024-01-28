/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test FileHelper
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
#include "ChartInfo.h"
#include "Coordinates.h"

TEST(ChartInfo,fromCache){
    String fn="dummy.oesu";
    String fns=fn;
    Coord::Extent e;
    e.xmin=Coord::lonToWorldx(30);
    ChartInfo i(Chart::ChartType::OESU,fn,10000,e);
    EXPECT_EQ(i.GetType(),Chart::ChartType::OESU);
    EXPECT_FALSE(i.IsRaster());
    EXPECT_FALSE(i.IsOverlay());
    EXPECT_TRUE(i.IsValid());
    Coord::Extent t=i.GetExtent();
    e.xmin=0;
    EXPECT_EQ(t.xmin,Coord::lonToWorldx(30)) << "extent not copied";
    fn.clear();
    EXPECT_EQ(i.GetFileName(),"dummy.oesu") << "file name not copied";
    std::cerr << "fromCache::toString" << i.ToString() << std::endl;
}
TEST(ChartInfo,fromCacheRaster){
    String fn="dummy.oernc";
    String fns=fn;
    Coord::Extent e;
    e.xmin=Coord::lonToWorldx(30);
    ChartInfo i(Chart::ChartType::ORNC,fn,10000,e);
    EXPECT_EQ(i.GetType(),Chart::ChartType::ORNC);
    EXPECT_TRUE(i.IsRaster());
    EXPECT_FALSE(i.IsOverlay()); 
}
TEST(ChartInfo,fromCacheOverlay){
    String fn="OCL358-RTAOBL.oesu";
    String fns=fn;
    Coord::Extent e;
    e.xmin=Coord::lonToWorldx(30);
    ChartInfo i(Chart::ChartType::OESU,fn,10000,e);
    EXPECT_EQ(i.GetType(),Chart::ChartType::OESU);
    EXPECT_FALSE(i.IsRaster());
    EXPECT_TRUE(i.IsOverlay()); 
}


//https://gis.stackexchange.com/questions/7430/what-ratio-scales-do-google-maps-zoom-levels-correspond-to
static StringVector SCALES={
"19:1128.497220",
"18:2256.994440",
"17:4513.988880",
"16:9027.977761",
"15:18055.955520",
"14:36111.911040",
"13:72223.822090",
"12:144447.644200",
"11:288895.288400",
"10:577790.576700",
"9:1155581.153000",
"8:2311162.307000",
"7:4622324.614000",
"6:9244649.227000",
"5:18489298.450000",
"4:36978596.910000",
"3:73957193.820000",
"2:147914387.600000",
"1:295828775.300000",
"0:591657550.500000"
};
typedef std::map<int,double> Scales;
static Scales getScales(){
    Scales rt;
    for (auto it=SCALES.begin();it != SCALES.end();it++){
        StringVector p=StringHelper::split(*it,":",2);
        if (p.size() == 2){
            rt[atoi(p[0].c_str())]=atof(p[1].c_str());
        }
    }
    return rt;
}
static double TOL=0.1; //0.1%
TEST(ZoomLevelScales,scale1){
    ZoomLevelScales scales(1.0);
    for (int i=0;i<=19;i++){
        IN_RANGE(scales.GetScaleForZoom(i),getScales()[i],TOL);
    }
}
TEST(ZoomLevelScales,FindZoomForScale){
    ZoomLevelScales scales(1.0);
    Scales s=getScales();
    for (int i=0;i<=19;i++){
        int zoom=scales.FindZoomForScale(s[i]*(100.0-TOL)/100);
        EXPECT_EQ(i,zoom);
    }
}
/*
  tn 54 13 10
  x=548,y=328,z=10
  tn 55 14 10
  x=551,y=323,z=10

  tn 54 13 11
  x=1097,y=657,z=11
  tn 55 14 11
  x=1103,y=647,z=11
*/



TEST(ChartInfo,HasChartForTile){
    String fn="dummy.oesu";
    String fns=fn;
    int tz=10;
    int nativeScale=getScales()[tz]*(100.0-TOL)/100.0;
    Coord::LLBox e;
    e.e_lon=14;
    e.s_lat=54;
    e.w_lon=13;
    e.n_lat=55;
    ChartInfo i(Chart::ChartType::OESU,fn,nativeScale,e.toWorld());
    EXPECT_TRUE(i.IsValid());
    for (int x=548;x<=551;x++){
        for(int y=323;y<=328;y++){
            TileInfo ti(tz,x,y,"dummy");
            Coord::Extent tileExtent=Coord::tileToBox(ti);
            int ts=i.HasTile(tileExtent);
            EXPECT_EQ(ts,nativeScale);
        }
    }
    TileInfo ti(tz,552,328,"dummy");
    Coord::Extent tileExtent=Coord::tileToBox(ti);
    int ts=i.HasTile(tileExtent);
    EXPECT_EQ(ts,0) << "outside tile accepted - wrong";
}
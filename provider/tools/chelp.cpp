/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  coordinate conversions
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 *
 */

#include "Coordinates.h"
#include "Types.h"
#include <iostream>
#include <functional>
#include <vector>
#include <limits>
#include <iomanip>

double pdouble(const char *p){
    while (*p != 0 && ! (isdigit(*p) || *p == '-')) p++;
    return ::atof(p); 
}

long plong(const char *p){
    while (*p != 0 && ! (isdigit(*p)|| *p == '-')) p++;
    return ::atoll(p);
}

void usage(const char *pn){
    std::cerr << "usage: " << pn << " mode p1 p2..." << std::endl;
}
void err(const char * pn,const String &us,const String &prfx=""){
    String outprfx=prfx.empty()?"missing parameter":prfx;
    std::cerr << outprfx << ", usage: " << pn << " " << us << std::endl;
    exit(1);
}
using Handler=std::function<int(int, char**)>;
class Action{
    public:
    String mode;
    String usage;
    int numParam=1;
    Handler handler;
    Action(const String &m,int np, const String &u, Handler h):usage(u),numParam(np),handler(h),mode(m){}
    String getUsage() const{
        return mode+ " " + usage;
    }
};

template<typename T>
void printLimits(String prfx=""){
    Coord::Limits<T>limits;
    std::cout << prfx << "  [limit] min="<< limits.min()  << ", max=" 
        << limits.max() << ", hshift=" << limits.halfshift()
        << ", tmin=" << limits.typeMin() << ", tmax=" << limits.typeMax() << std::endl;
}
void printll(const Coord::LatLon &lon, const Coord::LatLon &lat, const String prfx=""){
    std::cout << prfx << "  [ll   ] lon=" << std::setprecision(11) << lon << ", lat=" << std::setprecision(11) << lat << std::endl;
}
void printCoord(Coord::World x, Coord::World y, String prfx="", bool withLimits=true){
    std::cout << prfx << "  [cz=" << std::setw(2) << Coord::COORD_ZOOM_LEVEL << "] x="<<x<<", y=" << y << std::endl;
    if (withLimits) printLimits<Coord::World>(prfx);
    printll(Coord::worldxToLon(x), Coord::worldyToLat(y),prfx);
}
void printCoord(Coord::WorldXy p,String prfx="",bool withLimits=true){
    printCoord(p.x,p.y,prfx,withLimits);
}
void printTile(const Coord::TileInfoBase &tile, String prfx=""){
    std::cout << prfx << "  [zt=" << std::setw(2) << tile.zoom << "] tilex="<<tile.x<<", tiley=" << tile.y << std::endl;
    Coord::TileBox tileExtent=Coord::tileToBox(tile.zoom,tile.x,tile.y);
    std::cout << prfx << "  [text ] " << tileExtent.toString() << std::endl;
    printCoord(tileExtent.midPoint(),"mid ",false);
    printLimits<Coord::World>(prfx);
    Coord::LLBox tileBox=Coord::LLBox::fromWorld(tileExtent);
    std::cout << prfx << "  [tile ] " << tileBox.toString() << std::endl;
}

void printExtent(Coord::Extent e, String prfx=""){
    std::cout << prfx << "  [ext  ] " << e.toString() << std::endl;
    printCoord(e.midPoint(),"mid ",false);
    printLimits<Coord::World>(prfx);
    Coord::LLBox box=Coord::LLBox::fromWorld(e);
    std::cout << prfx << "  [llbox] " << box.toString() << std::endl; 
}


int ll(int argc , char ** par){
    Coord::LatLon lat=pdouble(par[0]);
    Coord::LatLon lon=pdouble(par[1]);
    uint32_t zoom=10;
    if (argc > 2) zoom=::atol(par[2]);
    std::cout << "ll        lat=" << lat << ", lon=" << lon << ", zoom=" << zoom << std::endl;
    Coord::World x=Coord::lonToWorldx(lon);
    Coord::World y=Coord::latToWorldy(lat);
    printCoord(x,y);
    Coord::TileInfoBase tile=Coord::worldPointToTile(Coord::WorldXy(x,y),zoom);
    printTile(tile);
    return 0;
}

int wc(int argc, char **par){
    Coord::World x=plong(par[0]);
    Coord::World y=plong(par[1]);
    uint32_t zoom=10;
    if (argc > 2) zoom=::atol(par[2]);
    std::cout << "wc          x=" << x << ", y=" << y << ", zoom=" << zoom << std::endl;
    printCoord(x,y);
    Coord::TileInfoBase tile=Coord::worldPointToTile(Coord::WorldXy(x,y),zoom);
    printTile(tile);
    return 0;
}

Coord::Extent parseExtent(char **par){
    Coord::Extent rt;
    int idx=0;
    rt.xmin=plong(par[idx++]);
    rt.xmax=plong(par[idx++]);
    rt.ymin=plong(par[idx++]);
    rt.ymax=plong(par[idx++]);
    rt.valid=true;
    return rt;
}

void printIntersects(const Coord::Extent &e1, const Coord::Extent &e2, String prfx=""){
    std::cout << prfx << "  [ ext1] " << e1.toString() << std::endl;
    std::cout << prfx << "  [ ext2] " << e2.toString() << std::endl;
    printLimits<Coord::World>(prfx);
    bool inters=e1.intersects(e2);
    bool includes=e1.includes(e2);
    std::cout << prfx << "  [  res] intersects=" << inters << ", includes=" << includes << std::endl;
}

int intersects(int argc, char **par){
    Coord::Extent e1=parseExtent(par);
    Coord::Extent e2=parseExtent(&par[4]);
    std::cout << "intersects"<< std::endl;
    printIntersects(e1,e2);
    return 0;
}

int wbox(int argc, char **par){
    Coord::Extent e=parseExtent(par);
    printExtent(e);
    return 0;
}

Coord::TileInfoBase parseTile(char **par){
    int x=plong(par[0]);
    int y=plong(par[1]);
    int z=plong(par[2]);
    return Coord::TileInfoBase(x,y,z);
}

int tile1(int argc, char **par){
    std::cout << "tile" << std::endl;
    Coord::TileInfoBase tile=parseTile(par);
    printTile(tile);
    return 0;    
}
int tilePix(int argc, char **par){
    std::cout << "tilePix" << std::endl;
    Coord::TileInfoBase tile=parseTile(par);
    Coord::Pixel x=plong(par[3]);
    Coord::Pixel y=plong(par[4]);
    printTile(tile);
    std::cout << "  [pixel] x=" << x << ",y= " << y << std::endl;
    Coord::TileBox tileBox=Coord::tileToBox(tile);
    Coord::WorldXy w=tileBox.relPixelToWorld(Coord::PixelXy(x,y));
    printCoord(w.x,w.y);
    return 0;    
}
int wcToPix(int argc, char **par){
    Coord::World x=plong(par[0]);
    Coord::World y=plong(par[1]);
    uint32_t zoom=10;
    if (argc > 2) zoom=::atol(par[2]);
    std::cout << "wc          x=" << x << ", y=" << y << ", zoom=" << zoom << std::endl;
    printCoord(x,y);
    Coord::TileInfoBase tile=Coord::worldPointToTile(Coord::WorldXy(x,y),zoom);
    printTile(tile);
    Coord::TileBox tileBox=Coord::tileToBox(tile);
    Coord::PixelXy px=tileBox.worldToPixel(Coord::WorldXy(x,y));
    std::cout << "  [pixel] x=" << px.x << ",y= " << px.y << std::endl;
    return 0;

}
int tileIntersect(int argc, char **par){
    std::cout << "tile intersects" <<std::endl;
    Coord::TileInfoBase tile1=parseTile(par);
    Coord::TileInfoBase tile2=parseTile(&par[3]);
    printTile(tile1,"T1");
    printTile(tile2,"T2");
    Coord::Extent e1=Coord::tileToBox(tile1);
    Coord::Extent e2=Coord::tileToBox(tile2);
    printIntersects(e1,e2,"  ");
    return 0;
}

int en(int argc, char **par){
    double easting=pdouble(par[0]);
    double norting=pdouble(par[1]);
    double lon=pdouble(par[2]);
    double lat=pdouble(par[3]);
    std::cout << "easting=" << easting << ", northing=" << norting << std::endl;
    Coord::LLXy ref(lon,lat);
    printll(ref.x,ref.y,"Ref");
    Coord::WorldXy wpoint=Coord::oldWorldFromSM(easting,norting,ref);
    printCoord(wpoint);
    return 0;
}
int ena(int argc, char **par){
    double easting=pdouble(par[0]);
    double norting=pdouble(par[1]);
    double lon=pdouble(par[2]);
    double lat=pdouble(par[3]);
    std::cout << "easting=" << easting << ", northing=" << norting << std::endl;
    Coord::LLXy ref(lon,lat);
    printll(ref.x,ref.y,"Ref");
    Coord::WorldXy refw=Coord::latLonToWorld(ref);
    printCoord(refw,"RefW");
    Coord::WorldXy wpoint=Coord::worldFromSM(easting,norting,refw);
    printCoord(wpoint);
    return 0;
}

std::vector<Action> actions({
    Action("ll",2,"lat lon [zoom=10]",ll),
    Action("wc",2,"x y [zoom=10]",wc),
    Action("wbox",4,"xmin xmax ymin ymax",wbox),
    Action("is",8,"xmin1 xmax1 ymin1 ymax1 xmin2 xmax2 ymin2 ymax2",intersects),
    Action("tc",3,"tilex tiley zoom",tile1),
    Action("tcpix",5,"tilex tiley zoom xpix ypix",tilePix),
    Action("tis",6,"tile1x tile1y zoom1 tile2x tile2y zoom2  ",tileIntersect),
    Action("wcpix",2,"x y [zoom=10]",wcToPix),
    Action("en",4,"easting norting reflon reflat",en),
    Action("ena",4,"easting norting reflon reflat",ena)

});

int main(int argc, char **argv){
    if (argc < 2){
        usage(argv[0]);
        exit(1);
    }
    String mode(argv[1]);
    int npar=argc-2;
    char ** par=&argv[2];
    String modes="";
    for (auto it=actions.begin();it!=actions.end();it++){
        modes.append(it->mode).append(" ");
        if (it->mode == mode){
            if (npar < it->numParam){
                err(argv[0],it->getUsage());
            }
            int rt=it->handler(npar,par);
            exit(rt);
        }
    }
    std::cerr << "invalid mode " << mode << ", allowed: " << modes << std::endl;
    
}
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Coordinates
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

#ifndef _COORDINATES_H
#define _COORDINATES_H
#include "Types.h"
#include "StringHelper.h"
#include "Exception.h"
#include <math.h> 
#include <algorithm>
#include <limits>

namespace Coord
{
    template <typename T>
    class Point{
        public:
        T x;
        T y;
        Point():x(0),y(0){}
        Point(const T &xp,const T &yp):x(xp),y(yp){}
        Point(const Point<T> &other):x(other.x),y(other.y){}
        void shift(T sx, T sy){
            x+=sx;
            y+=sy;
        }
        void invert(){
            x=-x;
            y=-y;
        }
        Point<T> getInverted() const{
            return Point<T>(-x,-y);
        }
        void shift(const Point<T> &xy){
            shift(xy.x,xy.y);
        }
        Point<T> getShifted(T x, T y) const{
            Point<T> shifted(*this);
            shifted.shift(x,y);
            return shifted;
        }
        Point<T> getShifted(const Point<T> &xy) const{
            return getShifted(xy.x,xy.y);
        }

        bool operator == (const Point<T> &other) const{
            return x==other.x && y==other.y;
        }
        bool operator != (const Point<T> &other) const{
            return ! (*this == other); 
        }

        /*bool operator < (const Point<T> &other) const{
            return x < other.x && y < other.y;
        }*/
    }; 
    template <typename T>
    class PointHash{
        public:
        std::size_t operator()(Point<T> const & p) const noexcept{
            std::size_t h1=std::hash<T>{}(p.x);
            std::size_t h2=std::hash<T>{}(p.y);
            return h1 ^ (h2 << 1);
        }
    };
    /**
     * world coordinates
     * those are projected coordinates computed for zoom level COORD_ZOOM_LEVEl
     * we reserve 2 bits for overflow (180° handling) and one bit for sign
     * Overflow can occur by:
     * (1) the points in the chart files are stored using easting/norting related
     *     to the ref point of the chart
     *     in theory this could create overflows in positive and negative direction
     *     if the ref point is close to +/-180° lon
     *     we allow a full range of the earth in both directions (1 bit)
     * (2) When finding charts and rendering we must handle the +/- 180°
     *     so we will try the normal tile extent and tile extents shifted by 
     *     a complete earth east and west (example for e.g. zl 16):
     *     for a tile with x=14 we also try with x=-2 and x=30
     *     As the pixel coordinates are computed by subtracting the tile min 
     *     coordinate we again need one bit overflow
     * As the values are not symmetric to 0 (projected coordinates run from 0...max)
     * we shift the world coordinates by half their (type) max values to negative
     * (for an int32 this is using - (2 << 16))
     * To optimize storage we would like to use a 32 bit int for world coordinates.
     * As we need 1 bit for sign and 2 bits for overflow we have 29 Bits available.
     * A max zoom level of 20 should be enough from the normal usage perspective.
     * With max zoom 20 we need:
     * 20 bits zoom + 8 Bits (Tile size 256) - leaving us one bit subpixel accuracy
     */
    static const constexpr double WGS84_semimajor_axis_meters=6378137.0;  // WGS84 semimajor axis
    static const constexpr double mercator_k0 = 0.9996; 
    static const constexpr double DEGREE=M_PI/180.0;  
    static const constexpr uint32_t COORD_ZOOM_LEVEL=20;
    static const constexpr uint32_t TILE_SIZE_BITS=8;
    static const constexpr uint32_t TILE_SIZE=1 << TILE_SIZE_BITS;
    static const constexpr uint32_t SUB_PIXEL_BITS=1;
    static const constexpr uint32_t SUB_PIXEL_MASK=(1<<SUB_PIXEL_BITS)-1;
    typedef double LatLon; // lat lon coordinates
    typedef Point<LatLon> LLXy;
    static const constexpr LatLon   SUB_PIXEL_MUL=1<<SUB_PIXEL_BITS;
    static const constexpr uint32_t WORLD_FACTOR=TILE_SIZE*(1<< SUB_PIXEL_BITS);
    static const constexpr uint32_t ZOOM_FACTOR=(1<<COORD_ZOOM_LEVEL);
    static const constexpr LatLon   COORD_FACTOR=ZOOM_FACTOR * WORLD_FACTOR;
    static const constexpr LatLon   MAX_LAT=85.0511;
    static const constexpr LatLon   MIN_LAT=-85.0511;

    
    typedef int32_t World;

    typedef Point<World> WorldXy;

    /**
     * pixel coordinates - basically Draw without the sub pixels
     */
    typedef int32_t Pixel;

    /**
     * limits for the "normal" range
    */
    template <typename RefT>
    class Limits{
        public:
        constexpr static const std::numeric_limits<RefT> typeLimits=std::numeric_limits<RefT>();
        //the value we add to the coordinates as described above
        constexpr RefT halfshift() const {return -((((int32_t)1) << (COORD_ZOOM_LEVEL+TILE_SIZE_BITS+SUB_PIXEL_BITS-1))-1);}
        //the max for the normal range without overflow
        constexpr RefT max() const {return (((int32_t)1) << (COORD_ZOOM_LEVEL+TILE_SIZE_BITS+SUB_PIXEL_BITS)) -1 + halfshift();}
        //the min for the normal range without overflow
        constexpr RefT min() const {return halfshift();}
        //clip to the range without overflow
        constexpr RefT clip(const RefT &v) const{
            if (v < min()) return min();
            if (v > max()) return max();
            return v;
        }
        //the value we have to add/sub to shift a whole world (lon)
        constexpr RefT worldShift() const { return (max()-min()) + 1;}
        //check if the value is within the min/max without overflow
        bool in(const RefT &v) const { return v>= min() && v <= max();}
        //min/max of the used type
        constexpr RefT typeMax() const { return typeLimits.max();}
        constexpr RefT typeMin() const { return typeLimits.min();}
        //safe shift operation clipping to the type min/max
        RefT shift(const RefT &v, const RefT shift) const{
            if (shift == 0) return v;
            if (shift > 0){
                if ((typeMax()-shift) < v) return typeMax();
                return v+shift;
            }
            if ((typeMin()-shift) > v) return typeMin();
            return v+shift;
        }
        
    };
    
    static Limits<World> worldLimits;

    //safe shift
    //shift > 0: leftshift
    template<typename T>
    static T bitshift(const T &v, int32_t shift){
        if (v == 0 || shift == 0) return v;
        if (shift < 0){
            if (v < 0) return - ((-v) >> (-shift));
            return v >> (-shift);
        }
        if (v < 0) return - ((-v) << shift);
        return v << shift;
    }
    
    static inline LatLon worldxToLon(World x, bool clip=true){
        LatLon rt = ((LatLon)x - (LatLon)worldLimits.halfshift()) / COORD_FACTOR * 360.0 - 180.0 ;
        if (clip){
            while (rt > 180) rt-=360;
            while (rt < -180) rt+=360;
        }
        return rt;
    }

    /**
     * convert the longitude to world x
     * to handle the 180°/0° overflow
     * we can allow to overflow (clip=false)
     * this is used when handling easting/norting (i.e. relative coordinates)
     * if clip=false the range checking has to be done by the caller!
    */
    static inline World lonToWorldx(LatLon lon, bool clip=true){

        if (clip){
            //TODO: normal range
            static LatLon clipLonMax=180.0;
            static LatLon clipLonMin=-180.0;
            while (lon > clipLonMax) lon-=360.0;
            while (lon < clipLonMin) lon+=360;
        }
        World rt=worldLimits.shift(floor((lon + 180.0) / 360.0 * COORD_FACTOR), worldLimits.halfshift());
        return rt;
    }
    static inline World latToWorldy(LatLon lat){
        if (lat < MIN_LAT) lat=MIN_LAT;
        if (lat > MAX_LAT) lat=MAX_LAT;
        LatLon latrad = lat * M_PI/180.0;
        World rt=worldLimits.shift(floor(((1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * COORD_FACTOR)), worldLimits.halfshift());
        return rt;
    }
    static inline LatLon worldyToLat(World y){
        static LatLon offset = (-worldLimits.halfshift());
        LatLon n = M_PI - 2.0 * M_PI * (LatLon)(y+offset) / COORD_FACTOR;
	    return 180.0 / M_PI * atan(0.5 * (exp(n) - exp(-n)));
    }
    static inline WorldXy latLonToWorld(LatLon lat, LatLon lon){
        WorldXy rt;
        rt.x=lonToWorldx(lon);
        rt.y=latToWorldy(lat);
        return rt;
    }
    static inline WorldXy latLonToWorld(const LLXy &ll){
        WorldXy rt;
        rt.x=lonToWorldx(ll.x);
        rt.y=latToWorldy(ll.y);
        return rt;
    }
    
    //be careful when using this:
    //as the world coordinates are shifted the returned pixel are shifted too
    //so normally it only make sense to call this for relative coordinates (e.g. relative to a tile)
    static inline Pixel worldToPixel(World c, uint32_t zoom){
        if (zoom > COORD_ZOOM_LEVEL) return c;
        return bitshift(c,-((COORD_ZOOM_LEVEL-zoom)+SUB_PIXEL_BITS));
    }

    //convert to 0 based Pixel coordinates
    static inline Pixel worldToAbsPixel(World c, uint32_t zoom){
        while (c > worldLimits.max()) c=worldLimits.shift(c,-worldLimits.worldShift());
        while (c < worldLimits.min()) c=worldLimits.shift(c,worldLimits.worldShift());
        c=worldLimits.clip(c);
        return worldToPixel(worldLimits.shift(c,-worldLimits.halfshift()),zoom);
    }

    typedef Point<Pixel> PixelXy;
    static inline PixelXy worldToPixel(const WorldXy &c, uint32_t zoom){
        return PixelXy(worldToPixel(c.x,zoom),worldToPixel(c.y,zoom));
    }
    static inline World pixelToWorld(const World c, uint32_t zoom){
         if (zoom > COORD_ZOOM_LEVEL) zoom=COORD_ZOOM_LEVEL;
         return bitshift(c,(COORD_ZOOM_LEVEL-zoom)+SUB_PIXEL_BITS);
    }
    
    template <typename T>
    class Box
    {
        public:
        using Type=T;
        static const Limits<T> limits;
        protected:
    public:
        bool valid = false;
        bool Valid() const
        {
            return valid;
        }
        // upper left : NW, lower right: SO
        T xmin = 0; // upper left x
        T xmax = 0; // lower right x
        T ymin = 0; // upper left y
        T ymax = 0; // lower right y
        Box() {}
        bool intersects(const Box<T> &other) const{
            if (! other.valid) return false;
            if (xmax < other.xmin || ymax < other.ymin) return false;
            if (xmin > other.xmax || ymin > other.ymax) return false;
            return true;
        }
        bool intersects(const Point<T> &other) const{
            if (other.x >= xmin && other.x <= xmax && other.y >= ymin && other.y <=ymax) return true;
            return false;
        }
        //do we include other completely?
        bool includes(const Box<T> &other) const
        {
            if (other.xmin >= xmin && other.xmax <= xmax
                && other.ymin >= ymin && other.ymax <= ymax
            )
            return true;
            return false;
        }
        bool extend(const Box &other)
        {
            if (!other.valid)
                throw AvException("cannot extend a box with an invalid one "+toString()+", o="+other.toString());
            if (!valid)
            {
                xmin = other.xmin;
                ymin = other.ymin;
                xmax = other.xmax;
                ymax = other.ymax;
                valid = true;
                return true;
            }
            bool changed = false;
            if (other.xmin < xmin)
            {
                xmin = other.xmin;
                changed = true;
            }
            if (other.ymin < ymin)
            {
                ymin = other.ymin;
                changed = true;
            }
            if (other.xmax > xmax)
            {
                xmax = other.xmax;
                changed = true;
            }
            if (other.ymax > ymax)
            {
                ymax = other.ymax;
                changed = true;
            }
            return changed;
        }
        bool extend(const Point<T> &point)
        {
            if (! valid){
                xmin=point.x;
                xmax=point.x;
                ymin=point.y;
                ymax=point.y;
                valid=true;
                return true;
            }
            bool changed = false;
            if (point.x < xmin)
            {
                xmin = point.x;
                changed = true;
            }
            if (point.y < ymin)
            {
                ymin = point.y;
                changed = true;
            }
            if (point.x > xmax)
            {
                xmax = point.x;
                changed = true;
            }
            if (point.y > ymax)
            {
                ymax = point.y;
                changed = true;
            }
            return changed;
        }
        bool shift(T x, T y){
            if (! valid) return false;
            xmin=limits.shift(xmin,x);
            xmax=limits.shift(xmax,x);
            ymin=limits.shift(ymin,y);
            ymax=limits.shift(ymax,y);
            return true;
        }
        void expand(const T x, const T y){
            if (! valid) return;
            xmin=limits.shift(xmin,-x);
            xmax=limits.shift(xmax,x);
            ymin=limits.shift(ymin,-x);
            ymax=limits.shift(ymax,y);
        }

        Box<T> getExpanded(const T x, const T y) const{
            Box<T> rt(*this);
            rt.expand(x,y);
            return rt;
        }
        bool shift(const Point<T> &xy){
            return shift(xy.x, xy.y);
        }
        Box<T> getShifted(T x, T y) const{
            Box<T> shifted(*this);
            shifted.shift(x,y);
            return shifted;
        }
        Box<T> getShifted(const Point<T> &xy) const{
            return getShifted(xy.x,xy.y);
        }
        Point<T> midPoint() const{
            Point<T> rt;
            rt.x=xmin/2+xmax/2;
            rt.y=ymin/2+ymax/2;
            return rt;
        }
        String toString() const{
            return FMT("Box: valid=%d,xmin=%d, xmax=%d, ymin=%d, ymax=%d",
                valid,xmin,xmax,ymin,ymax);
        }
    };
    template<typename T> const Limits<T> Box<T>::limits;

    using PixelBox=Box<Pixel>;
        
    class TileBounds : public PixelBox{
        public:
        TileBounds(){
            xmin=0;
            xmax=TILE_SIZE;
            ymin=0;
            ymax=TILE_SIZE;
            valid=true;
        }
    };

    class TileBox : public Box<World>
    {
        using Box::Box;
    public:
        uint32_t zoom;
        bool extend(const TileBox &other)
        {
            if (other.zoom != zoom)
                return false;
            return Box<World>::extend(other);    
        }
        TileBox getShifted(World x, World y) const{
            TileBox rt=(*this);
            rt.shift(x,y);
            return rt;
        }
        WorldXy relPixelToWorld(const PixelXy &relPoint)const{
            WorldXy relWorld(pixelToWorld(relPoint.x,zoom),pixelToWorld(relPoint.y,zoom));
            return WorldXy(limits.shift(xmin,relWorld.x),limits.shift(ymin,relWorld.y));
        }
        PixelXy worldToPixel(const WorldXy &wp) const{
            WorldXy relWorld(limits.shift(wp.x,-xmin),limits.shift(wp.y,-ymin));
            return PixelXy(Coord::worldToPixel(relWorld.x,zoom), Coord::worldToPixel(relWorld.y,zoom));
        }
        TileBounds getPixelBounds() const{
            TileBounds rt;
            rt.xmax=Coord::worldToPixel(xmax+1-xmin,zoom)-1;
            rt.ymax=Coord::worldToPixel(ymax+1-ymin,zoom)-1;
            return rt;
        }
    };


    using Extent=Box<World>;
    class LLBox {
        public:
        LatLon e_lon;
        LatLon w_lon;
        LatLon n_lat;
        LatLon s_lat;
        Extent toWorld() const{
            Extent rt;
            rt.xmin=lonToWorldx(w_lon);
            rt.ymin=latToWorldy(n_lat);
            rt.xmax=lonToWorldx(e_lon);
            rt.ymax=latToWorldy(s_lat);
            rt.valid=true;
            return rt;    
        }
        static LLBox fromWorld(const Extent &ext){
            LLBox rt;
            rt.e_lon=worldxToLon(ext.xmax);
            rt.s_lat=worldyToLat(ext.ymax);
            rt.w_lon=worldxToLon(ext.xmin);
            rt.n_lat=worldyToLat(ext.ymin);
            return rt;
        }
        LLXy midPoint() const{
            //TODO: 180/0 crossing!
            return LLXy((e_lon+w_lon)/2,(s_lat+n_lat)/2);
        }
        String toString() const{
            return StringHelper::format("LLBox: nwlat=%3.12f, nwlon=%3.12f, selat=%3.12f, selon=%3.12f",
            n_lat,w_lon,s_lat,e_lon);
        }
    };
    
    static inline PixelBox worldExtentToPixel(const Extent &etx, const TileBox &tile){
        Extent shifted=etx.getShifted(-tile.xmin,-tile.ymin);
        PixelBox rt;
        rt.xmin=worldToPixel(shifted.xmin,tile.zoom);
        rt.xmax=worldToPixel(shifted.xmax,tile.zoom);
        rt.ymin=worldToPixel(shifted.ymin,tile.zoom);
        rt.ymax=worldToPixel(shifted.ymax,tile.zoom);
        rt.valid=true;
        return rt;
    }

class TileInfoBase
    {
    public:
        int x = 0;
        int y = 0;
        int zoom = 0;
        bool valid = false;
        TileInfoBase(){}
        TileInfoBase(int xc, int yc, int z):x(xc),y(yc),zoom(z),valid(true){}
    };

    /**
     * tile to world extent
    */
    static inline TileBox tileToBox(uint32_t zoom, int tilex,int tiley, int pixelBorder=0){
        TileBox rt;
        if (zoom > COORD_ZOOM_LEVEL) zoom=COORD_ZOOM_LEVEL;
        uint32_t shift=COORD_ZOOM_LEVEL-zoom+TILE_SIZE_BITS+SUB_PIXEL_BITS;
        rt.zoom=zoom;
        rt.xmin=rt.limits.shift(bitshift(tilex,shift),rt.limits.halfshift());
        rt.ymin=rt.limits.shift(bitshift(tiley,shift),rt.limits.halfshift());
        rt.xmax=rt.limits.shift(bitshift(tilex+1,shift)-1,rt.limits.halfshift());
        rt.ymax=rt.limits.shift(bitshift(tiley+1,shift)-1,rt.limits.halfshift());
        if (pixelBorder > 0){
            int shiftedBorder=bitshift(pixelBorder,COORD_ZOOM_LEVEL-zoom+ SUB_PIXEL_BITS);
            rt.xmin= rt.limits.shift(rt.xmin,-shiftedBorder);
            rt.ymin= rt.limits.shift(rt.ymin, -shiftedBorder);
            rt.xmax= rt.limits.shift(rt.xmax, + shiftedBorder);
            rt.ymax= rt.limits.shift(rt.ymax, + shiftedBorder);
        }
        rt.valid=true;
        return rt;
    }
    static inline TileBox tileToBox(const TileInfoBase &info, int pixelBorder=0){
        return tileToBox(info.zoom,info.x,info.y,pixelBorder);
    }
    
    /**
     * get a tile for a world coordinate and a zoom level
    */
    static TileInfoBase worldPointToTile(const Coord::WorldXy &wp, const int &zoom){
        TileInfoBase rt;
        rt.zoom=(zoom<=COORD_ZOOM_LEVEL)?zoom:COORD_ZOOM_LEVEL;
        World x=wp.x;
        while (x > worldLimits.max()) x=worldLimits.shift(x,-worldLimits.worldShift());
        while (x < worldLimits.min()) x=worldLimits.shift(x,worldLimits.worldShift());
        World y=worldLimits.clip(wp.y); //no overflow for y...
        rt.valid=true;
        int shift=COORD_ZOOM_LEVEL-zoom+TILE_SIZE_BITS+SUB_PIXEL_BITS;
        rt.x=bitshift(x-worldLimits.halfshift(),-shift);
        rt.y=bitshift(y-worldLimits.halfshift(),-shift);
        return rt;
    }

    class CombinedPoint{
        public:
        LLXy llPoint;
        WorldXy worldPoint;
        bool valid=false;
        CombinedPoint(){}
        CombinedPoint(const LLXy &ll,const WorldXy &w):llPoint(ll),worldPoint(w),valid(true){}
    };
    /**
     * get world diff coordinates
     * from easting/norting of a ref point
     * for x-ccordinates we allow overflow below min and above max to handle the 0/180 issue
    */
    static inline WorldXy oldWorldFromSM(double x, double y, const LLXy &ref)
    {
        static LatLon clipLonMax=worldxToLon(worldLimits.max()+worldLimits.worldShift()-1,false);
        static LatLon clipLonMin=worldxToLon(worldLimits.min()-worldLimits.worldShift()+1,false);
        WorldXy rt;
        const double z = WGS84_semimajor_axis_meters * mercator_k0;

        // lat = arcsin((e^2(y+y0) - 1)/(e^2(y+y0) + 1))
        /*
              double s0 = sin(lat0 * DEGREE);
              double y0 = (.5 * log((1 + s0) / (1 - s0))) * z;

              double e = exp(2 * (y0 + y) / z);
              double e11 = (e - 1)/(e + 1);
              double lat2 =(atan2(e11, sqrt(1 - e11 * e11))) / DEGREE;
        */
        //    which is the same as....

        const double s0 = sin(ref.y * DEGREE);
        const double y0 = (.5 * log((1 + s0) / (1 - s0))) * z;

        double resy = (2.0 * atan(exp((y0 + y) / z)) - M_PI / 2.) / DEGREE;
        rt.y=latToWorldy(resy);

        // lon = x + lon0
        double resx = ref.x + (x / (DEGREE * z));
        if (resx > clipLonMax) {
            resx=clipLonMax;
        }
        if (resx < clipLonMin){
            resx=clipLonMin;
        }
        rt.x=lonToWorldx(resx,false);
        return rt;
    }

    static inline WorldXy worldFromSM(double x, double y, const WorldXy &ref)
    {
        static LatLon clipLonMax=worldxToLon(worldLimits.max()+worldLimits.worldShift()-1,false);
        static LatLon clipLonMin=worldxToLon(worldLimits.min()-worldLimits.worldShift()+1,false);
        const constexpr double z = WGS84_semimajor_axis_meters * mercator_k0;
        const constexpr double iwz=COORD_FACTOR/(2.0 *M_PI *z); //from mercator to world
        //original
        //World rt=worldLimits.shift(floor(((1.0 - asinh(tan(latrad)) / M_PI) / 2.0 * COORD_FACTOR)), worldLimits.halfshift());
        //asinh(tan(latrad) = (y+yref)/z
        
        World yr=ref.y-round(y*iwz);
        World xr=ref.x+round(x*iwz);
        return WorldXy(xr,yr);
    }
    
};
#endif
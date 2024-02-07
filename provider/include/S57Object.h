/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S57Object
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

#ifndef _S57OBJECT_H
#define _S57OBJECT_H
#include <vector>
#include <memory>
#include "Coordinates.h"
#include "Exception.h"
#include "RenderContext.h"
#include "DrawingContext.h"
#include "S52Data.h"
#include "S52Rules.h"
#include "OcAllocator.h"
#include "ObjectDescription.h"

class S57BaseObject{
    public:
    DECL_EXC(AvException, InvalidDataException);
    using Ptr=std::shared_ptr<S57BaseObject>;
    using ConstPtr=std::shared_ptr<const S57BaseObject>;
    S57BaseObject(ocalloc::PoolRef p,uint16_t featureId, uint16_t featureTypeCode, uint8_t featurePrimitive);
    virtual ~S57BaseObject(){}
    ocalloc::PoolRef pool;
    Coord::Extent extent;
    const uint16_t featureId = 0;
    const uint16_t featureTypeCode = 0;
    const uint8_t featurePrimitive = 0;
    Coord::WorldXy point=Coord::WorldXy(0,0);
    s52::GeoPrimitive geoPrimitive=s52::GEO_UNSET;
    s52::Attribute::Map attributes;
    virtual bool isMultipoint() const{return false;};
};
class S57Object : public S57BaseObject
{
public:
    using Ptr=std::shared_ptr<S57Object>;
    using ConstPtr=std::shared_ptr<const S57Object>;
    S57Object(ocalloc::PoolRef p,uint16_t featureId, uint16_t featureTypeCode, uint8_t featurePrimitive);
    class RenderObject
    {
    public:
        using Ptr=std::shared_ptr<RenderObject>;
        using StringTranslator=std::function<String(const String&)>;
        typedef ocalloc::Vector<const s52::Rule *> ARuleList;
        typedef ocalloc::Map<uint32_t, ARuleList> CondRules;
        RenderObject(ocalloc::PoolRef p,S57Object::ConstPtr o);
        void Render(RenderContext &, DrawingContext &ctx, Coord::TileBox const &tile, const s52::RenderStep &step) const;
        void RenderSingleRule(RenderContext &, DrawingContext &ctx, Coord::TileBox const &tile, const s52::Rule *rule) const;
        bool Intersects(const Coord::PixelBox &pixelExtent, Coord::TileBox const &tile) const;
        void SetLUP(const s52::LUPrec *lup) { this->lup = lup; }
        int GetDisplayPriority() const
        {
            if (!lup)
                return s52::PRIO_NONE;
            return lup->DPRI;
        }
        void expand(const s52::S52Data *s52data, s52::RuleCreator *creator, const s52::RuleConditions *conditions = NULL);
        void expandRule(const s52::S52Data *s52data, const s52::Rule *rule);
        s52::DisCat getDisplayCat() const;
        bool shouldRenderCat(const RenderSettings *rs) const; //check display category, muts be called after expand
        bool shouldRenderScale(const RenderSettings *rs,int scale) const;
        //get an object description
        //if the object would really render (could return empty)
        ObjectDescription::Ptr getObjectDescription(RenderContext &ctx, DrawingContext &draw,
                const Coord::TileBox &box,bool overview, StringTranslator translator) const;
    protected:
        ocalloc::PoolRef pool;
        S57Object::ConstPtr object;
        const s52::LUPrec *lup = nullptr;
        CondRules condRules; //late resolved conditional rules
        Coord::PixelBox pixelExtent; //extent relative to the point for point objects
        int xmargin=0; //if we do not use a pixel extent we add this to the translated extent
        int ymargin=0; //to add some pixels around the extent when checking for render
        ocalloc::Map<uint32_t,s52::DisplayString> expandedTexts; //expanded texts for TE/TX
        ocalloc::Map<uint32_t,s52::Arc> arcs;
        s52::DisCat displayCategory=s52::DisCat::UNDEFINED; //can be overwritten by special rule and will win against LUP cat
    };

    class VertexList
    {

    public:
        using Ptr=std::shared_ptr<VertexList>;
        uint8_t type = 0;
        int numAreaPoints = 0;
        int idx = 0;
        int capacity = 0;
        ocalloc::Vector<Coord::WorldXy> points;
        Coord::Extent extent;
        VertexList(uint8_t t, int capacity, ocalloc::PoolRef p) : type(t),points(p) {
            this->capacity=capacity;
            points.reserve(capacity);
        }
        ~VertexList(){
            
        }
        void add(Coord::WorldXy point)
        {
            if (numAreaPoints >= capacity)
                throw InvalidDataException("cannot add point to area");
            this->points.push_back(point);
            this->extent.extend(point);
            numAreaPoints++;
        }
    
    };

    class Sounding: public Coord::WorldXy{
        public:
        using Coord::WorldXy::WorldXy;
        Sounding(const Coord::WorldXy &p):Coord::WorldXy(p){
            
        }
        float depth=0;
    };
    typedef ocalloc::Vector<VertexList::Ptr> Area;
    class Soundings : public ocalloc::Vector<Sounding>{
        public:
        using base=ocalloc::Vector<Sounding>;
        using base::base;
        float min=99999;
        float max=-99999;
        void add(const Sounding &s){
            push_back(s);
            if (s.depth < min) min=s.depth;
            if (s.depth > max) max=s.depth;
        }
    };

    class LineSegment{
        public:
            Coord::WorldXy start;
            Coord::WorldXy end;
            bool valid=false;
    };
    class LineIndex{
        public:
            int first=-1;
            int vectorEdge=0;
            int endNode=0;
            bool forward=true;
            bool ownsEdges=false;
        LineIndex(const int buffer[4], bool above200=true){
            //see eSENCChart.cpp:AssembleLineGeometries
            //as we are on senc >= 200
            first=buffer[0];
            if (! above200){
                forward=buffer[1]>=0;
            }
            else{
                forward=buffer[3]==0;
            }
            if (! forward){
                int x=1; //debug
            }
            vectorEdge=buffer[1]>=0?buffer[1]:-buffer[1];
            endNode=buffer[2];
        }
        using SegmentIterator=std::function<void(Coord::WorldXy first, Coord::WorldXy last, bool isFirst)>;
        LineSegment startSegment;
        LineSegment endSegment;
        ocalloc::Vector<Coord::WorldXy> *edgeNodes=nullptr;
        bool hasPoints(){return startSegment.valid||endSegment.valid|| (edgeNodes && edgeNodes->size() >0);}
        Coord::WorldXy firstPoint() const;
        Coord::WorldXy lastPoint() const;
        void iterateSegments(SegmentIterator it, bool backward=false) const;
    };
    /**
     * the polygon has the reference to the line segments that
     * form a polygon
     * for one object there could be just one polygon - or a vector
     * of polygons
    */
    class Polygon{
        public:
        int startIndex=-1;
        int endIndex=-1;
        bool isComplete=false; //if the polygon is incomplete
                               //we must add a line from the last point
                               //to the first point
        const S57Object *obj=nullptr; // as we belong to the object this is safe
        using Winding=enum{
                WD_UN,  //unknown
                WD_CW, //clockwise
                WD_CC  //counter clockwise
            };
        Winding winding=WD_UN;
        Polygon(){}
        Polygon(const S57Object *o, int s,int e, Winding w,bool complete=true):
            startIndex(s),endIndex(e),winding(w),isComplete(complete),obj(o){}
        bool pointInPolygon(const Coord::WorldXy &p) const;
        void iterateSegments(LineIndex::SegmentIterator i,bool backwards=false) const;

    };
    
    using LineIndexes=ocalloc::Vector<LineIndex>;
    using Polygons=ocalloc::Vector<Polygon>;
    Area area;
    Soundings soundigs;
    LineIndexes lines;
    Polygons polygons;
    int scamin=-1;
    bool isAton=false;
    virtual bool isMultipoint() const override{return soundigs.size() > 0;}
};

#endif
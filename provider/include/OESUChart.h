/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  OESUChart
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
#ifndef _OESUCHART_H
#define _OESUCHART_H
#include "Chart.h"
#include "S57Object.h"
#include "S52Rules.h"
#include <unordered_set>
#include "OcAllocator.h"



class OESUChart : public Chart
{
    std::unique_ptr<ocalloc::Pool> apool;
    ocalloc::PoolRef poolRef;
    ocalloc::Vector<S57Object::Ptr> s57Objects;

public:
    class RenderData{
        public:
        s52::S52Data::ConstPtr s52data;
        s52::RuleCreator ruleCreator;
        typedef ocalloc::Vector<S57Object::RenderObject::Ptr> RenderObjects;
        RenderObjects renderObjects;
        double nextSafetyContour=1e6; //compute from all depth contures
        RenderData(ocalloc::PoolRef p,s52::S52Data::ConstPtr s52) : s52data(s52), 
            renderObjects(p),ruleCreator(p,1)
        {
        }
        ~RenderData(){
            
        }
        void addObject(S57Object::RenderObject::Ptr o){
            renderObjects.push_back(o);
        }
    };
    class VectorEdgeNode{
        public:
            ocalloc::Vector<Coord::WorldXy> points;
            Coord::Extent extent;
            VectorEdgeNode(ocalloc::PoolRef r):points(r){}   
            void add(const Coord::WorldXy &p){
                extent.extend(p);
                points.push_back(p);
            }
    };
    using VectorEdgeNodeTable=ocalloc::Map<int,VectorEdgeNode>;
    using ConnectedNodeTable=ocalloc::Map<int,Coord::WorldXy>;
    using StringMap=ocalloc::Map<ocalloc::String,ocalloc::String>;
    virtual ~OESUChart();
    OESUChart(const String &setKey, ChartType type, const String &fileName);
    virtual bool ReadChartStream(InputStream::Ptr input,s52::S52Data::ConstPtr s52data, bool headerOnly = false) override;
    virtual bool PrepareRender(s52::S52Data::ConstPtr s52data) override;
    virtual RenderResult Render(int pass,RenderContext & context,DrawingContext &out, const Coord::TileBox &box) const override;
    virtual int getRenderPasses() const override;
    virtual MD5Name GetMD5() const override;
    virtual bool IsIgnored() const override { return cellEdition == 0;}
    virtual void LogInfo(const String &prefix) const;
    virtual ObjectList FeatureInfo(RenderContext & context,DrawingContext &drawing, const Coord::TileBox &box, bool overview) const;
    virtual OexControl::OexCommands OpenHeaderCmd() const override{
         return type==Chart::OESU?OexControl::CMD_READ_OESU_HDR:OexControl::CMD_READ_ESENC_HDR;
    }
    virtual OexControl::OexCommands OpenFullCmd() const override{ 
        return type==Chart::OESU?OexControl::CMD_READ_OESU:OexControl::CMD_READ_ESENC;
    }
    class HeaderInfo{
        public:
        String cellName;
        String cellPublishDate;
        String cellUpdateDate;
        uint16_t cellUpdate=0;
        String createdDate;
        String soundingDatum;
    };

protected:
    bool setRigidFloat(const S57Object *obj); 
    int sencVersion = -1;
    std::unique_ptr<Coord::CombinedPoint> referencePoint;
    std::shared_ptr<RenderData> renderData;
    ocalloc::UnorderedSet<Coord::WorldXy,Coord::PointHash<Coord::World>> rigidAtons;
    ocalloc::UnorderedSet<Coord::WorldXy,Coord::PointHash<Coord::World>> floatAtons;
    VectorEdgeNodeTable edgeNodeTable;
    ConnectedNodeTable connectedNodeTable;
    StringMap txtdscTable;
    uint16_t cellEdition=UINT16_MAX; //avoid ignore if there was no cell edition
    void buildLineGeometries();
    HeaderInfo headerInfo;
    virtual bool aboveV200() const {return sencVersion > 200;}
};

class S57Chart: public OESUChart{
    public:
    S57Chart(const String &setKey, ChartType type, const String &fileName):
        OESUChart(setKey,type,fileName){
        }
    virtual ~S57Chart(){}
    virtual OexControl::OexCommands OpenHeaderCmd() const override{ return OexControl::OexCommands::CMD_UNKNOWN;}
    virtual OexControl::OexCommands OpenFullCmd() const override{ return OexControl::OexCommands::CMD_UNKNOWN;}
    protected:
    virtual bool aboveV200() const override {return false;}


};
class SENCChart: public OESUChart{
    public:
    SENCChart(const String &setKey, ChartType type, const String &fileName):
        OESUChart(setKey,type,fileName){
        }
    virtual ~SENCChart(){}
    virtual OexControl::OexCommands OpenHeaderCmd() const override{ return OexControl::OexCommands::CMD_UNKNOWN;}
    virtual OexControl::OexCommands OpenFullCmd() const override{ return OexControl::OexCommands::CMD_UNKNOWN;}
};

#endif
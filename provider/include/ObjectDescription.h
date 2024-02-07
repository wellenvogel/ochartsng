/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Object info class
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2024 by Andreas Vogel   *
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
#ifndef _OBJECTDESCRIPTION_H
#define _OBJECTDESCRIPTION_H
#include "Types.h"
#include "json.hpp"
#include "Coordinates.h"
#include "MD5.h"
#include <memory>
class ObjectDescription
{
public:
    Coord::WorldXy point;
    int score=0;
    MD5Name md5;
    virtual ~ObjectDescription(){}
    using Type= enum {
        T_UNDEF=0,
        T_CHART=1,
        T_OBJECT=2
    };
    Type type=T_UNDEF;
    using Ptr=std::shared_ptr<ObjectDescription>;
    double distance=std::numeric_limits<double>().max();
    virtual bool isPoint() const=0;
    virtual double computeDistance(const Coord::WorldXy &wp){return distance;};
    bool operator == (const ObjectDescription &other) const{
        return md5 == other.md5;
    }
    bool operator < (const ObjectDescription &other) const{
        if (type != other.type){
            return (int)type < (int)other.type;
        }
        if (score != other.score){
            return score > other.score;
        }
        return distance < other.distance;
    };
    virtual void toJson(json::JSON &js) const{
        js["type"]=(int)type;
    }
    virtual void jsonOverview(json::JSON &js) const{
        js["type"]=(int)type;
    }
    virtual void addValue(const String &k, const String &v){}
};

class ChartDescription : public ObjectDescription{
    protected:
    String setName;
    String chartName;
    public:
    ChartDescription(const String &n, const String &setn):setName(setn){
        distance=0;
        MD5 builder;
        chartName=n;
        builder.AddValue(n);
        type=T_CHART;
        md5=builder.GetValueCopy();
    }
    virtual ~ChartDescription(){}
    virtual bool isPoint() const { return false;};
    virtual double computeDistance(const Coord::WorldXy &wp){distance=0;return distance;};
    virtual void toJson(json::JSON &js) const{
        ObjectDescription::toJson(js);
        js["Chart"]=chartName;
        js["ChartSet"]=setName;
    }
    virtual void jsonOverview(json::JSON &js) const{
        ObjectDescription::jsonOverview(js);
        js["Chart"]=chartName;
        js["ChartSet"]=setName;
    }
};

typedef std::vector<ObjectDescription::Ptr> ObjectList;
#endif
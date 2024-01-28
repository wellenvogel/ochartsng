/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S57
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
#ifndef _S57_H
#define _S57_H
#include "Types.h"
#include "map"
class S57ObjectClass{
    public:
    uint16_t   Code;
    const char * ObjectClass;
    const char * Acronym;
    typedef enum{
        G,
        M,
        C,
        $,
        O,
        U //unknown
    } ClassType;
    const ClassType   Class;
    S57ObjectClass(uint16_t c,const char *oc, const char *ay, const ClassType cl):
        Code(c),ObjectClass(oc),Acronym(ay),Class(cl){}
    typedef std::map<uint16_t,const S57ObjectClass*> IdMap;
    typedef std::map<String,uint16_t> NameMap;    
};

class S57ObjectClassesBase{
    protected:
    static const S57ObjectClass::IdMap *idMap;
    static const S57ObjectClass::NameMap *nameMap;
    public:
    static const S57ObjectClass* getObjectClass(uint16_t id){
        auto it=idMap->find(id);
        if (it == idMap->end()) return nullptr;
        return it->second;
    }
    static uint16_t getId(const String name){
        auto it=nameMap->find(name);
        if (it == nameMap->end()) return 0;
        return it->second;
    }
};

class S57AttributeDescription{
    public:
    uint16_t Code;
    const char * Attribute;
    const char * Acronym;
    typedef enum{
        A,
        E,
        L,
        I,
        F,
        S
    } Type;
    const Type Attributetype;
    
    S57AttributeDescription(uint16_t c,const char *at, const char *ac, const Type ty):
        Code(c),Attribute(at),Acronym(ac),Attributetype(ty){}
    typedef std::map<uint16_t,const S57AttributeDescription*> IdMap;
    typedef std::map<const String,uint16_t> NameMap;    
};
class S57AttrIdsBase{
    protected:
    static const S57AttributeDescription::IdMap *idMap;
    static const S57AttributeDescription::NameMap *nameMap;
    public:
    static const S57AttributeDescription *getDescription(uint16_t id){
        auto it=idMap->find(id);
        if (it == idMap->end()) return nullptr;
        return it->second;
    }
    static uint16_t getId(const String &name){
        auto it=nameMap->find(name);
        if (it == nameMap->end()) return 0;
        return it->second;
    }

};

class S57AttrValueDescription{
    public:
    uint16_t attrid;
    uint16_t value;
    String strValue;
    using Key=std::pair<uint16_t,uint16_t>;
    using IdMap=std::map<Key,const S57AttrValueDescription*>;
    S57AttrValueDescription(uint16_t ai,uint16_t vi, const char * str):
        attrid(ai),value(vi),strValue(str){}
    String getString(bool withVal=true) const{
        if (! withVal) return strValue;
        String rt(strValue);
        rt.push_back('(');
        rt.append(std::to_string(value));
        rt.push_back(')');
        return rt;
    }
};
class S57AttrValuesBase{
    static const S57AttrValueDescription::IdMap *idMap;
    public:
    static const S57AttrValueDescription *getDescription(uint16_t aid, uint16_t v){
        auto it=idMap->find(S57AttrValueDescription::Key(aid,v));
        if (it == idMap->end()) return nullptr;
        return it->second;
    }
};
#endif
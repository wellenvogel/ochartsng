/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  User settings
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2023 by Andreas Vogel   *
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

#ifndef _USERSETTINGSBASE_H
#define _USERSETTINGSBASE_H
#include <vector>
#include "Types.h"
#include "Exception.h"
#include "json.hpp"
#include "StringHelper.h"
#include "RenderSettings.h"
#define IV(...) std::vector<int>({__VA_ARGS__})
#define US_SETTER(type,field) [](RenderSettings *s, type v){s->field=v;}
#define US_GETTER(type,field) [](const RenderSettings *s)->type{return s->field;}

class UserSettingsEntry{
    public:
    DECL_EXC(AvException,InvalidValueException);
    using Type=enum{
        TYPE_BOOL,
        TYPE_ENUM,
        TYPE_INT,
        TYPE_DOUBLE,
        TYPE_VERSION
    };

        String path;
        String name;
        Type type;
    virtual void setFromJson(RenderSettings* s, const json::JSON &value) =0;
    virtual void storeToJson(const RenderSettings *s, json::JSON &base) =0;
    virtual void addToMd5(const RenderSettings *s,MD5 *md5)=0;
    template <typename T>
    bool getJsonValue(const json::JSON &jv,T &v){
        bool ok=false;
        v =(T)jv.ToInt(ok);
        return ok;
    }
    bool getJsonValue(const json::JSON &jv,double &v){ 
        bool ok=false;
        v= jv.ToFloat(ok);
        if (ok) return ok;
        //be somehow tolerant and accept int values for doubles
        v= jv.ToInt(ok);
        return ok;
    }
    bool getJsonValue(const json::JSON &jv,bool &v)    {
        bool ok=false;
        v= jv.ToBool(ok);
        return ok;
    }
    protected:
    UserSettingsEntry(const String &p, const String &n, const Type &t):
        path(p),name(n),type(t){}
};
class UserSettingsEntryVersion: public UserSettingsEntry{
    String version;
    public:
    UserSettingsEntryVersion(const String &v):
        UserSettingsEntry("","version",UserSettingsEntry::TYPE_VERSION),version(v){}
    virtual void setFromJson(RenderSettings* s, const json::JSON &value){};
    virtual void storeToJson(const RenderSettings *s, json::JSON &base){};
    virtual void addToMd5(const RenderSettings *s,MD5 *md5){
        md5->AddValue(version);
    };
};
class UserSettingsEntryBool : public UserSettingsEntry{
    public:
    using Setter=std::function<void(RenderSettings *s, bool v)>;
    using Getter=std::function<bool(const RenderSettings *s)>;
    Setter setter;
    Getter getter;
    UserSettingsEntryBool(const String &p, const String &n, const Type &t,
        Getter g, Setter s):
        UserSettingsEntry(p,n,t),getter(g),setter(s){}
    virtual void setFromJson(RenderSettings* s,const json::JSON &value) override{
        bool v=false;
        if (! getJsonValue(value,v)) throw new InvalidValueException("no boolean");
        setter(s,v);    
    }
    virtual void storeToJson(const RenderSettings *s, json::JSON &base) override{
        bool v=getter(s);
        base[name]=v;
    }
    virtual void addToMd5(const RenderSettings *s,MD5 *md5){
        bool v=getter(s);
        MD5_ADD_VALUEP(md5,v);
    }
};
template <typename T>
class UserSettingsEntryMinMax : public UserSettingsEntry{
    public:
    T min=0;
    T max=0;
    using Setter=std::function<void(RenderSettings *s, T v)>;
    using Getter=std::function<T(const RenderSettings *s)>;
    Setter setter;
    Getter getter;
    virtual void setFromJson(RenderSettings* s,const json::JSON &value) override{
        T v=-1;
        if (! getJsonValue(value,v)) throw  InvalidValueException("invalid type");
        if (v < min || v > max) throw InvalidValueException(FMT("value %f out of range %f...%f",v,min,max));
        setter(s,v);
    }
    virtual void storeToJson(const RenderSettings *s, json::JSON &base) override{
        T v=getter(s);
        base[name]=v;
    }
    virtual void addToMd5(const RenderSettings *s,MD5 *md5){
        T v=getter(s);
        MD5_ADD_VALUEP(md5,v);
    }
    UserSettingsEntryMinMax(const String &p, const String &n, const Type &t, T mi, T ma,
        Getter g, Setter s):
        UserSettingsEntry(p,n,t),min(mi),max(ma),
        setter(s),getter(g){
        }
};

using UserSettingsEntryDouble=UserSettingsEntryMinMax<double>;
using UserSettingsEntryInt=UserSettingsEntryMinMax<int>;

template <typename T>
class UserSettingsEntryEnum : public UserSettingsEntry{
    public:
    std::vector<int> allowed;
    using Setter=std::function<void(RenderSettings *s, T v)>;
    using Getter=std::function<T(const RenderSettings *s)>;
    Setter setter;
    Getter getter;
    virtual void setFromJson(RenderSettings* s,const json::JSON &value) override{
        int v=-1;
        if (! getJsonValue(value,v)) throw new InvalidValueException("no int value");
        for (auto it=allowed.begin();it != allowed.end();it++){
            if (*it == v) {
                setter(s,(T)v);
                return;
            }
        }
        throw new InvalidValueException("unknown enum value");
    }
    virtual void storeToJson(const RenderSettings *s, json::JSON &base) override{
        T v=getter(s);
        base[name]=(int)v;
    }
    virtual void addToMd5(const RenderSettings *s,MD5 *md5){
        int v=getter(s);
        MD5_ADD_VALUEP(md5,v);
    }
    UserSettingsEntryEnum(const String &p, const String &n, const Type &t, std::vector<int> allowed,
        Getter g, Setter s):
        UserSettingsEntry(p,n,t),getter(g),setter(s){
            this->allowed=allowed;
        }
};

class UserSettingsEntryDetail : public UserSettingsEntry{
    public:
    int id;
    UserSettingsEntryDetail(const String &p, const String &n, const Type &t,int i):
        UserSettingsEntry(p,n,t),id(i){}
    virtual void setFromJson(RenderSettings* s,const json::JSON &value) override{
        bool v=false;
        if (! getJsonValue(value,v)) throw new InvalidValueException("no bool value");
        s->setVisibility(id,v);
    }
    virtual void storeToJson(const RenderSettings *s, json::JSON &base) override{
        bool v=s->getVisibility(id);
        base[name]=v;
    }
    virtual void addToMd5(const RenderSettings *s,MD5 *md5){
        bool v=s->getVisibility(id);
        MD5_ADD_VALUEP(md5,v);
    }
};
using UserSettingsList=std::vector<UserSettingsEntry *>;
#endif
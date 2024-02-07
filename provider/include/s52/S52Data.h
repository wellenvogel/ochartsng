/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S52 data
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
#ifndef _S52DATA_H
#define _S52DATA_H
#include "Types.h"
#include "S52Types.h"
#include "MD5.h"
#include <memory>
#include "Testing.h"
#include "StringHelper.h"
#include "Logger.h"
#include "DrawingContext.h"
#include "Timer.h"
#include "FontManager.h"
#include "OcAllocator.h"
#include "S52SymbolCache.h"
#include "StatusCollector.h"
#include "RenderSettings.h"
#include "MD5.h"
namespace s52
{       
    class Attribute{
        public:
        typedef enum{
            T_INT,
            T_STRING,
            T_DOUBLE,
            T_ANY

        } Type;
        protected:
        Type type=T_STRING;
        ocalloc::String sv;
        uint32_t iv=0;
        double dv=0;
        uint16_t typeCode=0;
        public:
        class Map : public ocalloc::Map<uint16_t,Attribute>{
            public:
            using ocalloc::Map<uint16_t,Attribute>::Map;
            bool hasAttr(uint16_t id,Attribute::Type type=Attribute::T_ANY) const;
            double getDouble(uint16_t id) const;
            bool getDouble(uint16_t id, double &v) const{
                if (hasAttr(id,T_DOUBLE)) {
                    v=getDouble(id);
                    return true;
                }
                return false;
            }
            int getInt(uint16_t id) const;
            bool getInt(uint16_t id, int &v) const{
                if (hasAttr(id,T_INT)){
                    v=getInt(id);
                    return true;
                }
                return false;
            }
            String getString(uint16_t id, bool convert=false) const;
            bool getString(uint16_t id, String &v) const{
                if (hasAttr(id,T_STRING)){
                    v=getString(id);
                    return true;
                }
                return false;
            }
            StringVector getList(uint16_t id) const;
            //return a string that contains the int values
            //as bytes so that we can use strpbrk
            String getParsedList(uint16_t id) const;
            bool listContains(uint16_t id,const StringVector &values) const;
        };
        Attribute(ocalloc::PoolRef &p):sv(p){}
        Attribute(ocalloc::PoolRef &p,uint16_t tc, uint32_t v)
            :typeCode(tc),type(T_INT),iv(v),sv(p){}
        Attribute(ocalloc::PoolRef &p,uint16_t tc,double v)
            :typeCode(tc),type(T_DOUBLE),dv(v),sv(p){}
        Attribute(ocalloc::PoolRef &p,uint16_t tc,const char * v, size_t len):
            typeCode(tc),type(T_STRING),sv(v,len,p){}
        
        bool operator == (const Attribute &other) const;
        bool operator != (const Attribute &other) const{
            return !(other == (*this));
        }
        /**
         * compare with an attribute value from a lookup rule
        */
        bool equals(const String &os) const;

        Type getType() const { return type;}

        void addToMd5(MD5 &md5) const{
            md5.AddValue(type);
            switch (type)
            {
            case T_DOUBLE:
                md5.AddValue(dv);
                break;
            case T_INT:
                md5.AddValue(iv);
                break;
            case T_STRING:
                md5.AddValue(sv);
                break;
            default:
                break;
            }
        }
        String getString(bool convert=false) const
        {
            if (type != T_STRING && !convert)
            {
                throw AvException(FMT("attribute %d is no String but %d", typeCode,type));
            }
            if (type == T_STRING)
            {
                return String(sv.c_str());
            }
            if (type == T_INT)
            {
                return std::to_string(iv);
            }
            if (type == T_DOUBLE)
            {
                char buf[30];
                snprintf(buf,30,"%.8g",dv);
                buf[29]=0;
                return String(buf);
            }
            return "";
        }
        int getInt() const{
            if (type != T_INT)
            {
                throw AvException(FMT("attribute %d is no int but %d", typeCode,type));
            }
            return iv;
        }
        double getDouble() const{
            if (type != T_DOUBLE)
            {
                throw AvException(FMT("attribute %d is no double but %d", typeCode,type));
            }
            return dv;
        }
    };

    typedef enum {
            RS_AREAS1=1, //AC rules only
            RS_AREAS2=2, //rules from LUP table PLAIN_BOUNDARIES/SYMBOLIZED_BOUNDARIES
            RS_LINES=3,  //rules from table LINES
            RS_POINTS=4  //rules from table PAPER_CHART/SIMPLIFIED
        } RenderStep; 
    class LUPrec
    {
    public:
        typedef std::map<uint16_t,String> AttributeMap;        
        int RCID;                 // record identifier
        String  OBCL;             // Name (6 char) '\0' terminated
        Object_t FTYP;            // 'A' Area, 'L' Line, 'P' Point
        DisPrio DPRI;             // Display Priority
        RadPrio RPRI;             // 'O' or 'S', Radar Priority
        LUPname TNAM;             // FTYP:  areas, points, lines
        AttributeMap ATTCArray;   // ArrayString of LUP Attributes
        String INST;              // Instruction Field (rules)
        DisCat DISC;              // Display Categorie: D/S/O, DisplayBase, Standard, Other
        int LUCM;                 // Look-Up Comment (PLib3.x put 'groupes' here,
                                  // hence 'int', but its a string in the specs)
        int nSequence;            // A sequence number, indicating order of encounter in
                                  //  the PLIB file
        RuleList ruleList;        // rasterization rule list
        RenderStep step;          //one of RS_AREAS2,RS_LINES,RS_POINTS
        int attributeMatch(const Attribute::Map *objectAttributes) const;
    };
    typedef std::vector<LUPrec> LUPRecords;
    typedef std::map<LUPname,LUPRecords> LUPTables;
    
    typedef std::map<String, RGBColor> ColorMap;
    class ColorTable
    {
        public:
        typedef std::shared_ptr<ColorTable> Ptr;
        String tableName;
        String rasterFileName;
        ColorMap colors;
        ColorTable(){}
        ColorTable(String n):tableName(n){}
    };

    class S52ObjectClass{
        public:
        uint16_t Code=0;
        String Acronym;
    };
    class S52AttributeDescription{
        public:
        uint16_t Code=0;
        String Acronym;
    };
    
    const String LS_PREFIX="ls:"; //line style symbol prefix
    const String PT_PREFIX="pt:"; //pattern symbol prefix
    class DisplayString{
        public:
        ocalloc::String value;
        int pivotX=0;
        int pivotY=0;
        bool valid=false;
        DrawingContext::ColorAndAlpha color;
        Coord::PixelBox relativeExtent; //relative to the objects xy
        DisplayString(ocalloc::PoolRef p):value(p){}
        DisplayString(const DisplayString &o):value(o.value){
            pivotX=o.pivotX;
            pivotY=o.pivotY;
            valid=o.valid;
            color=o.color;
            relativeExtent=o.relativeExtent;
        }
        DisplayString & operator=(const DisplayString &o){
            value=o.value;
            pivotX=o.pivotX;
            pivotY=o.pivotY;
            valid=o.valid;
            color=o.color;
            relativeExtent=o.relativeExtent;
            return *this;
        } 
    };

    class Arc
        {
        public:
            DrawingContext::ColorAndAlpha cOutline;
            int outlineWidth;
            DrawingContext::ColorAndAlpha cArc;
            int arcWidth;
            double sectr1;
            double sectr2;
            int arcRadius; //pixel
            int sectorRadius; //pixel
            Coord::PixelBox relativeExtent;
            bool valid=false;
        };

    class PrivateRules{
        public:
        static constexpr const char * PR_SOUND() {return "@S";}
        static constexpr const char * PR_LIGHT() { return "CA";}
        static constexpr const char * PR_CAT() { return "XC";} //display category
    };

    class S52Data : public StatusCollector
    {
    public:

        typedef std::shared_ptr<S52Data> Ptr;
        typedef std::shared_ptr<const S52Data> ConstPtr;
        S52Data(RenderSettings::ConstPtr settings,FontFileHolder::Ptr fontFile, int s=0);
        ~S52Data();
        void init(const String &dataDir);
        bool hasAttribute(int code) const;
        uint16_t getAttributeCode(const String &name) const;
        const LUPrec *findLUPrecord(const LUPname &name,uint16_t featureTypeCode,const Attribute::Map *attributes=NULL) const;
        void addColorTable(const ColorTable &table);
        RGBColor getColor(const String &color) const;
        DrawingContext::ColorAndAlpha convertColor(const RGBColor &c)const {
            return DrawingContext::convertColor(c.R,c.G,c.B);
        }
        void addLUP(const LUPrec &lup);
        void addSymbol(const String &name, const SymbolPosition &symbol);
        void addVectorSymbol(const String &name, const VectorSymbol &symbol);
        void buildRules();
        const SymbolPtr getSymbol(const String &name, int rotation=0, double scale=-1) const;
        String checkSymbol(const String &name) const;
        TESTVIRT MD5Name getMD5() const;
        TESTVIRT int getSequence() const;
        TESTVIRT RenderSettings::ConstPtr getSettings() const { return renderSettings;}
        void timerAdd(const String &item){
            timer.add(item);
        }
        using FontType=enum {
            FONT_TXT=0,
            FONT_SOUND=1
        };
        FontManager::Ptr getFontManager(FontType type, int size=16) const;
        /**
         * convert sounding if the attrid matches
         * 0 - always convert
        */
        double convertSounding(double valMeters, uint16_t attrid=0) const;
    protected:
        std::unique_ptr<ocalloc::Pool> pool;
        ocalloc::PoolRef pref;
        bool initialized = false;
        bool froozenRules=false;
        bool froozenSymbols=false;
        typedef std::map<String,ColorTable> ColorTables;
        typedef std::map<uint16_t,S52ObjectClass> ObjectClasses;
        typedef std::map<uint16_t,S52AttributeDescription> AttributeMap;
        typedef std::map<String,S52AttributeDescription> InvAttributeMap;
        typedef std::map<String,SymbolPosition> SymbolMap;
        ColorTables colorTables;
        LUPTables lupTables;
        RenderSettings::ConstPtr renderSettings;
        SymbolMap symbolMap;
        void createRulesForLup(LUPrec &lup);
        void createRasterSymbols(const String &dataDir);
        RuleCreator *ruleCreator;
        Timer::Measure timer;
        using FontManagers=std::map<int,FontManager::Ptr>;
        FontManagers fontManagers;
        SymbolCache::Ptr symbolCache;
        virtual bool LocalJson(StatusStream &stream);
        int sequence=0;
        FontFileHolder::Ptr fontFile;
    };
}

#endif
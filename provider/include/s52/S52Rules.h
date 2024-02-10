/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S52 rules
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
#ifndef _S52RULES_H
#define _S52RULES_H
#include "S52Data.h"
#include "StringHelper.h"
#include "Logger.h"
#include "S52Data.h"
#include "S52TextParser.h"
#include "Coordinates.h"
#include "DrawingContext.h"
namespace s52
{
    
    using RuleType= enum _RuleType
    {
        RUL_NONE,   // no rule type (init)
        RUL_TXT_TX, // TX
        RUL_TXT_TE, // TE
        RUL_SYM_PT, // SY
        RUL_SIM_LN, // LS
        RUL_COM_LN, // LC
        RUL_ARE_CO, // AC
        RUL_ARE_PA, // AP
        RUL_CND_SY, // CS
        RUL_MUL_SG, // Multipoint Sounding
        RUL_SIN_SG, // single sounding
        RUL_ARC_2C,  // Circular Arc, used for sector lights, opencpn private
        RUL_XC_CAT  //change display category to base
    };

    class RuleCreator;

    class Rule{
        public:
        RuleType type=RUL_NONE;
        ocalloc::String parameter;
        uint32_t key=0;
        static const constexpr RuleType TC(){return RUL_NONE;}
        virtual ~Rule(){}
        template <typename T>
        const T* cast(bool doThrow=true) const{
            if (T::TC() != type) {
                if (doThrow) throw AvException(FMT("rule type mismatch, expected %d, got %d",T::TC(),type));
                return nullptr;
            }
            return (T*)this;
        }
        bool isValid() const{ return valid;}
        protected:
        std::size_t sz=0;
        std::size_t al=0;
        bool valid=false;
        Rule(ocalloc::PoolRef pr,RuleType t, bool v=true):type(t),valid(v),parameter(pr){}
        friend class RuleCreator;
    };

    class RuleConditions
    {
    public:
        GeoPrimitive geoPrimitive=GEO_UNSET;
        bool hasFloatingBase = false;            // if there is a floating boy/light at this position
        const Attribute::Map *attributes = NULL; // object attributes
        double nextSafetyContour=1e6;
        uint16_t featureTypeCode=0;
    };
    /**
     * factory for rules
     * it will keep track of created rules
     * and will delete them when destroyed
    */
    class RuleCreator{
        protected:
        using RulePtr=Rule*;
        ocalloc::PoolRef pool;
        static const constexpr int MAXRUL=1024*1024;
        ocalloc::Vector<RulePtr> ruleList;
        uint32_t currentKey=0;
        public:
            RuleCreator(ocalloc::PoolRef pr,uint32_t keyFactor):pool(pr),ruleList(pr){
                currentKey=keyFactor*MAXRUL;
            }
            ~RuleCreator(){
                for (auto it=ruleList.begin();it != ruleList.end();it++){
                    std::size_t sz=(*it)->sz;
                    std::size_t al=(*it)->al;
                    (*it)->~Rule();
                    pool.deallocate(*it,sz);
                }
            }
            template<typename T, typename ...Args>
            const T* create(const String &param,Args&& ...args){
                void *mem=pool.allocate(sizeof(T),alignof(T));
                T* rt=new (mem) T(pool,std::forward<Args>(args)...);
                rt->sz=sizeof(T);
                rt->al=alignof(T);
                rt->parameter.assign(param.c_str());
                rt->key=currentKey++;
                ruleList.push_back(rt);
                return rt;    
            }
            void rulesFromString(const LUPrec *lup,const String &ruleStr,const S52Data *s52data,std::function<void (const s52::Rule *)>,bool tryExpansion,const RuleConditions *conditions=NULL);
    };

    template<s52::RuleType id>
    class GenericRule: public Rule{
        public:
        static const constexpr RuleType TC(){return id;}
        protected:
        GenericRule(ocalloc::PoolRef pr):
            Rule(pr,id){}
        public:
        friend class RuleCreator;
    };
    class AreaRule : public GenericRule<RUL_ARE_CO>{
        public:
        RGBColor color;
        protected:
        AreaRule(ocalloc::PoolRef pr,const RGBColor &c):GenericRule(pr),color(c){}
        friend class RuleCreator;
    };
    class SymAreaRule : public GenericRule<RUL_ARE_PA>{
        public:
        SymbolPtr symbol;
        protected:
        SymAreaRule(ocalloc::PoolRef pr,SymbolPtr s):GenericRule(pr),symbol(s){}
        friend class RuleCreator;
    };
    using CondRule =GenericRule<RUL_CND_SY>;
    class SymbolRule : public GenericRule<RUL_SYM_PT>{
        protected:
        SymbolRule(ocalloc::PoolRef pr,const String name):
            GenericRule(pr),finalName(name){
            }
        public:
        String finalName;
        friend class RuleCreator;
    };

    
    class StringTERule: public GenericRule<RUL_TXT_TE>{
        public:
        StringOptions options;
        protected:
        StringTERule(ocalloc::PoolRef pr,const StringOptions &o):options(o),
            GenericRule(pr){}
        public:
        friend class RuleCreator;
    };
    class StringTXRule: public GenericRule<RUL_TXT_TX>{
        public:
        StringOptions options;
        protected:
        StringTXRule(ocalloc::PoolRef pr,const StringOptions &o):options(o),
            GenericRule(pr){}
        public:
        friend class RuleCreator;
    };

    using SoundingRule=GenericRule<RUL_MUL_SG>;
    using SingleSoundingRule=GenericRule<RUL_SIN_SG>;
    using CARule=GenericRule<RUL_ARC_2C>;
    using SimpleLineRule=GenericRule<RUL_SIM_LN>;
    using DisCatRule=GenericRule<RUL_XC_CAT>;
    class SymbolLineRule : public GenericRule<RUL_COM_LN>{
        public:
        friend class RuleCreator;
        protected:
        SymbolLineRule(ocalloc::PoolRef pr,const String  &p):
            GenericRule(pr),symbol(p){}
        public:
        String symbol;
    };  
}

#endif
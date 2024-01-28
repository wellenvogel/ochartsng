/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S52 conditional rules
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
#include "S52CondRules.h"
#include "generated/S57ObjectClasses.h"
#include "generated/S57AttributeIds.h"
#include "S52Rules.h"
#include "StringHelper.h"
#include "Logger.h"
#include <algorithm>
#include <string.h>
namespace s52
{
    String S52CondRules::expand(const LUPrec *lup, const String &rule,const S52Data *s52data, const RuleConditions *conditions){
        String name=rule.substr(3);
        StringHelper::replaceInline(name,")","");
        auto it=rules.find(name);
        if (it == rules.end()) {
            LOG_DEBUG("unknown cond rule %s",rule);
            return rule;
        }
        if (it->second.type == Rule::T_STEP2 && conditions == nullptr) return rule;
        return it->second.function(lup,rule,s52data,conditions);    
    }

    static String DEPARE01(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)
    {
        if (!conditions){
            return rule;
        }
        const Attribute::Map *objectAttributes=conditions->attributes;
        if (!objectAttributes){
            return rule;
        }    
        const RenderSettings *settings = s52data->getSettings().get();
        if (!settings)
        {
            LOG_DEBUG("invalid rule call for DEPARE01 - no settings");
            return rule;
        }
        double drval1 = -1.0;
        bool drval1Found=false;
        // DRVAL1=87
        drval1Found = objectAttributes->getDouble(S57AttrIds::DRVAL1,drval1);
        double drval2 = drval1 + 0.01;
        // DRVAL2=88
        objectAttributes->getDouble(S57AttrIds::DRVAL2,drval2);
        bool shallow = true;
        String rt = "AC(DEPIT)";
        if (drval1 >= 0.0 && drval2 > 0.0)
        {
            rt = "AC(DEPVS)";
        }
        if (settings->S52_MAR_TWO_SHADES)
        {
            if (drval1 >= settings->S52_MAR_SAFETY_CONTOUR &&
                drval2 > settings->S52_MAR_SAFETY_CONTOUR)
            {
                rt = "AC(DEPDW)";
                shallow = false;
            }
        }
        else
        {
            if (drval1 >= settings->S52_MAR_SHALLOW_CONTOUR &&
                drval2 > settings->S52_MAR_SHALLOW_CONTOUR)
                rt = "AC(DEPMS)";

            if (drval1 >= settings->S52_MAR_SAFETY_CONTOUR &&
                drval2 > settings->S52_MAR_SAFETY_CONTOUR)
            {
                rt = "AC(DEPMD)";
                shallow = false;
            }

            if (drval1 >= settings->S52_MAR_DEEP_CONTOUR &&
                drval2 > settings->S52_MAR_DEEP_CONTOUR)
            {
                rt = "AC(DEPDW)";
                shallow = false;
            }
        }

        //  If object is DRGARE....

        if (lup->OBCL=="DRGARE")
        {
            if (!drval1Found) // If DRVAL1 was not defined...
            {
                rt="AC(DEPMD)";
                shallow = false;
            }
            rt.append(";AP(DRGARE01)");
            rt.append(";LS(DASH,1,CHGRF)");

            // Todo Restrictions
            /*
                    char pval[30];
                    if(true == GetStringAttr(obj, "RESTRN", pval, 20))
                    {
                        GString *rescsp01 = _RESCSP01(geo);
                        if (NULL != rescsp01)
                        {
                            g_string_append(depare01, rescsp01->str);
                            g_string_free(rescsp01, TRUE);
                        }
                    }
            */
        }
        rt.push_back('\037');
        return rt;
    }


    static String dummy1(const LUPrec *,const String &rule,const S52Data *s52data, const RuleConditions *conditions){
    return rule;
    }
    static String SOUNDG02(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions){
        return "MP();";
    }
    static String TOPMAR01(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions){
        if (! conditions || ! conditions->attributes){
            return rule;
        }
        if (! conditions->attributes->hasAttr(S57AttrIds::TOPSHP /*TOPSHP*/,s52::Attribute::T_INT)){
            LOG_DEBUG("invalid TOPMAR01 in %d, no TOPSHP attr in object",lup->RCID);
            return rule;
        }
        int topshp=conditions->attributes->getInt(S57AttrIds::TOPSHP);
        if (conditions->hasFloatingBase){
            switch (topshp) {
                case 1 : return "SY(TOPMAR02)"; break;
                case 2 : return "SY(TOPMAR04)"; break;
                case 3 : return "SY(TOPMAR10)"; break;
                case 4 : return "SY(TOPMAR12)"; break;

                case 5 : return "SY(TOPMAR13)"; break;
                case 6 : return "SY(TOPMAR14)"; break;
                case 7 : return "SY(TOPMAR65)"; break;
                case 8 : return "SY(TOPMAR17)"; break;

                case 9 : return "SY(TOPMAR16)"; break;
                case 10: return "SY(TOPMAR08)"; break;
                case 11: return "SY(TOPMAR07)"; break;
                case 12: return "SY(TOPMAR14)"; break;

                case 13: return "SY(TOPMAR05)"; break;
                case 14: return "SY(TOPMAR06)"; break;
                case 17: return "SY(TMARDEF2)"; break;
                case 18: return "SY(TOPMAR10)"; break;

                case 19: return "SY(TOPMAR13)"; break;
                case 20: return "SY(TOPMAR14)"; break;
                case 21: return "SY(TOPMAR13)"; break;
                case 22: return "SY(TOPMAR14)"; break;

                case 23: return "SY(TOPMAR14)"; break;
                case 24: return "SY(TOPMAR02)"; break;
                case 25: return "SY(TOPMAR04)"; break;
                case 26: return "SY(TOPMAR10)"; break;

                case 27: return "SY(TOPMAR17)"; break;
                case 28: return "SY(TOPMAR18)"; break;
                case 29: return "SY(TOPMAR02)"; break;
                case 30: return "SY(TOPMAR17)"; break;

                case 31: return "SY(TOPMAR14)"; break;
                case 32: return "SY(TOPMAR10)"; break;
                case 33: return "SY(TMARDEF2)"; break;
                default: return "SY(TMARDEF2)"; break;
            }
        }
        else{
            switch (topshp) {
                case 1 : return "SY(TOPMAR22)"; break;
                case 2 : return "SY(TOPMAR24)"; break;
                case 3 : return "SY(TOPMAR30)"; break;
                case 4 : return "SY(TOPMAR32)"; break;

                case 5 : return "SY(TOPMAR33)"; break;
                case 6 : return "SY(TOPMAR34)"; break;
                case 7 : return "SY(TOPMAR85)"; break;
                case 8 : return "SY(TOPMAR86)"; break;

                case 9 : return "SY(TOPMAR36)"; break;
                case 10: return "SY(TOPMAR28)"; break;
                case 11: return "SY(TOPMAR27)"; break;
                case 12: return "SY(TOPMAR14)"; break;

                case 13: return "SY(TOPMAR25)"; break;
                case 14: return "SY(TOPMAR26)"; break;
                case 15: return "SY(TOPMAR88)"; break;
                case 16: return "SY(TOPMAR87)"; break;

                case 17: return "SY(TMARDEF1)"; break;
                case 18: return "SY(TOPMAR30)"; break;
                case 19: return "SY(TOPMAR33)"; break;
                case 20: return "SY(TOPMAR34)"; break;

                case 21: return "SY(TOPMAR33)"; break;
                case 22: return "SY(TOPMAR34)"; break;
                case 23: return "SY(TOPMAR34)"; break;
                case 24: return "SY(TOPMAR22)"; break;

                case 25: return "SY(TOPMAR24)"; break;
                case 26: return "SY(TOPMAR30)"; break;
                case 27: return "SY(TOPMAR86)"; break;
                case 28: return "SY(TOPMAR89)"; break;

                case 29: return "SY(TOPMAR22)"; break;
                case 30: return "SY(TOPMAR86)"; break;
                case 31: return "SY(TOPMAR14)"; break;
                case 32: return "SY(TOPMAR30)"; break;
                case 33: return "SY(TMARDEF1)"; break;
                default: return "SY(TMARDEF1)"; break;
            }
        }
    }

    static String CSQUAPNT01(const Attribute::Map *attributes)
    // Remarks: The attribute QUAPOS, which identifies low positional accuracy, is attached
    // only to the spatial component(s) of an object.
    //
    // This procedure retrieves any QUALTY (ne QUAPOS) attributes, and returns the
    // appropriate symbols to the calling procedure.

    {
        String quapnt01;
        bool accurate = true;
        int qualty = 10;
        bool bquapos = attributes->getInt(S57AttrIds::QUALTY, qualty);
        if (bquapos)
        {
            if (2 <= qualty && qualty < 10)
                accurate = false;
        }
        if (!accurate)
        {
            switch (qualty)
            {
            case 4:
                quapnt01.append(";SY(QUAPOS01)");
                break; // "PA"
            case 5:
                quapnt01.append(";SY(QUAPOS02)");
                break; // "PD"
            case 7:
            case 8:
                quapnt01.append(";SY(QUAPOS03)");
                break; // "REP"
            default:
                quapnt01.append(";SY(LOWACC03)");
                break; // "?"
            }
        }
        return quapnt01;
    }
    // Remarks: Obstructions or isolated underwater dangers of depths less than the safety
    // contour which lie within the safe waters defined by the safety contour are
    // to be presented by a specific isolated danger symbol and put in IMO
    // category DISPLAYBASE (see (3), App.2, 1.3). This task is performed
    // by the sub-procedure "UDWHAZ03" which is called by this symbology
    // procedure. Objects of the class "under water rock" are handled by this
    // routine as well to ensure a consistent symbolization of isolated dangers on
    // the seabed.
    static String OBSTRN04(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions){
        if (! conditions || ! conditions->attributes) return rule;
        String sounding;
        String rt;
        double safetyDepth=s52data->getSettings()->S52_MAR_SAFETY_CONTOUR;
        double depthValueAttr;
        double depthValue;
        bool hasDepth=conditions->attributes->getDouble(S57AttrIds::VALSOU,depthValueAttr);
        if (hasDepth){
            //create a special "SP" rule (single point sounding)
            sounding=StringHelper::format(";%s(%f)",PrivateRules::PR_SOUND(),depthValueAttr);
            depthValue=depthValueAttr;
        }
        else
        {
            // original _DEPVAL01 does nothing any more - so do not use this
            int catObs = 0;
            conditions->attributes->getInt(S57AttrIds::CATOBS, catObs);
            int watLev = 0;
            conditions->attributes->getInt(S57AttrIds::WATLEV, watLev);
            if (6 == catObs)
                depthValue = 0.01;
            else if (0 == watLev) // default
                depthValue = -15.0;
            else
            {
                switch (watLev)
                {
                case 5:
                    depthValue = 0.0;
                    break;
                case 3:
                    depthValue = 0.01;
                    break;
                case 4:
                case 1:
                case 2:
                default:
                    depthValue = -15.0;
                    break;
                }
            }
        }
        String danger;
        if (depthValue <= safetyDepth){
            //normally we should use the _UDWHAZ03 procedure to check
            //which areas we intersect with
            //danger=";SY(ISODGR51)";
            rt.append(";");
            rt.append(PrivateRules::PR_CAT());
            rt.append("();");
        }
        String qual=CSQUAPNT01(conditions->attributes);
        switch (conditions->geoPrimitive){
            case GEO_POINT:
            {
                if (! danger.empty()){
                    rt.append(danger);
                    rt.append(qual);
                    return rt;
                }
                bool useSounding=false;
                if (hasDepth){
                    if (depthValue <= 20.0){
                        int watLev=-9;
                        conditions->attributes->getInt(S57AttrIds::WATLEV,watLev);
                        if (lup->OBCL == "UWTROC"){
                            switch(watLev){
                                case 4:
                                case 5:
                                    rt.append(";SY(UWTROC04)");
                                    break;
                                default:
                                    //default
                                    rt.append(";SY(DANGER51)");
                                    useSounding=true;
                                    break;
                            }

                        }
                        else{
                            // OBSTRN
                            switch(watLev){
                                case -9:
                                    rt.append(";SY(DANGER01)");
                                    useSounding=true;
                                    break;
                                case 1:
                                case 2:
                                    rt.append(";SY(LNDARE01)");
                                    break;
                                case 3:
                                    rt.append(";SY(DANGER52)");
                                    useSounding=true;
                                    break;
                                case 4:
                                case 5:
                                    rt.append(";SY(DANGER53)");
                                    useSounding=true;
                                    break;
                                default:
                                    rt.append(";SY(DANGER51)");
                                    useSounding=true;
                            }
                        }
                    }
                    else{
                        //depth > 20.0
                        rt.append(";SY(DANGER52)");
                        useSounding=true;
                    }
                }
                else{
                    //no VALSOU attribute
                    int watLev=-9;
                    conditions->attributes->getInt(S57AttrIds::WATLEV,watLev);
                    if (lup->OBCL == "UWTROC"){
                        switch(watLev){
                            case 2:
                                rt.append(";SY(LNDARE01)");
                                break;
                            case 3:
                                rt.append(";SY(UWTROC03)");
                                break;
                            default:
                                rt.append(";SY(UWTROC04)");
                        }                        
                    }
                    else{
                        //OBSTRN
                        switch(watLev){
                            case 1:
                            case 2:
                                rt.append(";SY(OBSTRN11)");
                                break;
                            case 4:
                            case 5:
                                rt.append(";SY(OBSTRN03)");
                                break;
                            default:
                                rt.append(";SY(OBSTRN01)");
                        }
                    }
                }
                if (useSounding){
                    rt.append(sounding);
                }
                rt.append(qual);
                return rt;
            }
            break;
            case GEO_LINE:
            {
                if (! qual.empty()){
                    //some strange code in original at around lines 2078
                    //getting back an int val from this string...
                    if (!danger.empty()){
                        rt.append(";LC(LOWACC41)");
                        return rt;
                    }
                    else{
                        rt.append(";LC(LOWACC31)");
                        return rt;
                    }
                }
                if (!danger.empty()){
                    rt.append("LS(DOTT,2,CHBLK)");
                    return rt;
                }
                if (hasDepth){
                    if (depthValue <= 20.0){
                        rt.append(";LS(DOTT,2,CHBLK)");
                        rt.append(sounding);
                    }
                    else{
                        rt.append(";LS(DASH,2,CHBLK)");
                    }
                }
                else{
                    rt.append(";LS(DOTT,2,CHBLK)");
                }
                return rt;
            }
            break;
            case GEO_AREA:
            {
                if (!danger.empty()){
                    rt.append(";AC(DEPVS);AP(FOULAR01)");
                    rt.append(";LS(DOTT,2,CHBLK)");
                    rt.append(danger);
                    rt.append(qual);
                    return rt;
                }
                if (hasDepth){
                    if (depthValue <= 20.0){
                        rt.append(";LS(DOTT,2,CHBLK)");
                    }
                    else{
                        rt.append(";LS(DASH,2,CHBLK)");
                    }
                    rt.append(sounding);
                }
                else{
                    int watLev=-9;
                    conditions->attributes->getInt(S57AttrIds::WATLEV,watLev);
                    switch(watLev){
                        case 3:
                        {
                            int catObs=-9;
                            conditions->attributes->getInt(S57AttrIds::CATOBS,catObs);
                            if (catObs == 6){
                                rt.append(";AC(DEPVS);AP(FOULAR01);LS(DOTT,2,CHBLK)");
                            }
                        }
                        break;
                        case 1:
                        case 2:
                            rt.append(";AC(CHBRN);LS(SOLD,2,CSTLN)");
                            break;
                        case 4:
                            rt.append(";AC(DEPIT);LS(DASH,2,CSTLN)");
                            break;
                        default:
                            rt.append(";AC(DEPVS);LS(DOTT,2,CHBLK)");    
                    }
                }
                rt.append(qual);
                return rt;
            }
            break;
            default:
            break;
        }
        return rule;
    }

    static String WRECKS02(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions){
        if (! conditions || ! conditions->attributes){
            return rule;
        }
        double safetyDepth=s52data->getSettings()->S52_MAR_SAFETY_CONTOUR;
        double depthValueAttr;
        double depthValue;
        bool hasDepth=conditions->attributes->getDouble(S57AttrIds::VALSOU,depthValueAttr);
        int watLev=-9;
        conditions->attributes->getInt(S57AttrIds::WATLEV,watLev);
        int catWrk=-9;
        conditions->attributes->getInt(S57AttrIds::CATWRK,catWrk);
        String qual=CSQUAPNT01(conditions->attributes);
        String sounding;
        String rt;
        if (hasDepth){
            depthValue=depthValueAttr;
            sounding=StringHelper::format(";%s(%f)",PrivateRules::PR_SOUND(),depthValueAttr);
        }
        else{
            switch (catWrk){
                case 1:
                    depthValue=20.0;
                    break;
                case 2:
                    depthValue=0;
                    break;
                case 4:
                case 5:
                    depthValue=-15.0;
                    break;
                default:
                    {
                        switch(watLev){
                            case 1:
                            case 2:
                                depthValue=-15;
                                break;
                            case 3:
                                depthValue=0.01;
                                break;
                            case 4:
                                depthValue=-15;
                                break;
                            case 5:
                                depthValue=0;
                                break;
                            case 6:
                                depthValue=-15;
                                break;
                            default:
                                depthValue=-15;            
                        }
                    }        
            }
        }
        String danger;
        bool quasou7=conditions->attributes->listContains(S57AttrIds::QUASOU,StringVector({"7"}));
        if (! quasou7 && depthValue <= safetyDepth){
            //TODO: check QUASOU to not contain 7 and use _UDWHAZ03 to get the danger string
            //simplified for now
            //danger=";SY(ISODGR51)";
            rt.append(";");
            rt.append(PrivateRules::PR_CAT());
            rt.append("();");

        }
        switch (conditions->geoPrimitive){
            case s52::GEO_POINT:
            {
                if (! danger.empty()){
                    rt.append(danger);
                    rt.append(qual);
                    return rt;
                }
                if (hasDepth){
                    if (depthValue < safetyDepth){
                        rt.append(";SY(DANGER51)");
                    }
                    else{
                        rt.append(";SY(DANGER52)");
                    }
                    rt.append(";TX('Wk',2,1,2,'15110',1,0,CHBLK,21)");
                    if (quasou7){
                        rt.append(";SY(WRECKS07)");
                    }
                    rt.append(sounding);
                    rt.append(danger);
                    rt.append(qual);
                    return rt;
                }
                else
                {
                    if (-9 != catWrk && -9 != watLev)
                    {
                        if (1 == catWrk && 3 == watLev)
                            rt.append(";SY(WRECKS04)");
                        else
                        {
                            if (2 == catWrk && 3 == watLev)
                                rt.append(";SY(WRECKS05)");
                            else
                            {
                                if (4 == catWrk || 5 == catWrk)
                                    rt.append(";SY(WRECKS01)");
                                else
                                {
                                    if (1 == watLev ||
                                        2 == watLev ||
                                        5 == watLev ||
                                        4 == watLev)
                                    {
                                        rt.append(";SY(WRECKS01)");
                                    }
                                    else
                                        rt.append(";SY(WRECKS05)"); // default
                                }
                            }
                        }
                    }
                    rt.append(qual);
                    return rt;
                }
            }
            break;
            case s52::GEO_AREA:
            {
                int quapos=0;
                conditions->attributes->getInt(S57AttrIds::QUAPOS,quapos);
                if (2 <= quapos && quapos < 10){
                    rt.append(";LC(LOWACC41)");
                }
                else{
                    if (! danger.empty()){
                        rt.append(";LS(DOTT,2,CHBLK)");
                    }
                    else{
                        switch(watLev){
                            case 1:
                            case 2:
                                rt.append(";LS(SOLD,2,CSTLN)");
                                break;
                            case 4:
                                rt.append(";LS(DASH,2,CSTLN)");
                                break;
                            default:
                                rt.append(";LS(DOTT,2,CSTLN)");
                        }
                    }
                }
                if (hasDepth){
                    if (depthValue <= 20){
                        rt.append(danger);
                        rt.append(qual);
                        rt.append(sounding);
                    }
                    else{
                        rt.append(danger);
                        rt.append(qual);
                    }
                }
                else{
                    switch (watLev)
                    {
                    case 1:
                    case 2:
                        rt.append(";AC(CHBRN)");
                        break;
                    case 4:
                        rt.append(";AC(DEPIT)");
                        break;
                    default:
                        rt.append(";AC(DEPVS)");
                        break;
                    }
                    rt.append(danger);
                    rt.append(qual);
                }
                return rt;
            }
            break;
            default:
                break;
        }
        return rt;
    }
    static String lightSymbol(int numColors,const char *colors,bool allRound,double valnmr, int radius=0){
        String rt;
        int outlineWidth=4;
        if (!allRound)
        {
            rt = ";SY(LITDEF11";

            if (numColors == 1)
            {
                if (strpbrk(colors, "\003"))
                {
                    rt = ";SY(LIGHTS11";
                }
                else if (strpbrk(colors, "\004"))
                {
                    rt = ";SY(LIGHTS12";
                }
                else if (strpbrk(colors, "\001\006\011"))
                {
                    rt = ";SY(LIGHTS13";
                }
            }
            else if (numColors == 2)
            {
                if (strpbrk(colors, "\001") && strpbrk(colors, "\003"))
                    rt = ";SY(LIGHTS11";
                else if (strpbrk(colors, "\001") && strpbrk(colors, "\004"))
                    rt = ";SY(LIGHTS12";
            }
        }
        else{
            // all-round fixed light symbolized as a circle, radius depends on color
            // This treatment is seen on SeeMyDenc by SevenCs
            // This may not be S-52 compliant....
            if (numColors == 1){
                if (strpbrk(colors, "\003"))
                    rt=FMT(",LITRD, 2,0,360,%d,0", radius + 1 *outlineWidth);
                else if (strpbrk(colors, "\004"))
                    rt=FMT(",LITGN, 2,0,360,%d,0", radius);
                else if (strpbrk(colors, "\001\006\011"))
                    rt=FMT(",LITYW, 2,0,360,%d,0", radius + 2*outlineWidth);
                else if (strpbrk(colors, "\014"))
                    rt=FMT(",CHMGD, 2,0,360,%d,0", radius + 3*outlineWidth);
                else
                    rt=FMT(",CHMGD, 2,0,360,%d,0", radius + 5*outlineWidth);
            }
            else if (numColors == 2){
                if (strpbrk(colors, "\001") && strpbrk(colors, "\003"))
                      rt=FMT(",LITRD, 2,0,360,%d,0", radius + 1 *outlineWidth);
                else if (strpbrk(colors, "\001") && strpbrk(colors, "\004"))
                      rt=FMT(",LITGN, 2,0,360,%d,0", radius);
                else
                      rt=FMT(",CHMGD, 2,0,360,%d,0", radius + 5*outlineWidth); 
            }
            else{
                rt=FMT(",CHMGD, 2,0,360,%d,0", radius + 5*outlineWidth);
            }
            if (! rt.empty()){
                rt=FMT(";%s(OUTLW, %d",PrivateRules::PR_LIGHT(),outlineWidth)+rt;
            }
        }
        return rt;
    }
    static String _LITDSN01(const S52Data* s52data,const Attribute::Map *attributes)
    {
        // Remarks: In S-57 the light characteristics are held as a series of attributes values. The
        // mariner may wish to see a light description text string displayed on the
        // screen similar to the string commonly found on a paper chart. This
        // conditional procedure, reads the attribute values from the above list of
        // attributes and composes a light description string which can be displayed.
        // This procedure is provided as a C function which has as input, the above
        // listed attribute values and as output, the light description.
        // CATLIT, LITCHR, COLOUR, HEIGHT, LITCHR, SIGGRP, SIGPER, STATUS, VALNMR

        String rt;

        /*
          1: directional function  IP 30.1-3;  475.7;
          2: rear/upper light
          3: front/lower light
          4: leading light           IP 20.1-3;      475.6;
          5: aero light                  IP 60;      476.1;
          6: air obstruction light IP 61;      476.2;
          7: fog detector light        IP 62;  477;
          8: flood light                 IP 63;      478.2;
          9: strip light                 IP 64;      478.5;
          10: subsidiary light          IP 42;  471.8;
          11: spotlight
          12: front
          13: rear
          14: lower
          15: upper
          16: moire' effect           IP 31;    475.8;
          17: emergency
          18: bearing light                   478.1;
          19: horizontally disposed
          20: vertically disposed
        */

        // LITCHR
        int litchr = -9;
        attributes->getInt(S57AttrIds::LITCHR,litchr);
        String spost;
        bool grp2 = false; // 2 GRP attributes expected
        if (-9 != litchr)
        {
            switch (litchr)
            {
                /*
                                  case 1:   rt.append("F"));    break;
                                  case 2:   rt.append("Fl"));   break;
                                  case 3:   rt.append("Fl"));   break;
                                  case 4:   rt.append("Q"));    break;
                                  case 7:   rt.append("Iso"));  break;
                                  case 8:   rt.append("Occ"));  break;
                                  case 12:  rt.append("Mo"));   break;
                */

            case 1:
                rt.append("F");
                break; // fixed     IP 10.1;
            case 2:
                rt.append("Fl");
                break; // flashing  IP 10.4;
            case 3:
                rt.append("LFl");
                break; // long-flashing   IP 10.5;
            case 4:
                rt.append("Q");
                break; // quick-flashing  IP 10.6;
            case 5:
                rt.append("VQ");
                break; // very quick-flashing   IP 10.7;
            case 6:
                rt.append("UQ");
                break; // ultra quick-flashing  IP 10.8;
            case 7:
                rt.append("Iso");
                break; // isophased IP 10.3;
            case 8:
                rt.append("Occ");
                break; // occulting IP 10.2;
            case 9:
                rt.append("IQ");
                break; // interrupted quick-flashing  IP 10.6;
            case 10:
                rt.append("IVQ");
                break; // interrupted very quick-flashing   IP 10.7;
            case 11:
                rt.append("IUQ");
                break; // interrupted ultra quick-flashing  IP 10.8;
            case 12:
                rt.append("Mo");
                break; // morse     IP 10.9;
            case 13:
                rt.append("F + Fl");
                grp2 = true;
                break; // fixed/flash     IP 10.10;
            case 14:
                rt.append("Fl + LFl");
                grp2 = true;
                break; // flash/long-flash
            case 15:
                rt.append("Occ + Fl");
                grp2 = true;
                break; // occulting/flash
            case 16:
                rt.append("F + LFl");
                grp2 = true;
                break; // fixed/long-flash
            case 17:
                rt.append("Al Occ");
                break; // occulting alternating
            case 18:
                rt.append("Al LFl");
                break; // long-flash alternating
            case 19:
                rt.append("Al Fl");
                break; // flash alternating
            case 20:
                rt.append("Al Grp");
                break; // group alternating
            case 21:
                rt.append("F");
                spost =" (vert)";
                break; // 2 fixed (vertical)
            case 22:
                rt.append("F");
                spost = " (horz)";
                break; // 2 fixed (horizontal)
            case 23:
                rt.append("F");
                spost=" (vert)";
                break; // 3 fixed (vertical)
            case 24:
                rt.append("F");
                spost = " (horz)";
                break; // 3 fixed (horizontal)
            case 25:
                rt.append("Q + LFl");
                grp2 = true;
                break; // quick-flash plus long-flash
            case 26:
                rt.append("VQ + LFl");
                grp2 = true;
                break; // very quick-flash plus long-flash
            case 27:
                rt.append("UQ + LFl");
                grp2 = true;
                break; // ultra quick-flash plus long-flash
            case 28:
                rt.append("Alt");
                break; // alternating
            case 29:
                rt.append("F + Alt");
                grp2 = true;
                break; // fixed and alternating flashing

            default:
                break;
            }
        }

        size_t firstGrp;
        if (grp2)
        {
            String ret_new;
            firstGrp = rt.find(" ");
            if (String::npos != firstGrp)
            {
                ret_new = rt.substr(0, firstGrp);
                ret_new.append("(?)");
                ret_new.append(rt.substr(firstGrp));
                rt = ret_new;
                firstGrp += 1;
            }
        }

        // SIGGRP, (c)(c) ...
        String grpStr;
        if (attributes->getString(S57AttrIds::SIGGRP,grpStr))
        {
            if (grp2)
            //we expect something like (1)(2)
            //put the first at the place where we inserted (?)
            {
                StringHelper::replaceInline(grpStr," ","");
                StringVector grpParts=StringHelper::split(grpStr,")(");
                if (grpParts.size() > 1){
                    StringHelper::replaceInline(rt,"(?)",grpParts[0]+")");
                    if (grpParts[1] != "1)"){
                        rt.append("("+grpParts[1]);
                    }
                }
            }
            else
            {
                if (grpStr != "(1)")
                    rt.append(grpStr);
            }
        }

        
        // Don't show for sectored lights since we are only showing one of the sectors.
        double sectrTest;
        bool hasSectors = attributes->getDouble(S57AttrIds::SECTR1, sectrTest);

        //      if( ! hasSectors )
        {
            String colors=attributes->getParsedList(S57AttrIds::COLOUR);
            for (int i = 0; i < colors.size(); i++)
            {
                switch (colors[i])
                {
                case 1:
                    rt.append("W");
                    break;
                case 3:
                    rt.append("R");
                    break;
                case 4:
                    rt.append("G");
                    break;
                case 6:
                    rt.append("Y");
                    break;
                default:
                    break;
                }
            }
        }

        /*
          1: white     IP 11.1;    450.2-3;
          2: black
          3: red IP 11.2;    450.2-3;
          4: green     IP 11.3;    450.2-3;
          5: blue      IP 11.4;    450.2-3;
          6: yellow    IP 11.6;    450.2-3;
          7: grey
          8: brown
          9: amber     IP 11.8;    450.2-3;
          10: violet    IP 11.5;    450.2-3;
          11: orange    IP 11.7;    450.2-3;
          12: magenta
          13: pink
        */

        // SIGPER, xx.xx
        double sigper;
        if (attributes->getDouble(S57AttrIds::SIGPER,sigper))
        {
            if (fabs(roundf(sigper) - sigper) > 0.01)
                rt.append(FMT("%4.1fs", sigper));
            else
                rt.append(FMT("%2.0fs", sigper));
        }

        // HEIGHT, xxx.x
        double height;
        if (attributes->getDouble(S57AttrIds::HEIGHT,height))
        {
            height=s52data->convertSounding(height,0);
            switch (s52data->getSettings()->S52_DEPTH_UNIT_SHOW)
            {
            case DepthUnit::FEET:
                rt.append(FMT("%3.0fft",height));
                break;
            case DepthUnit::FATHOMS:
                rt.append(FMT("%3.0ffm",height));
                break;
            default:
                rt.append(FMT("%3.0fm",height));
                break;
            }
        }

        // VALNMR, xx.x
        double valnmr;
        if (attributes->getDouble(S57AttrIds::VALNMR,valnmr) && !hasSectors)
        {
            rt.append(FMT("%2.0fNm", valnmr));
        }

        rt.append(spost); // add any final modifiers
        return rt;
    }
    static String LIGHTS06(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions){
        if (! conditions || ! conditions->attributes) return "";
        String catLitS=conditions->attributes->getParsedList(S57AttrIds::CATLIT);
        const char *catlit=catLitS.c_str();
        String rt;
        if (catLitS.size()){
            if (strpbrk(catlit,"\010\013")){
                rt.append(";SY(LIGHTS82)");
            }
            else if (strpbrk(catlit, "\011")){
                rt.append(";SY(LIGHTS81)");
            }
        }
        String colors=conditions->attributes->getParsedList(S57AttrIds::COLOUR);
        if (! colors.size()){
            colors="\014";
        }
        double valnmr=9.0;
        conditions->attributes->getDouble(S57AttrIds::VALNMR,valnmr);
        double sectr1=-9;
        double sectr2=-9;
        bool isFlare=false;
        bool flare45=false;
        double arc_radius = 75.59; //ps -> ~20 mm with 96dpi
                    arc_radius *= s52data->getSettings()->symbolScale;
                    double sector_radius = 94.48; //px ->25 mm with 96dpi
                    sector_radius *= s52data->getSettings()->symbolScale;
        if (! conditions->attributes->getDouble(S57AttrIds::SECTR1,sectr1)
            || ! conditions->attributes->getDouble(S57AttrIds::SECTR2,sectr2)){
                String sym;
                //no sectors
                if (valnmr < 10.0){
                    sym=lightSymbol(colors.size(),colors.c_str(),false,valnmr);
                    isFlare=true;
                    flare45=true;
                }
                else{
                    sym=lightSymbol(colors.size(),colors.c_str(),true,valnmr, arc_radius);
                    isFlare=false;
                }
                //  Is the light a directional or moire?
                if (strpbrk(catlit, "\001\016")){
                    //ORIENT not handled in orig...
                    rt.append(";SY(QUESMRK1)");
                }
                else{
                    rt.append(sym);
                    if (isFlare){
                        if (flare45){
                            rt.append(",45)");
                        }
                        else{
                            rt.append(",135)");
                        }
                    }
                    else{
                        rt.append(")");
                    }
                }
            }
            else{
                double sweep=0;
                //sectors
                if (sectr1 == -9){
                    sectr1=0;
                    sectr2=0;
                }
                else{
                    sweep = (sectr1 > sectr2) ? sectr2-sectr1+360 : sectr2-sectr1;
                }
                if (sweep<1.0 || sweep==360.0){
                    rt.append(lightSymbol(colors.size(),colors.c_str(),true,valnmr,arc_radius));
                }
                else{
                    //        Build the (opencpn private) command string like this:
                    //        e.g.  ";CA(OUTLW, 4,LITRD, 2, sectr1, sectr2, radius)"
                    rt=FMT(";%s(OUTLW, 4",PrivateRules::PR_LIGHT());
                    const char * colist=colors.c_str();
                    if (colors.size() == 1)
                    {
                        if (strpbrk(colist, "\003"))
                            rt.append(",LITRD, 2");
                        else if (strpbrk(colist, "\004"))
                            rt.append(",LITGN, 2");
                        else if (strpbrk(colist, "\001\006\013"))
                            rt.append(",LITYW, 2");
                        else
                            rt.append(",CHMGD, 2");
                    }
                    else if (colors.size() == 2)
                    {
                        if (strpbrk(colist, "\001") && strpbrk(colist, "\003"))
                            rt.append(",LITRD, 2");
                        else if (strpbrk(colist, "\001") && strpbrk(colist, "\004"))
                            rt.append(",LITGN, 2");
                        else
                            rt.append(",CHMGD, 2");
                    }
                    else{
                        rt.append(",CHMGD, 2");
                    }
                    String litvis=conditions->attributes->getParsedList(S57AttrIds::LITVIS);
                    if (!litvis.empty()){
                        if (strpbrk(litvis.c_str(), "\003\007\010"))
                            rt=(FMT(";%s(CHBLK, 4,CHBRN, 1",PrivateRules::PR_LIGHT()));
                    }
                    if (sectr2 <= sectr1)
                        sectr2 += 360;
                    //    Sectors are defined from seaward
                    if (sectr1 > 180)
                        sectr1 -= 180;
                    else
                        sectr1 += 180;
                    if (sectr2 > 180)
                        sectr2 -= 180;
                    else
                        sectr2 += 180;
                    rt.append(FMT(",%5.1f, %5.1f, %5.1f, %5.1f", sectr1, sectr2, arc_radius, sector_radius));
                }
            }

            //final part (l06_end)
            //_LITDSN01
            String descr=_LITDSN01(s52data, conditions->attributes);
            //we should be abl to try to print the description always as
            //the text handling should prevent clutter
            rt.append(";TX('");
            rt.append(descr);
            if (flare45){
                rt.append("',3,3,3,'15110',2,-1,CHBLK,23)");
            }
            else{
                rt.append("',3,2,3,'15110',2,0,CHBLK,23)");
            }
            return rt;
    }
    static String DEPCNT02(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)
    // Remarks: An object of the class "depth contour" or "line depth area" is highlighted and must
    // be shown under all circumstances if it matches the safety contour depth value
    // entered by the mariner (see IMO PS 3.6). But, while the mariner is free to enter any
    // safety contour depth value that he thinks is suitable for the safety of his ship, the
    // SENC only contains a limited choice of depth contours. This symbology procedure
    // determines whether a contour matches the selected safety contour. If the selected
    // safety contour does not exist in the data, the procedure will default to the next deeper
    // contour. The contour selected is highlighted as the safety contour and put in
    // DISPLAYBASE. The procedure also identifies any line segment of the spatial
    // component of the object that has a "QUAPOS" value indicating unreliable
    // positioning, and symbolizes it with a double dashed line.
    //
    // Note: Depth contours are not normally labeled. The ECDIS may provide labels, on demand
    // only as with other text, or provide the depth value on cursor picking
    {
        //      GString *depcnt02  = NULL;
        //      int      safe      = FALSE;     // initialy not a safety contour
        //      GString *objlstr   = NULL;
        //      int      objl      = 0;
        //      GString *quaposstr = NULL;
        //      int      quapos    = 0;
        if (!conditions)
        {
            return rule;
        }
        const Attribute::Map *objectAttributes = conditions->attributes;
        if (!objectAttributes)
        {
            return rule;
        }
        const RenderSettings *settings = s52data->getSettings().get();
        if (!settings)
        {
            LOG_DEBUG("invalid rule call for DEPCNT02 - no settings");
            return rule;
        }
        double depth_value=0;
        bool safe = false;
        String rt;
        double safety_contour = settings->S52_MAR_SAFETY_CONTOUR;
        // in the current rules this CS is only called
        // for line types
        if (conditions->geoPrimitive != GEO_LINE)
        {
            return rule;
        }
        double valdco = 0;
        if (objectAttributes->getDouble(S57AttrIds::VALDCO, valdco))
        {
            if (valdco == conditions->nextSafetyContour)
            {
                safe = true;
            }
            depth_value = valdco;
        }
        else
        {
            double drval1 = 0.0;
            objectAttributes->getDouble(S57AttrIds::DRVAL1, drval1);
            double drval2 = drval1;
            objectAttributes->getDouble(S57AttrIds::DRVAL2, drval2); // default values
            if (drval1 <= safety_contour)
            {
                if (drval2 >= safety_contour)
                    safe = true;
            }
            else
            {
                double next_safe_contour = conditions->nextSafetyContour;
                if (fabs(drval1 - next_safe_contour) < 1e-4)
                {
                    safe = true;
                }
            }
            depth_value = drval1;
        }
        int quapos = 0;
        objectAttributes->getInt(S57AttrIds::QUAPOS, quapos);
        if (0 != quapos)
        {
            if (2 <= quapos && quapos < 10)
            {
                if (safe)
                {
                    rt = ";LS(DASH,2,DEPSC)";
                    // a lookup to SAFECD taht is in the original code seems not to succeed
                    // as this is not found in any s57data
                }
                else
                    rt = ";LS(DASH,1,DEPCN)";
            }
        }
        else
        {
            if (safe)
            {
                rt = ";LS(SOLD,2,DEPSC)";
                // lookup to SAFECN never succeeds
            }
            else
                rt = ";LS(SOLD,1,DEPCN)";
        }

        if (safe)
        {
            //Move this object to DisplayBase category
            rt.append(";");
            rt.append(PrivateRules::PR_CAT());
            rt.append("()");
        }
        rt.push_back('\037');
        return rt;
    }

    static String RESARE02(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)
    {
        // Remarks: A list-type attribute is used because an area of the object class RESARE may
        // have more than one category (CATREA). For example an inshore traffic
        // zone might also have fishing and anchoring prohibition and a prohibited
        // area might also be a bird sanctuary or a mine field.
        //
        // This conditional procedure is set up to ensure that the categories of most
        // importance to safe navigation are prominently symbolized, and to pass on
        // all given information with minimum clutter. Only the most significant
        // restriction is symbolized, and an indication of further limitations is given by
        // a subscript "!" or "I". Further details are given under conditional
        // symbology procedure RESTRN01
        //
        // Other object classes affected by attribute RESTRN are handled by
        // conditional symbology procedure RESTRN01.

        if (!conditions)
        {
            return rule;
        }
        const RenderSettings *settings = s52data->getSettings().get();
        if (!settings)
        {
            LOG_DEBUG("invalid rule call for RESARE02 - no settings");
            return rule;
        }

        String resare02;
        String restrn = conditions->attributes->getParsedList(S57AttrIds::RESTRN);
        String catrea = conditions->attributes->getParsedList(S57AttrIds::CATREA);
        String symb;
        String line;
        String prio;
        bool symbolized = settings->nBoundaryStyle==s52::LUPname::SYMBOLIZED_BOUNDARIES;
        if (!restrn.empty())
        {
            if (strpbrk(restrn.c_str(), "\007\010\016"))
            { // entry restrictions
                // Continuation A
                if (strpbrk(restrn.c_str(), "\001\002\003\004\005\006")) // anchoring, fishing, trawling
                    symb = ";SY(ENTRES61)";
                else
                {
                    if (!catrea.empty() && strpbrk(catrea.c_str(), "\001\010\011\014\016\023\025\031"))
                        symb = ";SY(ENTRES61)";
                    else
                    {
                        if (strpbrk(restrn.c_str(), "\011\012\013\014\015"))
                            symb = ";SY(ENTRES71)";
                        else
                        {
                            if (!catrea.empty() && strpbrk(catrea.c_str(), "\004\005\006\007\012\022\024\026\027\030"))
                                symb = ";SY(ENTRES71)";
                            else
                                symb = ";SY(ENTRES51)";
                        }
                    }
                }

                if (symbolized)
                    line = ";LC(RESARE51)";
                else
                    line = ";LS(DASH,2,CHMGD)";

                prio = ";OP(6---)"; // display prio set to 6
            }
            else
            {
                if (strpbrk(restrn.c_str(), "\001\002"))
                { // anchoring
                    // Continuation B
                    if (strpbrk(restrn.c_str(), "\003\004\005\006"))
                        symb = ";SY(ACHRES61)";
                    else
                    {
                        if (!catrea.empty() && strpbrk(catrea.c_str(), "\001\010\011\014\016\023\025\031"))
                            symb = ";SY(ACHRES61)";
                        else
                        {
                            if (strpbrk(restrn.c_str(), "\011\012\013\014\015"))
                                symb = ";SY(ACHRES71)";
                            else
                            {
                                if (!catrea.empty() && strpbrk(catrea.c_str(), "\004\005\006\007\012\022\024\026\027\030"))
                                    symb = ";SY(ACHRES71)";
                                else
                                    symb = ";SY(RESTRN51)";
                            }
                        }
                    }

                    if (symbolized)
                        line = ";LC(RESARE51)"; // could be ACHRES51 when _drawLC is implemented fully
                    else
                        line = ";LS(DASH,2,CHMGD)";

                    prio = ";OP(6---)"; // display prio set to 6
                }
                else
                {
                    if (strpbrk(restrn.c_str(), "\003\004\005\006"))
                    { // fishing/trawling
                        // Continuation C
                        if (!catrea.empty() && strpbrk(catrea.c_str(), "\001\010\011\014\016\023\025\031"))
                            symb = ";SY(FSHRES51)";
                        else
                        {
                            if (strpbrk(restrn.c_str(), "\011\012\013\014\015"))
                                symb = ";SY(FSHRES71)";
                            else
                            {
                                if (!catrea.empty() && strpbrk(catrea.c_str(), "\004\005\006\007\012\022\024\026\027\030"))
                                    symb = ";SY(FSHRES71)";
                                else
                                    symb = ";SY(FSHRES51)";
                            }
                        }

                        if (symbolized)
                            line = ";LC(FSHRES51)";
                        else
                            line = ";LS(DASH,2,CHMGD)";

                        prio = ";OP(6---)"; // display prio set to 6
                    }
                    else
                    {
                        if (strpbrk(restrn.c_str(), "\011\012\013\014\015")) // diving, dredging, waking...
                            symb = ";SY(INFARE51)";
                        else
                            symb = ";SY(RSRDEF51)";

                        if (symbolized)
                            line = ";LC(CTYARE51)";
                        else
                            line = ";LS(DASH,2,CHMGD)";
                    }
                    //  Todo more for s57 3.1  Look at caris catalog ATTR::RESARE
                }
            }
        }
        else
        {
            // Continuation D
            if (!catrea.empty())
            {
                if (strpbrk(catrea.c_str(), "\001\010\011\014\016\023\025\031"))
                {
                    if (strpbrk(catrea.c_str(), "\004\005\006\007\012\022\024\026\027\030"))
                        symb = ";SY(CTYARE71)";
                    else
                        symb = ";SY(CTYARE51)";
                }
                else
                {
                    if (strpbrk(catrea.c_str(), "\004\005\006\007\012\022\024\026\027\030"))
                        symb = ";SY(INFARE51)";
                    else
                        symb = ";SY(RSRDEF51)";
                }
            }
            else
                symb = ";SY(RSRDEF51)";

            if (symbolized)
                line = ";LC(CTYARE51)";
            else
                line = ";LS(DASH,2,CHMGD)";
        }

        // create command word
        if (!prio.empty())
            resare02.append(prio);
        resare02.append(line);
        resare02.append(symb);

        resare02.push_back('\037');

        return resare02;
    }

    static String RESTRN01(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)
    // Object class RESARE is symbolised for the effect of attribute RESTRN in a separate
    // conditional symbology procedure called RESARE02.
    //
    // Since many of the areas concerned cover shipping channels, the number of symbols used
    // is minimised to reduce clutter. To do this, values of RESTRN are ranked for significance
    // as follows:
    // "Traffic Restriction" values of RESTRN:
    // (1) RESTRN 7,8: entry prohibited or restricted
    //     RESTRN 14: IMO designated "area to be avoided" part of a TSS
    // (2) RESTRN 1,2: anchoring prohibited or restricted
    // (3) RESTRN 3,4,5,6: fishing or trawling prohibited or restricted
    // (4) "Other Restriction" values of RESTRN are:
    //     RESTRN 9, 10: dredging prohibited or restricted,
    //     RESTRN 11,12: diving prohibited or restricted,
    //     RESTRN 13   : no wake area.
    {
        if (!conditions)
        {
            return rule;
        }
        String restrn = conditions->attributes->getParsedList(S57AttrIds::RESTRN);
        if (restrn.empty())
            return String();
        String rt;
        String symb;
        if (strpbrk(restrn.c_str(), "\007\010\016"))
        {
            // continuation A
            if (strpbrk(restrn.c_str(), "\001\002\003\004\005\006"))
                symb = ";SY(ENTRES61)";
            else
            {
                if (strpbrk(restrn.c_str(), "\011\012\013\014\015"))
                    symb = ";SY(ENTRES71)";
                else
                    symb = ";SY(ENTRES51)";
            }
        }
        else
        {
            if (strpbrk(restrn.c_str(), "\001\002"))
            {
                // continuation B
                if (strpbrk(restrn.c_str(), "\003\004\005\006"))
                    symb = ";SY(ACHRES61)";
                else
                {
                    if (strpbrk(restrn.c_str(), "\011\012\013\014\015"))
                        symb = ";SY(ACHRES71)";
                    else
                        symb = ";SY(ACHRES51)";
                }
            }
            else
            {
                if (strpbrk(restrn.c_str(), "\003\004\005\006"))
                {
                    // continuation C
                    if (strpbrk(restrn.c_str(), "\011\012\013\014\015"))
                        symb = ";SY(FSHRES71)";
                    else
                        symb = ";SY(FSHRES51)";
                }
                else
                {
                    if (strpbrk(restrn.c_str(), "\011\012\013\014\015"))
                        symb = ";SY(INFARE51)";
                    else
                        symb = ";SY(RSRDEF51)";
                }
            }
        }
        rt.append(symb);
        rt.push_back('\037');
        return rt;
    }

    static String QUALIN01(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)
    // Remarks: The attribute QUAPOS, which identifies low positional accuracy, is attached
    // only to the spatial component(s) of an object.
    //
    // A line object may be composed of more than one spatial object.
    //
    // This procedure looks at each of the spatial
    // objects, and symbolizes the line according to the positional accuracy.
    {
        if (!conditions)
        {
            return rule;
        }
        int quapos = 0;
        bool bquapos = conditions->attributes->getInt(S57AttrIds::QUAPOS, quapos);
        String line;

        if (bquapos)
        {
            if (2 <= quapos && quapos < 10)
                line = "LC(LOWACC21)";
        }
        else
        {
            if (conditions->featureTypeCode == S57ObjectClasses::COALNE)
            {
                int conrad = 0;
                bool bconrad = conditions->attributes->getInt(S57AttrIds::CONRAD, conrad);

                if (bconrad)
                {
                    if (1 == conrad)
                        line = "LS(SOLD,3,CHMGF);LS(SOLD,1,CSTLN)";
                    else
                        line = "LS(SOLD,1,CSTLN)";
                }
                else
                    line = "LS(SOLD,1,CSTLN)";
            }
            else // LNDARE
                line = "LS(SOLD,1,CSTLN)";
        }
        if (line.empty())
            line.push_back('\037');
        return line;
    }
    static String QUAPNT01(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)
    // Remarks: The attribute QUAPOS, which identifies low positional accuracy, is attached
    // only to the spatial component(s) of an object.
    //
    // This procedure retrieves any QUALTY (ne QUAPOS) attributes, and returns the
    // appropriate symbols to the calling procedure.

    {
        if (!conditions)
        {
            return rule;
        }
        String quapnt01;
        bool accurate = true;
        int qualty = 10;
        bool bquapos = conditions->attributes->getInt(S57AttrIds::QUALTY, qualty);

        if (bquapos)
        {
            if (2 <= qualty && qualty < 10)
                accurate = false;
        }

        if (!accurate)
        {
            switch (qualty)
            {
            case 4:
                quapnt01.append(";SY(QUAPOS01)");
                break; // "PA"
            case 5:
                quapnt01.append(";SY(QUAPOS02)");
                break; // "PD"
            case 7:
            case 8:
                quapnt01.append(";SY(QUAPOS03)");
                break; // "REP"
            default:
                quapnt01.append(";SY(LOWACC03)");
                break; // "?"
            }
        }

        quapnt01.push_back('\037');
        return quapnt01;
    }

    static String QUAPOS01(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)
    // Remarks: The attribute QUAPOS, which identifies low positional accuracy, is attached
    // to the spatial object, not the feature object.
    // In OpenCPN implementation, QUAPOS of Point Objects has been converted to
    // QUALTY attribute of object.
    //
    // This procedure passes the object to procedure QUALIN01 or QUAPNT01,
    // which traces back to the spatial object, retrieves any QUAPOS attributes,
    // and returns the appropriate symbolization to QUAPOS01.
    {
        if (!conditions)
        {
            return rule;
        }
        if (GEO_LINE == conditions->geoPrimitive)
            return QUALIN01(lup, rule, s52data, conditions);

        else
            return QUAPNT01(lup, rule, s52data, conditions);
    }

    static String SLCONS03(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)

    // Remarks: Shoreline construction objects which have a QUAPOS attribute on their
    // spatial component indicating that their position is unreliable are symbolized
    // by a special linestyle in the place of the varied linestyles normally used.
    // Otherwise this procedure applies the normal symbolization.
    {
        if (!conditions)
        {
            return rule;
        }
        String slcons03;
        String cmdw;
        int quapos = 0;
        bool bquapos = conditions->attributes->getInt(S57AttrIds::QUAPOS, quapos);

        if (GEO_POINT == conditions->geoPrimitive)
        {
            if (bquapos)
            {
                if (2 <= quapos && quapos < 10)
                    cmdw = "SY(LOWACC01)";
            }
        }
        else
        {

            // This instruction not found in PLIB 3.4, but seems to appear in later PLIB implementations
            // by commercial ECDIS providers, so.....
            if (GEO_AREA == conditions->geoPrimitive)
            {
                slcons03 = "AP(CROSSX01);";
            }

            // GEO_LINE and GEO_AREA are the same
            if (bquapos)
            {
                if (2 <= quapos && quapos < 10)
                    cmdw = "LC(LOWACC01)";
            }
            else
            {
                int ival = 0;
                bool bvalstr = conditions->attributes->getInt(S57AttrIds::CONDTN, ival);
                if (bvalstr && (1 == ival || 2 == ival))
                    cmdw = "LS(DASH,1,CSTLN)";
                else
                {
                    ival = 0;
                    bvalstr = conditions->attributes->getInt(S57AttrIds::CATSLC, ival);
                    if (bvalstr && (6 == ival || 15 == ival || 16 == ival)) // Some sort of wharf
                        cmdw = "LS(SOLD,4,CSTLN)";
                    else
                    {
                        bvalstr = conditions->attributes->getInt(S57AttrIds::WATLEV, ival);

                        if (bvalstr && 2 == ival)
                            cmdw = "LS(SOLD,2,CSTLN)";
                        else if (bvalstr && (3 == ival || 4 == ival))
                            cmdw = "LS(DASH,2,CSTLN)";
                        else
                            cmdw = "LS(SOLD,2,CSTLN)"; // default
                    }
                }
            }
        }

        // WARNING: not explicitly specified in S-52 !!
        // FIXME:this is to put AC(DEPIT) --intertidal area
        // Could this be bug in OGR ?
        /*
        if (AREAS_T == S57_getObjtype(geo)) {
            GString    *seabed01  = NULL;
            GString    *drval1str = S57_getAttVal(geo, "DRVAL1");
            double      drval1    = (NULL == drval1str)? -UNKNOWN : atof(drval1str->str);
            GString    *drval2str = S57_getAttVal(geo, "DRVAL2");
            double      drval2    = (NULL == drval2str)? -UNKNOWN : atof(drval2str->str);
            // NOTE: change sign of infinity (minus) to get out of bound in seabed01


            PRINTF("***********drval1=%f drval2=%f \n", drval1, drval2);
            seabed01 = _SEABED01(drval1, drval2);
            slcons03 = g_string_new(seabed01->str);
            g_string_free(seabed01, TRUE);

        }
        */

        if (!cmdw.empty())
            slcons03.append(cmdw);

        //      Match CM93 CMAPECS presentation?
        /*
            if (GEO_AREA == obj->Primitive_type)
                  slcons03.Append(_T(";AC(LANDA)"));
        */

        slcons03.push_back('\037');
        return slcons03;
    }

    static String DATCVR01(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions){
        String rt="LC(HODATA01)";
        rt.push_back('\037');
        return rt;
    }
    S52CondRules::Rule::Map S52CondRules::rules({
        /*
        empty rules:
            CLRLIN01
            DEPVAL01
            LEGLIN02
            LITDSN01
            OWNSHP02
            PASTRK01
            SEABED01
            UDWHAZ03
            VESSEL01
            VRMEBL01
        TODO:
            DATCVR01 ?? - limiting enc coverage - not found in chartsymbols
            SOUNDG03 - only for multipoint soundings - that we do different!
            SYMINS01 - virtual ais atons

        */
        {"dummy1", Rule(dummy1)},
        {"DATCVR01", Rule(DATCVR01, Rule::T_STEP1)},
        {"SLCONS03", Rule(SLCONS03)},
        {"QUAPOS01", Rule(QUAPOS01)},
        {"QUAPNT01", Rule(QUAPNT01)},
        {"QUALIN01", Rule(QUALIN01)},
        {"RESTRN01", Rule(RESTRN01)},
        {"DEPARE01", Rule(DEPARE01)},
        {"DEPARE02", Rule(DEPARE01)},
        {"RESARE02", Rule(RESARE02)},
        {"TOPMAR01", Rule(TOPMAR01)},
        {"OBSTRN04", Rule(OBSTRN04)},
        {"WRECKS02", Rule(WRECKS02)},
        {"LIGHTS06", Rule(LIGHTS06)},
        {"LIGHTS05", Rule(LIGHTS06)},
        {"DEPCNT02", Rule(DEPCNT02)},
        {"SOUNDG02", Rule(SOUNDG02, Rule::T_STEP1)} // already translate when building s52data
    });
}

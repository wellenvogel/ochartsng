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
#include "S52Rules.h"
#include "S52Data.h"
#include "S52CondRules.h"
#include "Exception.h"
#include "sstream"
#include "iostream"
namespace s52
{
    
    void RuleCreator::rulesFromString(const LUPrec *lup, const String &ruleStr, const S52Data *s52data, std::function<void (const s52::Rule *)> writer, bool tryExpansion, const RuleConditions *conditions)
    {
        StringVector topRules = StringHelper::split(ruleStr, "\037");
        for (auto pRule = topRules.begin(); pRule != topRules.end(); pRule++)
        {
            StringVector strRules = StringHelper::split(*pRule, ";");
            for (auto sRule = strRules.begin(); sRule != strRules.end(); sRule++)
            {
                if (sRule->empty()){
                    continue;
                }
                //LOG_DEBUG("LUP %d:parsing rule %s, num %ld",lup->RCID, ruleStr,rules.size());
                StringVector nv = StringHelper::split(*sRule, "(",1);
                if (nv.size() != 2)
                {
                    LOG_DEBUG("%d:%s invalid rule", lup->RCID, *sRule);
                    continue;
                }
                try
                {
                    StringHelper::rtrimI(nv[1], ')');
                    if (nv[0] == "CS")
                    {
                        if (tryExpansion)
                        {
                            String expanded = S52CondRules::expand(lup, *sRule, s52data, conditions);
                            rulesFromString(lup, expanded, s52data, writer, false);
                        }
                        else
                        {
                            const CondRule *rule = create<CondRule>(*sRule);
                            writer(rule);
                        }
                        continue;
                    }
                    if (nv[0] == "AC")
                    {
                        const AreaRule *r = create<AreaRule>(nv[1], s52data->getColor(StringHelper::beforeFirst(nv[1],",",true)));
                        writer(r);
                        continue;
                    }
                    if (nv[0] == "AP")
                    {
                        SymbolPtr symbol=s52data->getSymbol(PT_PREFIX+nv[1]);
                        const SymAreaRule *r=create<SymAreaRule>(nv[1],symbol);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == "SY")
                    {
                        const SymbolRule *r = create<SymbolRule>(nv[1], s52data->checkSymbol(nv[1]));
                        writer(r);
                        continue;
                    }
                    if (nv[0] == "TE")
                    {
                        StringVector parts=StringHelper::split(nv[1],",");
                        StringOptions options=S52TextParser::parseStringOptions(s52data,parts,2);
                        const StringTERule *r = create<StringTERule>(nv[1],options);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == "TX")
                    {
                        StringVector parts=StringHelper::split(nv[1],",");
                        StringOptions options=S52TextParser::parseStringOptions(s52data,parts,1);
                        const StringTXRule *r = create<StringTXRule>(nv[1],options);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == "MP")
                    {
                        StringVector parts=StringHelper::split(nv[1],",");
                        const SoundingRule *r = create<SoundingRule>(nv[1]);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == PrivateRules::PR_SOUND()){
                        const SingleSoundingRule *r=create<SingleSoundingRule>(nv[1]);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == PrivateRules::PR_LIGHT()){
                        const CARule *r=create<CARule>(nv[1]);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == PrivateRules::PR_CAT()){
                        const DisCatRule *r=create<DisCatRule>(nv[1]);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == "LS"){
                        const SimpleLineRule *r=create<SimpleLineRule>(nv[1]);
                        writer(r);
                        continue;
                    }
                    if (nv[0] == "LC"){
                        String symName=LS_PREFIX+nv[1];
                        const SymbolLineRule *r = create<SymbolLineRule>(nv[1],s52data->checkSymbol(symName));
                        writer(r);
                        continue;
                    }
                    LOG_DEBUG("unknown rule %s",*sRule);
                }
                catch (AvException &e)
                {
                    LOG_DEBUG("exception parsing rule %s(%s):%s", nv[0],nv[1], e.what());
                }
            }
        }
        return;
    }
}

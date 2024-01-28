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
#ifndef _S52CONDRULES_H
#define _S52CONDRULES_H
#include "Types.h"
#include "S52Data.h"
#include "tinyxml2.h"
#include <memory>
namespace s52
{
    class RuleConditions;
    class S52CondRules
    {
    public:
        static String expand(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions = NULL);

    protected:
        class Rule
        {
        public:
            typedef std::map<String, Rule> Map;
            String name;
            typedef std::function<String(const LUPrec *lup, const String &rule, const S52Data *s52data, const RuleConditions *conditions)> RuleFunction;
            RuleFunction function;
            typedef enum
            {
                T_STEP1, // only depends on settings, not on object
                T_STEP2  // depends on object
            } Type;
            Type type;
            Rule(RuleFunction f, Type t = T_STEP2) : function(f), type(t) {}
        };
        static Rule::Map rules;
    };
}

#endif
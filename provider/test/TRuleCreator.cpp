/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test RuleCreator
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
#include <gtest/gtest.h>
#include "S52Rules.h"
#include "TestHelper.h"
#include "OcAllocator.h"

static int t1Desc=0;
static int t1Const=0;

static void clearCounter(){
    t1Desc=0;
    t1Const=0;
}

class T1Rule: public s52::Rule{
    static const constexpr s52::RuleType TC(){return s52::RUL_ARE_CO;}
        virtual ~T1Rule(){
            LOG_DEBUG("~T1Rule");
            t1Desc++;
        }
        protected:
        T1Rule(ocalloc::PoolRef pr):Rule(pr,s52::RUL_ARE_CO){t1Const++;}
        friend class s52::RuleCreator;
    };


TEST(RuleCreator,simple){
    clearCounter();
    std::unique_ptr<ocalloc::Pool> pool(ocalloc::makePool("test"));
    ocalloc::PoolRef pr(pool);
    s52::RuleCreator *c1=new s52::RuleCreator(pr,0);
    const T1Rule *r1=c1->create<T1Rule>("test1");
    EXPECT_NE(r1,nullptr);
    EXPECT_EQ(r1->parameter,"test1");
    EXPECT_EQ(t1Const,1);
    EXPECT_EQ(t1Desc,0);
    delete c1;
    EXPECT_EQ(t1Desc,1);
}

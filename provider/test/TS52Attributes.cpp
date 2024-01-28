/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test s52Attributes
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
#include "S52Data.h"
#include "TestHelper.h"

#define MKPOOL auto apool=ocalloc::makePool("test"); auto poolRef=ocalloc::PoolRef(apool);

TEST(S52Attributes,equalsString){
    MKPOOL;
    s52::Attribute sattr(poolRef,1,"4",1);
    EXPECT_TRUE(sattr.equals("4"));
    EXPECT_FALSE(sattr.equals("5"));
}
TEST(S52Attributes,equalsInt){
    MKPOOL;
    s52::Attribute sattr(poolRef,1,(uint32_t)4);
    EXPECT_TRUE(sattr.equals("4"));
    EXPECT_FALSE(sattr.equals("5"));
}
TEST(S52Attributes,equalsDouble){
    MKPOOL;
    s52::Attribute sattr(poolRef,1,4.1);
    EXPECT_TRUE(sattr.equals("4.1"));
    EXPECT_FALSE(sattr.equals("5"));
}
TEST(S52Attributes,compareListStr){
    MKPOOL;
    s52::Attribute::Map attributes({
        {1,{poolRef,1,"5",1}},
        {2,{poolRef,2,"7",1}}
    },poolRef);
    s52::LUPrec::AttributeMap lupAttrs({
        {1,"5"},
        {2,"7"}
    });
    s52::LUPrec lup;
    lup.ATTCArray=lupAttrs;
    EXPECT_TRUE(lup.attributeMatch(&attributes));
}
TEST(S52Attributes,compareListMix){
     MKPOOL
    s52::Attribute::Map attributes({
        {1,{poolRef,1,"5",1}},
        {2,{poolRef,2,(uint32_t)7}},
        {3,{poolRef,3,99.1}}
    },poolRef);
    s52::LUPrec::AttributeMap lupAttrs({
        {1,"5"},
        {2,"7"},
        {3,"99.1"}
    });
    s52::LUPrec lup;
    lup.ATTCArray=lupAttrs;
    EXPECT_TRUE(lup.attributeMatch(&attributes));
}

TEST(S52Attributes,compareListFail){
    MKPOOL
    s52::Attribute::Map attributes({
        {1,{poolRef,1,"5",1}},
        {2,{poolRef,2,(uint32_t)8}},
        {3,{poolRef,3,99.1}}
    },poolRef);
    s52::LUPrec::AttributeMap lupAttrs({
        {1,"5"},
        {2,"7"},
        {3,"99.1"}
    });
    s52::LUPrec lup;
    lup.ATTCArray=lupAttrs;
    EXPECT_FALSE(lup.attributeMatch(&attributes));
}
TEST(S52Attributes,LUPCompareEmpty){
    MKPOOL
    s52::Attribute::Map attributes(poolRef);
    s52::LUPrec::AttributeMap lupAttrs({
        {1,"5"},
        {2,"7"},
        {3,"99.1"}
    });
    s52::LUPrec lup;
    lup.ATTCArray=lupAttrs;
    EXPECT_FALSE(lup.attributeMatch(&attributes));
}
TEST(S52Attributes,LUPCompareQMark){
    MKPOOL
    s52::Attribute::Map attributes({
        {1,{poolRef,1,"5",1}},
        {2,{poolRef,2,(uint32_t)8}}
    },poolRef);
    s52::LUPrec::AttributeMap lupAttrs({
        {1,"5"},
        {2,"8"},
        {3,"?"}
    });
    s52::LUPrec lup;
    lup.ATTCArray=lupAttrs;
    EXPECT_TRUE(lup.attributeMatch(&attributes));
}
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test DrawingContext
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
#include "DrawingContext.h"
#include "Coordinates.h"
#include "TestHelper.h"


TEST(BasicDrawingContext,BHLhoriz1){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(-10,20);
    Coord::PixelXy p1(100,20);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,20),c);
    EXPECT_EQ(*ctx->pixel(0,21),0);
    EXPECT_EQ(*ctx->pixel(100,20),c);
    EXPECT_EQ(*ctx->pixel(101,20),0);
}
TEST(BasicDrawingContext,BHLhoriz2){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(-10,20);
    Coord::PixelXy p1(500,20);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,20),c);
    EXPECT_EQ(*ctx->pixel(0,21),0);
    EXPECT_EQ(*ctx->pixel(100,20),c);
    EXPECT_EQ(*ctx->pixel(255,20),c);
}

TEST(BasicDrawingContext,BHLhoriz3){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(100,20);
    Coord::PixelXy p1(-10,20);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,20),c);
    EXPECT_EQ(*ctx->pixel(0,21),0);
    EXPECT_EQ(*ctx->pixel(100,20),c);
    EXPECT_EQ(*ctx->pixel(101,20),0);
}

TEST(BasicDrawingContext,BHLvert1){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(100,20);
    Coord::PixelXy p1(100,40);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(100,20),c);
    EXPECT_EQ(*ctx->pixel(100,19),0);
    EXPECT_EQ(*ctx->pixel(100,40),c);
    EXPECT_EQ(*ctx->pixel(100,41),0);
    EXPECT_EQ(*ctx->pixel(101,20),0);
}

TEST(BasicDrawingContext,BHLvert2){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(100,40);
    Coord::PixelXy p1(100,20);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(100,20),c);
    EXPECT_EQ(*ctx->pixel(100,19),0);
    EXPECT_EQ(*ctx->pixel(100,40),c);
    EXPECT_EQ(*ctx->pixel(100,41),0);
    EXPECT_EQ(*ctx->pixel(101,20),0);
}
TEST(BasicDrawingContext,BHLvert3){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(100,-20);
    Coord::PixelXy p1(100,500);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(100,20),c);
    EXPECT_EQ(*ctx->pixel(100,0),c);
    EXPECT_EQ(*ctx->pixel(100,40),c);
    EXPECT_EQ(*ctx->pixel(100,255),c);
    EXPECT_EQ(*ctx->pixel(101,20),0);
}
TEST(BasicDrawingContext,BHLvert4){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(255,500);
    Coord::PixelXy p1(255,-20);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(255,20),c);
    EXPECT_EQ(*ctx->pixel(255,0),c);
    EXPECT_EQ(*ctx->pixel(255,40),c);
    EXPECT_EQ(*ctx->pixel(255,255),c);
    EXPECT_EQ(*ctx->pixel(254,20),0);
}

TEST(BasicDrawingContext,BHLsimple1){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(100,0);
    Coord::PixelXy p1(140,40);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(100,0),c);
    EXPECT_EQ(*ctx->pixel(140,40),c);
    EXPECT_EQ(*ctx->pixel(110,10),c);
    EXPECT_EQ(*ctx->pixel(120,20),c);
    EXPECT_EQ(*ctx->pixel(130,30),c);
    EXPECT_EQ(*ctx->pixel(141,40),0);
}
TEST(BasicDrawingContext,BHLinvertedx){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(140,0);
    Coord::PixelXy p1(100,40);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(140,0),c);
    EXPECT_EQ(*ctx->pixel(100,40),c);
    EXPECT_EQ(*ctx->pixel(110,30),c);
    EXPECT_EQ(*ctx->pixel(120,20),c);
    EXPECT_EQ(*ctx->pixel(130,10),c);
    EXPECT_EQ(*ctx->pixel(141,40),0);
}

TEST(BasicDrawingContext,BHLclipx){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(-10,10);
    Coord::PixelXy p1(100,120);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,20),c);
    EXPECT_EQ(*ctx->pixel(100,120),c);
    EXPECT_EQ(*ctx->pixel(10,30),c);
    EXPECT_EQ(*ctx->pixel(50,70),c);
    EXPECT_EQ(*ctx->pixel(70,90),c);
    EXPECT_EQ(*ctx->pixel(141,40),0);
}
TEST(BasicDrawingContext,BHLclipx2){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(-10,10);
    Coord::PixelXy p1(290,160);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,15),c);
    EXPECT_EQ(*ctx->pixel(254,142),c);
    EXPECT_EQ(*ctx->pixel(10,20),c);
    EXPECT_EQ(*ctx->pixel(50,40),c);
    EXPECT_EQ(*ctx->pixel(100,65),c);
    EXPECT_EQ(*ctx->pixel(254,138),0);
}
TEST(BasicDrawingContext,BHLclipx2Inverty){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p1(-10,10);
    Coord::PixelXy p0(290,160);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,15),c);
    EXPECT_EQ(*ctx->pixel(254,142),c);
    EXPECT_EQ(*ctx->pixel(10,20),c);
    EXPECT_EQ(*ctx->pixel(50,40),c);
    EXPECT_EQ(*ctx->pixel(100,65),c);
    EXPECT_EQ(*ctx->pixel(254,138),0);
}
TEST(BasicDrawingContext,BHLclipxClipy){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p1(-10,-10);
    Coord::PixelXy p0(290,290);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,0),c);
    EXPECT_EQ(*ctx->pixel(255,255),c);
    EXPECT_EQ(*ctx->pixel(10,10),c);
    EXPECT_EQ(*ctx->pixel(40,40),c);
    EXPECT_EQ(*ctx->pixel(100,100),c);
    EXPECT_EQ(*ctx->pixel(255,254),0);
}
TEST(BasicDrawingContext,BHLclipxClipyInvY){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy p0(-10,-10);
    Coord::PixelXy p1(290,290);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawLine(p0,p1,c,false);
    EXPECT_EQ(*ctx->pixel(0,0),c);
    EXPECT_EQ(*ctx->pixel(255,255),c);
    EXPECT_EQ(*ctx->pixel(10,10),c);
    EXPECT_EQ(*ctx->pixel(40,40),c);
    EXPECT_EQ(*ctx->pixel(100,100),c);
    EXPECT_EQ(*ctx->pixel(255,254),0);
}

TEST(BasicDrawingContext,DrawArcOutline){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy cp(100,100);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawArc(cp,c,50);
    EXPECT_EQ(*ctx->pixel(150,100),c);
    EXPECT_EQ(*ctx->pixel(50,100),c);
    EXPECT_EQ(*ctx->pixel(100,150),c);
    EXPECT_EQ(*ctx->pixel(100,50),c);
    EXPECT_EQ(*ctx->pixel(100,49),0);
    EXPECT_EQ(*ctx->pixel(100,51),0);
}
TEST(BasicDrawingContext,DrawArcOutlineRange){
    DrawingContext *ctx=DrawingContext::create(Coord::TILE_SIZE,Coord::TILE_SIZE);
    Coord::PixelXy cp(100,100);
    DrawingContext::ColorAndAlpha c=DrawingContext::convertColor(255,255,0);
    ctx->drawArc(cp,c,50,25);
    EXPECT_EQ(*ctx->pixel(150,100),c);
    EXPECT_EQ(*ctx->pixel(50,100),c);
    EXPECT_EQ(*ctx->pixel(100,150),c);
    EXPECT_EQ(*ctx->pixel(100,50),c);
    EXPECT_EQ(*ctx->pixel(100,49),0);
    EXPECT_EQ(*ctx->pixel(100,55),c);
    EXPECT_EQ(*ctx->pixel(100,74),c);
    EXPECT_EQ(*ctx->pixel(74,100),c);
}
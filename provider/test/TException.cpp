/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test Exception
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
#include <Exception.h>
#include <OexControl.h>
#include "TestHelper.h"
#include <sstream>

TEST(Exception,Base){
    try{
        throw AvException();
    }catch(const AvException &e){
        EXPECT_EQ(e.msg(),String("AvException:"));
    }
}
TEST(Exception,BaseString){
    try{
        throw AvException("TEST1");
    }catch(const AvException &e){
        EXPECT_EQ(e.msg(),String("AvException:TEST1"));
    }
}
DECL_EXC(AvException,TException)
TEST(Exception,InheritedString){
    try{
        throw TException("TEST2");
    }catch(const AvException &e){
        EXPECT_EQ(e.msg(),String("TException:TEST2"));
    }
}
TEST(Exception,InheritedStringError){
    try{
        throw TException("TEST2",99);
    }catch(const AvException &e){
        EXPECT_EQ(e.msg(),String("TException(99):TEST2"));
        EXPECT_EQ(e.getError(),99);
    }
}
TEST(Exception,ReasonWhat){
    try{
        throw TException("TEST2",99);
    }catch(const AvException &e){
        EXPECT_STREQ(e.what(),"TEST2");
        EXPECT_EQ(e.getError(),99);
    }
}
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test Helper
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 *
 */
#ifndef TESTHELPER_H
#define TESTHELPER_H
#include <map>
#include "SimpleThread.h"
class TestCounter{
    std::mutex mutex;
    public:
        std::map<const String,int> counter;
        void increment(const String &name);
        void reset();
        int getValue(const String &name);
        static TestCounter *instance();
    private:
        static TestCounter *_instance;
};
#define IN_RANGE(v,compare,percent) \
    { \
    double cvmin=compare * (100.0-percent)/100.0; \
    double cvmax=compare * (100.0+percent)/100.0;\
    if (cvmin > cvmax){ double t=cvmax;cvmax=cvmin;cvmin=t;}\
    EXPECT_LE(v,cvmax) << "mus be between " << cvmin << " and " << cvmax; \
    EXPECT_GE(v,cvmin) << "mus be between " << cvmin << " and " << cvmax; \
    }

#endif
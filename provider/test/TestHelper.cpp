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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.             *
 ***************************************************************************
 *
 */

#include "TestHelper.h"
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <iostream>
void TestCounter::increment(const String &name)
{
    Synchronized l(mutex);
    auto it = counter.find(name);
    if (it == counter.end())
    {
        counter[name] = 1;
    }
    else
    {
        it->second = it->second + 1;
    }
}
void TestCounter::reset()
{
    Synchronized l(mutex);
    counter.clear();
}
int TestCounter::getValue(const String &name)
{
    Synchronized l(mutex);
    auto it = counter.find(name);
    if (it == counter.end())
    {
        return -1;
    }
    return it->second;
}
TestCounter *TestCounter::_instance = new TestCounter();
TestCounter *TestCounter::instance() { return _instance; }

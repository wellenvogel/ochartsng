/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Timer functions
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
#ifndef _TIMER_H
#define _TIMER_H
#include <unistd.h>
#include <chrono>
#include <sstream>
#include <string.h>
#include "errno.h"
#include "Types.h"

class Timer
{
public:
    static unsigned long long systemMillis()
    {
        unsigned long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();
        return ms;
    }

    static void microSleep(unsigned long sleep)
    {
        usleep((useconds_t)sleep);
    }

    using SteadyTimePoint = std::chrono::steady_clock::time_point;

    static SteadyTimePoint steadyNow()
    {
        return std::chrono::steady_clock::now();
    }
    /**
     * get a (not very accurate) timestamp (millis since epoch)
     * from a steadynow
    */
    static int64_t steadyToTime(const SteadyTimePoint &steady){
        auto sysNow=std::chrono::system_clock::now();
        SteadyTimePoint stNow=steadyNow();
        int64_t passed = std::chrono::duration_cast<std::chrono::milliseconds>(steady-stNow).count();
        int64_t millisNow=std::chrono::duration_cast<std::chrono::milliseconds>(sysNow.time_since_epoch()).count();
        return millisNow+passed;
    }
    static bool steadyPassedMillis(SteadyTimePoint start, int64_t timeout)
    {
        int64_t passed = std::chrono::duration_cast<std::chrono::milliseconds>(steadyNow() - start).count();
        return passed >= timeout;
    }
    static int64_t remainMillis(SteadyTimePoint start, int64_t timeout)
    {
        int64_t passed = std::chrono::duration_cast<std::chrono::milliseconds>(steadyNow() - start).count();
        return (timeout > passed) ? timeout - passed : 0;
    }
    static int64_t steadyDiffMicros(SteadyTimePoint start, SteadyTimePoint end = steadyNow())
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    static int64_t steadyDiffMillis(SteadyTimePoint start, SteadyTimePoint end = steadyNow())
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }

    static SteadyTimePoint addMillis(SteadyTimePoint v, int64_t millis){
        return v+std::chrono::milliseconds(millis);
    }
    class Measure
    {
        class Entry
        {
        public:
            int64_t diff = 0;
            String item;
            int idx = -1;
            Entry(const String &e, int64_t d, int i = -1) : item(e), diff(d), idx(i)
            {
            }
        };
        std::vector<Entry> items;
        SteadyTimePoint start;
        SteadyTimePoint last;

    public:
        Measure()
        {
            start = steadyNow();
            last = start;
        }
        void reset()
        {
            start = steadyNow();
            last = start;
            items.clear();
        }
        void add(const String &e, int idx = -1)
        {
            SteadyTimePoint next = steadyNow();
            int64_t diff = steadyDiffMicros(last, next);
            last = next;
            items.push_back(Entry(e, diff, idx));
        }
        void set(SteadyTimePoint v, const String &e, int idx = -1, bool setLast = true)
        {
            SteadyTimePoint now = steadyNow();
            int64_t diff = steadyDiffMicros(v, now);
            items.push_back(Entry(e, diff, idx));
            if (setLast)
                last = now;
        }
        SteadyTimePoint current()
        {
            return last;
        }
        String toString()
        {
            std::stringstream out;
            for (auto it = items.begin(); it != items.end(); it++)
            {
                out << it->item;
                if (it->idx >= 0)
                    out << "[" << it->idx << "]";
                out << "=" << it->diff << ",";
            }
            out << "#=" << steadyDiffMicros(start, last);
            return out.str();
        }
    };
};

#endif

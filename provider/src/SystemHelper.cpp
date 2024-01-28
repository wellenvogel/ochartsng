/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  System Helper
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2020 by Andreas Vogel   *
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

#include "SystemHelper.h"
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/sysinfo.h>

SystemHelper::SystemHelper()
{
}

SystemHelper::~SystemHelper()
{
}
// see OpenCPN chart1.cpp GetMemoryStatus
bool SystemHelper::GetMemInfo(int *global, int *our, int *vsz)
{
    if (global)
    {
        *global = 0;
        struct sysinfo sys_info;
        if (sysinfo(&sys_info) != -1)
            *global = ((u_int64_t)sys_info.totalram * sys_info.mem_unit) / 1024;
    }
    if (our || vsz)
    {
        if (our)
            *our = 0;
        if (vsz)
            *vsz = 0;
        FILE *file = fopen("/proc/self/statm", "r");
        int vszp = 0;
        int rssp = 0;
        if (file)
        {
            if (fscanf(file, "%d %d", &vszp, &rssp))
            {
                if (our)
                    *our = 4 * rssp; // XXX assume 4K page
                if (vsz)
                    *vsz = 4 * vszp;
            }
            fclose(file);
        }
    }
    return true;
}

int SystemHelper::GetAvailableMemoryKb()
{
    long pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize <= 0)
        return -1;
    long pagesAvail = get_avphys_pages();
    if (pagesAvail < 0)
        return -1;
    return pagesAvail * pagesize / 1024;
}
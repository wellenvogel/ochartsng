/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  System Helper
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
#ifndef SYSTEMHELPER_H
#define SYSTEMHELPER_H
#include "Types.h"
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

class SystemHelper
{
public:
    SystemHelper();
    virtual ~SystemHelper();
    // meminfo in kb
    static bool GetMemInfo(int *global, int *our,int *vsz=nullptr);
    static int GetAvailableMemoryKb();
    static String sysError(int error = -1)
    {
        char buffer[256];
        buffer[0] = 0;
        if (error == -1)
            error = errno;
        char *ept = strerror_r(error, buffer, 255);
        return String(ept);
    }
    //max VSZ in kb
    static int maxVsz(){
        if (sizeof(void*) < 8 ){
            //32 bit
            return 3*1024*1024; //3GB
        }
        return INT_MAX;
    }

private:
};

#endif /* SYSTEMHELPER_H */


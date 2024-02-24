/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  preload android
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2024 by Andreas Vogel   *
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
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>     
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <mutex>
typedef int (*BindFunction)(int socket, const struct sockaddr *addr,
                socklen_t addrlen);

BindFunction o_bind=NULL;
static std::mutex bindLock;
typedef std::unique_lock<std::mutex> Synchronized;
typedef struct sockaddr_un daddr;
#define DEFAULT_ADDR "com.opencpn.ocharts_pi"
#define DPRINTF(...) {if (debug) {printf("(%d)",getpid());printf(__VA_ARGS__);fflush(stdout);}}
extern "C"{
int bind(int socket, const struct sockaddr *addr,
                socklen_t addrlen){
                    {
                        Synchronized l(bindLock);
                        if (o_bind == NULL){
                            o_bind=(BindFunction)dlsym(RTLD_NEXT,"bind");
                        }
                    }
                    bool debug=getenv("AVNAV_OCHARTS_DEBUG") != NULL;
                    daddr address;
                    if (addrlen == sizeof(daddr)){
                        daddr *old=(daddr*)addr;
                        if (old->sun_family == AF_LOCAL){
                            if (strcmp(DEFAULT_ADDR,old->sun_path) == 0){
                                DPRINTF("bind to %s detected\n",DEFAULT_ADDR);
                                const char *newAddr=getenv("TEST_PIPE_ENV");
                                if (newAddr != NULL){
                                    DPRINTF("changing to %s\n",newAddr);
                                    address.sun_family=AF_LOCAL;
                                    strncpy(address.sun_path,newAddr,sizeof(address.sun_path)-1);
                                    return o_bind(socket,(struct sockaddr *)&address,sizeof(address));
                                }
                            }
                        }
                    }
                    return o_bind(socket, addr,addrlen);
                }
};
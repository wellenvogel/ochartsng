/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  preload android
 *           just make the domain socket address configurable
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2024 by Andreas Vogel   *
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:

 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ***************************************************************************
 *
 */
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>     
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <string.h>
#include "EnvDefines.h"
typedef int (*BindFunction)(int socket, const struct sockaddr *addr,
                socklen_t addrlen);

BindFunction o_bind = NULL;
typedef struct sockaddr_un daddr;
#define DEFAULT_ADDR "com.opencpn.ocharts_pi"
#define DPRINTF(...)                  \
    {                                 \
        if (debug)                    \
        {                             \
            printf("(%d)", getpid()); \
            printf(__VA_ARGS__);      \
            fflush(stdout);           \
        }                             \
    }

int bind(int socket, const struct sockaddr *addr,
         socklen_t addrlen)
{
    int debug=(getenv("AVNAV_OCHARTS_DEBUG") != NULL)?1:0;
    DPRINTF("bind call\n");
    daddr address;
    {
        if (o_bind == NULL)
        {
            o_bind = (BindFunction)dlsym(RTLD_NEXT, "bind");
        }
    }

    daddr *old = (daddr *)addr;
    if (old->sun_family == AF_LOCAL)
    {
        if (old->sun_path[0] == 0 &&  strcmp(DEFAULT_ADDR, old->sun_path+1) == 0)
        {
            DPRINTF("bind to %s detected\n", DEFAULT_ADDR);
            const char *newAddr = getenv(TEST_PIPE_ENV);
            if (newAddr != NULL)
            {
                DPRINTF("changing to %s\n", newAddr);
                address.sun_family = AF_LOCAL;
                address.sun_path[0]=0;
                strncpy(address.sun_path+1, newAddr, sizeof(address.sun_path) - 2);
                int slen=offsetof(struct sockaddr_un, sun_path)+strlen(address.sun_path+1)+1;
                return o_bind(socket, (struct sockaddr *)&address, slen);
            }
        }
        else
        {
            DPRINTF("bind other address %s\n", old->sun_path);
        }
    }
    else
    {
        DPRINTF("bind other family %d\n", (int)old->sun_family);
    }

    return o_bind(socket, addr, addrlen);
}

/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  socket helper functions
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

#ifndef SOCKETHELPER_H
#define SOCKETHELPER_H
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "Logger.h"

typedef struct sockaddr_storage SocketAddress; //will allow us to hide OS stuff

class SocketHelper{
private:
    static bool WaitFor(int socket,long ms,bool read=true){
        if (socket < 0) return false;
        fd_set fds;
        struct timeval tv;
        int rt;
        FD_ZERO(&fds);
        FD_SET(socket,&fds);
        tv.tv_sec=(int)(ms/1000);
        tv.tv_usec=(ms-tv.tv_sec*1000)*1000;
        if (read){
            rt=select(socket+1,&fds,NULL,NULL,(ms != 0)?&tv:NULL);
        }
        else{
            rt=select(socket+1,NULL,&fds,NULL,(ms != 0)?&tv:NULL);
        }
        if (LogSysError(rt,"select",socket)) return false;
        return FD_ISSET(socket,&fds);
    }
public:
    static bool LogSysError(int res,const char *txt,const char *ip,int port){
        if (res >=0) return false;
        char buffer[200];
        const char *ep=strerror_r(errno,buffer,200);
        if (ip == NULL) ip="0.0.0.0";
        LOG_ERROR("%s(%s:%d): SYSERROR: %d=%s",txt,ip,port,errno,(ep?ep:"null"));
        return true;
    }
    static bool LogSysError(int res,const char *txt,int fd){
        if (res >=0) return false;
        char buffer[200];
        const char *ep=strerror_r(errno,buffer,200);
        LOG_ERROR("%s(fd=%d): SYSERROR: %d=%s",txt,fd,errno,(ep?ep:""));
        return true;
    }
    static SocketAddress CreateAddress(const char *ip,int port,sa_family_t family=AF_INET){
        SocketAddress rt={0};
        rt.ss_family=family;
        if (family == AF_INET){
            sockaddr_in *r=(sockaddr_in *)&rt;
            r->sin_port=htons(port);
            if (ip != NULL){
                inet_pton(family,ip,&(r->sin_addr)); //TODO: error check
            }
            else{
                r->sin_addr.s_addr=INADDR_ANY;
            }
        }
        else{
            sockaddr_in6 *r=(sockaddr_in6 *)&rt;
            r->sin6_port=htons(port);
            if (ip != NULL){
                inet_pton(family,ip,&(r->sin6_addr)); //TODO: error check
            }
            else{
                r->sin6_addr=in6addr_any;
            }
            
        }
        return rt;
    }
    
    class IfAddress{
    private:
        /**
         * only for the usage within returns from getifaddr!
         * @param sa
         * @param ss
         */
        
        static void IfToSa(struct sockaddr* sa, SocketAddress *ss){
            if (sa == NULL || ss == NULL) return;
            int family=sa->sa_family;
            if (family == AF_INET){                
                memcpy(ss,sa,sizeof(struct sockaddr_in));
            }
            else{
                memcpy(ss,sa,sizeof(struct sockaddr_in6));
            }
        }
    public:
        SocketAddress base={0};
        SocketAddress netmask={0};
        IfAddress(ifaddrs *current){
            IfToSa(current->ifa_addr,&base);
            IfToSa(current->ifa_netmask,&netmask);
        }
        bool IsInNet(SocketAddress *other){
            int family=base.ss_family;
            if (family != other->ss_family) return false;
            int numBytes=8;
            unsigned char *mask=NULL;
            unsigned char *basep=NULL;
            unsigned char *addr=NULL;
            if (family == AF_INET6){
                numBytes=16;
                mask=(unsigned char *) &(((sockaddr_in6*)(&netmask))->sin6_addr);
                basep=(unsigned char *) &(((sockaddr_in6*)(&base))->sin6_addr);
                addr=(unsigned char *) &(((sockaddr_in6*)(other))->sin6_addr);
            }
            else{
                numBytes=4;
                mask=(unsigned char *) &(((sockaddr_in*)(&netmask))->sin_addr);
                basep=(unsigned char *) &(((sockaddr_in*)(&base))->sin_addr);
                addr=(unsigned char *) &(((sockaddr_in*)other)->sin_addr);
            }
            for (int i=0;i<numBytes;i++){
                if ((*basep & *mask) != (*addr & *mask)) return false;
                basep++;
                addr++;
                mask++;
            }
            return true;
        }
    };
    
    typedef std::vector<IfAddress> IfAddressList;
    
    static IfAddressList GetNetworkInterfaces(){
        IfAddressList rt;
        struct ifaddrs *ifaddr, *ifa;
        int rs=getifaddrs(&ifaddr);
        if (LogSysError(rs,"getifaddrs",0)) return rt;
        int family;
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
               if (ifa->ifa_addr == NULL)
                   continue;
               if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6){
                   continue;
               }
               IfAddress address(ifa);
               rt.push_back(address);
        }
        freeifaddrs(ifaddr);
        return rt;
    }
    
    static SocketAddress GetLocalAddress(int socket){
        SocketAddress rt={0};
        socklen_t len=sizeof(rt);
        int res=getsockname(socket,(sockaddr *)&rt,&len);
        if (LogSysError(res,"GetLocalAddress",socket)) return rt;
        return rt;
    }
    static SocketAddress GetRemoteAddress(int socket){
        SocketAddress rt={0};
        socklen_t len=sizeof(rt);
        int res=getpeername(socket,(sockaddr*)&rt,&len);
        if (LogSysError(res,"GetLocalAddress",socket)) return rt;
        return rt;
    }
    
    static String GetAddress(SocketAddress sa ){
        if (sa.ss_family == AF_INET){
            sockaddr_in *s=(sockaddr_in*)&sa;
            char buffer[INET_ADDRSTRLEN];
            if (inet_ntop(s->sin_family,&(s->sin_addr),buffer,INET_ADDRSTRLEN) == NULL){
                LogSysError(-1,"GetAddress",-1);
                return String();
            }
            String rt(buffer);
            return rt;
        }
        else{
            sockaddr_in6 *s=(sockaddr_in6*)&sa;
            char buffer[INET6_ADDRSTRLEN];
            if (inet_ntop(s->sin6_family,&(s->sin6_addr),buffer,INET6_ADDRSTRLEN) == NULL){
                LogSysError(-1,"GetAddress",-1);
                return String();
            }
            String rt(buffer);
            return rt;
        }
    }
    
    static int GetPort(SocketAddress sa){
        if (sa.ss_family == AF_INET){
            sockaddr_in *s=(sockaddr_in*)&sa;
            return ntohs(s->sin_port);
        }
        else{
            sockaddr_in6 *s=(sockaddr_in6*)&sa;
            return ntohs(s->sin6_port);
        }
    }
    
    static int SetNonBlocking(int socket,bool on=true){
        
        int save_fd = fcntl( socket, F_GETFL );
        if (LogSysError(save_fd,"set blocking on/off",socket)) return -1;
        if (on){
            save_fd |= O_NONBLOCK;
        }
        else{
            save_fd &= ~O_NONBLOCK;
        }
        fcntl( socket, F_SETFL, save_fd );
        
        return 0;
    }
    
    static int CreateAndBind(const char *ip,int port,int family=AF_INET){
        int sock=socket(family,SOCK_STREAM|SOCK_CLOEXEC,IPPROTO_TCP);
        if (LogSysError(sock,"createSocket",ip,port))return -1;
        int y=1;
        setsockopt( sock, SOL_SOCKET,
                SO_REUSEADDR, &y, sizeof(int));
        SocketAddress address=CreateAddress(ip,port,family);
        int res=bind(sock,(struct sockaddr*)&address,sizeof(address));
        if (LogSysError(res,"bind",ip,port)) {
            close(sock);
            return -1;
        }
        return sock;
    }
    static int Listen(int socket,int len=10){
        int res=listen(socket,len);
        if (LogSysError(res,"listen",socket)) return -1;
        return res;
    }
    /**
     * run accept
     * @param socket
     * @param timeout if set, we should also have set the socket to non blocking
     * @return 
     */
    
    static int Accept(int socket,long timeout=0){
        if (timeout > 0){
            if (! WaitFor(socket,timeout)) return -1;
        }
        int rt=accept(socket,NULL,NULL);
        if (LogSysError(rt,"accept",socket)) return -1;
        fcntl(rt,F_SETFD,FD_CLOEXEC);
        return rt;
    }
    static int Read(int socket,char *buffer, int len,long timeout=0){
        
        if (! WaitFor(socket,timeout)) return -1;
        
        int rt=read(socket,buffer,len);
        if (LogSysError(rt,"read",socket)) return -1;
        return rt;
    }
    static int Write(int socket,const void * buffer, int len, long timeout=0){
        if (timeout > 0){
            if (! WaitFor(socket,timeout,false)) return -1;
        }
        int rt=write(socket,buffer,len);
        if (LogSysError(rt,"write",socket)) return -1;
        return rt;
    }
    static int WriteAll(int socket,const void * buffer, int len, long timeout=0){
        int offset=0;
        int remain=len;
        int wr;
        const char * bp=(const char *)buffer; //avoid warnings about pointer arithmetic
        Timer::SteadyTimePoint start=Timer::steadyNow();
        while (remain > 0 &&(timeout == 0 || ! Timer::steadyPassedMillis(start,timeout))){
            int wr=Write(socket,bp+offset,remain,(timeout == 0)?0: Timer::remainMillis(start,timeout));
            if (LogSysError(wr,"write",socket)) return -1;
            remain-=wr;
            offset+=wr;
        }
        return len;
    }
};



#endif /* SOCKETHELPER_H */


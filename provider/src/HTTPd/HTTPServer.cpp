/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  HTTP server
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
#include "HTTPServer.h"
#include "Logger.h"
#include "Worker.h"
#include "SocketHelper.h"
#include "SystemHelper.h"



RequestHandler *HandlerMap::GetHandler(String url)
{
    LOG_DEBUG("HTTPd::GetHandler(%s)",url.c_str());
    HandlerList::iterator it;
    String baseName;
    for (it=handlers.begin();it<handlers.end();it++){
        String prfx=(*it)->GetUrlPattern();
        if (prfx == url){
            return *it;
        }
        if (prfx.back()=='*'){
            String pattern=prfx.substr(0,prfx.length()-1);
            if (url.find(pattern) == 0){
                return *it;
            }
        }
    }
    return NULL;
}

HandlerMap::~HandlerMap(){}
HandlerMap::HandlerMap(){ }

void HandlerMap::AddHandler(RequestHandler* handler){
    handlers.push_back(handler);
}


class InterfaceListProvider: public Thread{
private:
    SocketHelper::IfAddressList interfaces;
    std::mutex                  lock;
    long                        loopMs;
public:
    InterfaceListProvider(long loopMs=3000): Thread(){
        this->loopMs=loopMs;
    }
    virtual ~InterfaceListProvider(){
       
    }
    virtual void run(){
        while (! shouldStop()){
            SocketHelper::IfAddressList newInterfaces=SocketHelper::GetNetworkInterfaces();
            {
                Synchronized locker(lock);
                interfaces=newInterfaces;
            }
            LOG_DEBUG("InterfaceListProvider: fetched %ld interface addresses",
                newInterfaces.size());
            SocketHelper::IfAddressList::iterator it;
            for (it=newInterfaces.begin();it!=newInterfaces.end();it++){
                LOG_DEBUG("InterfaceListProvider: interface %s, mask %s",
                        SocketHelper::GetAddress(it->base).c_str(),
                        SocketHelper::GetAddress(it->netmask).c_str());
            }
            waitMillis(loopMs);
        }
    }
    bool IsLocalNet(SocketAddress *other){
        Synchronized locker(lock);
        SocketHelper::IfAddressList::iterator it;
        for (it=interfaces.begin();it != interfaces.end();it++){
            if ((*it).IsInNet(other)) return true;
        }
        return false;
    }
};



HTTPServer::HTTPServer(int port,int numThreads) {
    this->port=port;
    this->numThreads=numThreads;
    this->started=false;
    this->acceptCondition=new Condition(acceptMutex);
    this->acceptBusy=false;
    this->handlers=new HandlerMap();
    this->interfaceLister=new InterfaceListProvider();
}

HTTPServer::~HTTPServer() {
    if (started) {
        Stop();        
        delete interfaceLister;
        interfaceLister=NULL;
    }
    delete this->acceptCondition;
}

bool HTTPServer::Start(){
    if (started) return false;
    LOG_INFO("HTTPServer start");
    interfaceLister->start();
    listener=SocketHelper::CreateAndBind(NULL,port);
    if (listener < 0){
        throw HTTPException(FMT("unable to bind at port %d: %s",port,SystemHelper::sysError()),errno);
    } 
    for (int i=0;i<numThreads;i++){
        Worker *w=new Worker(this,handlers);
        w->start();
        workers.push_back(w);
    }
    
    LOG_INFO("HTTP Server starting at port %d listening with fd %d", port,listener);
    int rt=SocketHelper::Listen(listener);
    if (rt < 0){
        close(listener);
        throw HTTPException(FMT("unable to listen at port %d: %s",port,SystemHelper::sysError()),errno);
    }
    SocketHelper::SetNonBlocking(listener);
    started=true;
    return true;
}
void HTTPServer::Stop(){
    if (!started) return;
    LOG_INFO("stopping HTTP server");
    WorkerList::iterator it;
    for (it=workers.begin();it<workers.end();it++){
        (*it)->stop();
    }
    started=false;
    close(listener);
    acceptCondition->notifyAll();
    for (it=workers.begin();it<workers.end();it++){
        (*it)->join();
        delete *(it);
    }
    interfaceLister->stop();
    interfaceLister->join();
    LOG_INFO("HTTP server stopped");
}
void HTTPServer::AddHandler(RequestHandler * handler){
    handlers->AddHandler(handler);
}

int HTTPServer::Accept(){
    bool canAccept=false;
    while (! canAccept && started){
        {
            Synchronized x(acceptMutex);
            canAccept=!acceptBusy;
            if (canAccept) acceptBusy=true;
        }
        if (canAccept){
            int socket=SocketHelper::Accept(listener,500);
            {
                Synchronized x(acceptMutex);
                acceptBusy=false;
                acceptCondition->notifyAll(x);
            }
            if (socket >= 0){
                SocketAddress peer=SocketHelper::GetRemoteAddress(socket);
                if (! interfaceLister->IsLocalNet(&peer)){
                    LOG_DEBUG("discard request from %s, no local net",
                            SocketHelper::GetAddress(peer).c_str());
                    socket=-1;
                }
            }
            return socket;
        }
        if (! started){
            return -1;
        }
        acceptCondition->wait(1000);
    }
    return -1;
}
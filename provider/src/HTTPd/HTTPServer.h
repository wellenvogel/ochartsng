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
#ifndef HTTPSERVER_H
#define HTTPSERVER_H
#include "RequestHandler.h"
#include "Worker.h"
#include "SimpleThread.h"
#include "Exception.h"

class Worker;
class InterfaceListProvider;
typedef std::vector<Worker*> WorkerList;

typedef std::vector<RequestHandler *> HandlerList;
class HandlerMap{
private:
    HandlerList handlers;
public:
    HandlerMap();
    RequestHandler * GetHandler(String url);
    void AddHandler(RequestHandler *handler);
    ~HandlerMap();
};

class AcceptInterface{
public:
    virtual int Accept()=0;
};
class HandlerMap;
class HTTPServer: public AcceptInterface {
public:    
    DECL_EXC(AvException,HTTPException)
private:
    int             listener;
    int             port;
    int             numThreads;
    HandlerMap      *handlers;
    WorkerList      workers;
    bool            started;
    std::mutex      acceptMutex;
    Condition       *acceptCondition;
    bool            acceptBusy;
    InterfaceListProvider *interfaceLister;

public:    
    HTTPServer(int port,int numThreads);
    bool Start();
    void Stop();
    void AddHandler(RequestHandler * handler);
    virtual ~HTTPServer();
    virtual int Accept();

};

#endif /* HTTPSERVER_H */


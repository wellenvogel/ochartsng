/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  HTTP worker thread
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
#ifndef WORKER_H
#define WORKER_H

#include "SimpleThread.h"
#include "Logger.h"
#include "HTTPServer.h"
#include "SocketHelper.h"

class AcceptInterface;
class HandlerMap;
class Worker : public Thread{
private:
    AcceptInterface *accepter;
    HandlerMap *handlers;
public:
    virtual ~Worker();
    Worker(AcceptInterface *accepter,HandlerMap *handlers);
    virtual void run();
    void HandleRequest(int socket);
    void ParseAndExecute(int socket,StringVector header,HTTPRequest *request);
    void WriteHeadersAndCookies(int socket,HTTPResponse *response,HTTPRequest* request);
    void ReturnError(int socket,int code,const char * description);
    void SendData(int socket,
        HTTPResponse *rsponse,HTTPRequest *request);
    
};

#endif /* WORKER_H */


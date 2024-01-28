/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Status Request Handler
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
#ifndef STATUSREQUESTHANDLER_
#define STATUSREQUESTHANDLER_
#include <sstream>
#include "RequestHandler.h"
#include "Logger.h"
#include "ItemStatus.h"
#include "Types.h"
#include "StringHelper.h"





class StatusRequestHandler : public RequestHandler {
public:
    const String URL_PREFIX="/status";
private:
    const char * FRAME="{\"status\":\"OK\",\n\"data\":\n%s\n}\n";
    ItemStatus    *status;
public:
   
    StatusRequestHandler(ItemStatus *status){
        this->status=status;
    }
    virtual HTTPResponse *HandleRequest(HTTPRequest* request) {
        json::JSON statusStream;
        status->ToJson(statusStream);
        HTTPResponse *rt=new HTTPJsonResponse(statusStream);
        rt->responseHeaders["Access-Control-Allow-Origin"]=corsOrigin(request);
        return rt;       
    }
    virtual String GetUrlPattern() {
        return URL_PREFIX+String("*");
    }

};

#endif /* STATUSREQUESTHANDLER_ */


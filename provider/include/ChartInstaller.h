/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Installer
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
#ifndef _CHARTINSTALLER_H
#define _CHARTINSTALLER_H
#include "ItemStatus.h"
#include "SimpleThread.h"
#include "ChartManager.h"
#include "Timer.h"
#include <atomic>
#include <memory>
class ChartInstaller: public ItemStatus{
    public:
        static constexpr const int MAX_REQUESTS=20;
        DECL_EXC(AvException,InterruptedException);
        DECL_EXC(AvException,ZipException);
        typedef std::shared_ptr<ChartInstaller> Ptr;
        ChartInstaller(ChartManager::Ptr chartManager, const String &chartDir,const String &tempDir);
        typedef struct {
            String chartUrl; //chart zip, either file://... or http[s]://...
            String keyFileUrl;
            String subDir; //if set it must match the subdir in a zip
        } ChartRequest;
        typedef enum{
            ST_WAIT,
            ST_DOWNLOAD,
            ST_UPLOAD,
            ST_UNPACK,
            ST_TEST,
            ST_PARSE,
            ST_ERROR,
            ST_INTERRUPT,
            ST_DONE,
            ST_UNKNOWN
        }State;
        class Request{
            public:
                typedef std::shared_ptr<std::ofstream> StreamPtr;
                String chartUrl; //chart zip, either file://... or http[s]://...
                String keyFileUrl;
                String setId; //if set it must match the set id as parsed in chartsetinfo
                ChartRequest request;
                State state=ST_UNKNOWN;
                String tempDir;
                Timer::SteadyTimePoint start;
                Timer::SteadyTimePoint end;
                int id=-1;
                int progress=0; //progress per state in percent
                StreamPtr stream; //if opening for upload
                String  targetZipFile;
                String  targetKeyFile;
                String  error;
                void ToJson(StatusStream &stream);
                void Cleanup();
        };
        void Start();
        void Stop();
        Request GetRequest(int requestId);
        Request CurrentRequest();
        bool    InterruptRequest(int requestId);
        Request StartRequest(const String &chartUrl, const String &keyUrl); //for shop download
        Request StartRequestForUpload(); 
        bool    DeleteChartSet(const String &key);
        int     ParseChartDir(const String &name);    
        void FinishUpload(int requestId,const String &error=String());
        String GetTempDir() const {return tempDir;}
        ~ChartInstaller();
        bool IsActive() const { return worker != NULL;}
        bool CanInstall() const;
        void UpdateProgress(int requestId, int progress,bool checkInterrupted=true);
        virtual void ToJson(StatusStream &stream);
    protected:
        class WorkerRunner;
        String CreateTempDir(int requestId);    
        Request NextRequest();
        void UpdateRequest(const Request &v, bool checkInterrupted=true);
        void HouseKeeping(bool doThrow=true);
        bool CheckInterrupted(int requestId, bool throwInterrupted=true);
        typedef std::map<int,Request> RequestMap;
        RequestMap requests;
        Condition waiter;
        WorkerRunner *worker=NULL;
        String chartDir;
        String tempDir;
        String startupError;
        int nextRequestId=1;
        ChartManager::Ptr chartManager;

};
#endif
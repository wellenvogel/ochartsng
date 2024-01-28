/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  oexserverd controller
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
#ifndef _OEXCONTROL_H
#define _OEXCONTROL_H
#include "Types.h"
#include "SimpleThread.h"
#include "ItemStatus.h"
#include "FileHelper.h"
#include "Testing.h"
#include "Exception.h"
#include "Timer.h"
#include <memory>
#include <atomic>
class OexControl : public ItemStatus{
    public:
        DECL_EXC(AvException,OpenException)
        DECL_EXC(AvException,OexException)
        static const long DEFAULT_WAITTIME=3000;
        typedef std::shared_ptr<OexControl> Ptr;
        virtual ~OexControl();
        typedef enum{
            UNKNOWN,
            STARTING,
            RUNNING,
            ERROR
        } OexState;
        typedef enum {
            CMD_TEST_AVAIL=1,
            CMD_EXIT=2,
            CMD_READ_ESENC=0,
            CMD_READ_ESENC_HDR=3,
            CMD_GET_HK=6,
            CMD_OPEN_RNC=4,
            CMD_OPEN_RNC_FULL=5,
            CMD_READ_OESU=8,
            CMD_READ_OESU_HDR=9,
            CMD_UNKNOWN=1000
        } OexCommands;

        typedef struct{
            String name;
            String value;
            bool forDongle=false;
        } FPR;
        virtual void ToJson(StatusStream &stream);
        OexState GetState();
        void Start();
        void Stop();
        void Restart(String reason);
        bool WaitForState(OexState state, long timeoutMillis);
        static void Init(String progDir,String tempDir,StringVector additionalParameters);
        static Ptr Instance();
        TESTVIRT int OpenConnection(long timeoutMillis, bool ignoreState=false);
        TESTVIRT InputStream::Ptr SendOexCommand(OexCommands cmd,const String &fileName, const String &key, long waitTime=DEFAULT_WAITTIME);
        TESTVIRT FPR GetFpr(long timeoutMillis,bool forDongle=false, bool alternative=false);
        TESTVIRT bool DonglePresent(long timeoutMillis=10000); 
        TESTVIRT String GetDongleName(long timeoutMillis=10000);
        TESTVIRT void Kill();
        TESTVIRT StringVector GetAdditionalParameters(){return additionalParameters;}
        TESTVIRT String GetOchartsVersion() const;
        TESTVIRT bool CanHaveDongle() const;
    private:
        class Supervisor : public Thread{
            public:
            Supervisor(OexControl::Ptr ctl);
            virtual void run();
            void Pause();
            void Resume();
            private:
            bool paused=false;
            OexControl::Ptr control;
        };
        typedef struct {
            unsigned char cmd=0;
            char fifo_name[256]={0};
            char senc_name[256]={0};
            char senc_key[512]={0};
        } OexMessage;
        bool PingOex(long waitTime=DEFAULT_WAITTIME);
        TESTVIRT InputStream::Ptr DoSendOexCommand(OexCommands cmd,const String &fileName, const String &key, long waitTime=DEFAULT_WAITTIME, bool ignoreState=false);
        TESTVIRT String GetDongleNameInternal(long timeoutMillis=10000);
        OexControl(String progDir,String tempDir,StringVector additionalParameters);
        void StopServer(pid_t pid);
        bool SetState(OexState state,String error=String());
        bool SetRequestedState(OexState state);
        long getNextTempIdx();
        OexState GetRequestedState();
        String GetLastError();
        Supervisor *supervisor=NULL;
        OexState _state=UNKNOWN;
        OexState _requestedState=UNKNOWN;
        static Ptr _instance;
        String progDir;
        String tempDir;
        std::mutex mutex;
        StringVector additionalParameters;
        String lastError;
        long tempDirIdx=0;
        std::atomic<int> pid={-1};
        Timer::SteadyTimePoint lastDongleState;
        bool donglePresent=false;
        String dongleName="";
};
#endif
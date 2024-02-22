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

#include "Types.h"
#include "OexControl.h"
#include "Timer.h"
#include "SystemHelper.h"
#include "Logger.h"
#include "FileHelper.h"
#include "EnvDefines.h"
#include "StringHelper.h"
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <utility>
#include <map>
#include <functional>
#include <sstream>
#include <utility>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define TMP_PREFIX "OEX"
#ifndef OCHARTS_VERSION
#define OCHARTS_TEXT "1.0.0.0"
#else
#define OCHARTS_TEXT TOSTRING(OCHARTS_VERSION)
#endif

static const int STOPTIME=10;
static const int QUERY_DONGLE_MS=5000; //minimal intervall to query the dongle state

typedef std::function<void(const String &prefix,const char *)> WriteFunction;
typedef std::function<bool(void)> StopFunction;

static void logWrite(const String &prefix, const char *text){
    LOG_INFO("%s: %s",prefix,text);
}

static void pipeReader(int pipeFd, const String &prefix, WriteFunction writer, StopFunction stopFunction=StopFunction()){
    LOG_INFO("pipe reader started for %s", prefix);
    struct pollfd fds[1];
    fds[0].fd = pipeFd;
    fds[0].events = POLLIN;
    static const int BSIZE = 1000;
    char buffer[BSIZE];
    int bp = 0;
    fcntl(pipeFd, F_SETFL, fcntl(pipeFd, F_GETFL) | O_NONBLOCK);
    while (true)
    {
        if (stopFunction){
            if (stopFunction()){
                break;
            }
        }
        int res = poll(fds, 1, 1000);
        if (fds[0].revents)
        {
            ssize_t bRead = read(pipeFd, buffer + bp, BSIZE - bp - 1);
            if (bRead == 0)
            {
                if (bp > 0)
                {
                    buffer[bp] = 0;
                    writer(prefix,buffer);
                }
                LOG_DEBUG("pipe reading from %s stopped",prefix);
                break;
            }
            if (bRead < 0)
            {
                if (errno != EAGAIN){
                    if (bp > 0)
                    {
                        buffer[bp] = 0;
                        writer(prefix,buffer);
                    }
                    LOG_DEBUG("pipe reading from %s stopped",prefix);
                    break;
                }
            }
            else
            {
                bp += bRead;
                buffer[bp] = 0;
                char *eol = strchr(buffer, '\n');
                while (eol != NULL)
                {
                    *eol = 0;
                    if (eol > buffer)
                    {
                        writer(prefix, buffer);
                    }
                    if (eol < (buffer + bp - 1))
                    {
                        memmove(buffer, eol + 1, (buffer + bp - 1) - eol);
                        bp -= eol + 1 - buffer;
                        eol = strchr(buffer, '\n');
                    }
                    else
                    {
                        bp = 0;
                        eol = NULL;
                    }
                }
                if (bp >= (BSIZE - 1))
                {
                    writer(prefix, buffer);
                    bp = 0;
                }
            }
        }
    }
    LOG_INFO("Pipe reader stopped for %s",prefix);
    close(pipeFd);
}

class OexConfig{
    public:
    String exe;
    bool fprStdout;
    String preload;
    String socketAddress;
    bool setLibPath;
    NameValueMap environment;
};

#ifdef AVNAV_ANDROID
    static OexConfig oexconfig={
        String("liboexserverd.so"),
        true,
        String(""),
        String("com.opencpn.ocharts_pi"),
        false
    };
#else
    static OexConfig oexconfig={
        String("oexserverd"),
        false,
        String("libpreload.so"),
        String("com.opencpn.ocharts_pi"),
        true,
        {{String(TEST_PIPE_ENV),String("LOCAL:com.opencpn.ocharts_pi")}}
    };
#endif
static int waitChild(int pid, int waitSeconds)
{
    int rt = -1;
    int status = 0;
    Timer::SteadyTimePoint start=Timer::steadyNow();
    int64_t waitMillis=waitSeconds*1000;
    while (((rt=waitpid(pid,&status,WNOHANG))  >= 0) || ((rt=waitpid(- pid,&status,WNOHANG))  >= 0)){
        if (Timer::steadyPassedMillis(start,waitMillis)){
            return -1;
        }
        if (rt == 0){
            Timer::microSleep(10000);
        }
    }
    return 0;
}
class StartResult{
    public:
    int pid=-1;
    int readFd=-1;
    bool hasError=false;
    String error;
    void setError(const String &error){
        this->error=error;
        hasError=true; 
    }
};
/**
 * run the oexserverd
 * only use LD_PRELOAD if args are empty
 */
StartResult runOexServerd(const OexConfig &config,const String &progDir,const StringVector &args, bool usePreload){    
    LOG_INFO("starting oexserverd with param %s",StringHelper::concat(args).c_str());
    StartResult rt;
    String exe=FileHelper::concatPath(progDir,oexconfig.exe);
    if (!FileHelper::exists(exe)){
        rt.setError(FMT("oexserverd %s not found", exe.c_str()));
        LOG_ERROR("%s",rt.error.c_str());
        return rt;
    }
    pid_t parent=getpid();
    int rwfds[2];
    if (pipe(rwfds) != 0){
        rt.setError(FMT("unable to create pipes for oexserverd: %s",SystemHelper::sysError().c_str()));
        LOG_ERROR("%s",rt.error.c_str());
        return rt;
    }
    int childRead=rwfds[0];
    int writePipe=rwfds[1];
    pid_t pid=fork();
    if (pid == 0){
        //child
        close(childRead);
        dup2(writePipe,1);
        dup2(writePipe,2);
        int rt=setpgid(0,0); //have all children in a new process group
        if ( rt != 0){
            std::cerr << "oexserverd: unable to set process group id:" << SystemHelper::sysError() << std::endl;
        }
        if (! FileHelper::exists(exe)){
            std::cerr << "oexserverd" << exe << " not found " << std::endl;
            exit(1);
        }
        for (auto it=config.environment.begin();it != oexconfig.environment.end();it++){
            putenv(StringHelper::cloneData(StringHelper::format("%s=%s",it->first.c_str(),it->second.c_str())));
        }
        putenv(StringHelper::cloneData(StringHelper::format("%s=%d",ENV_AVNAV_PID,parent)));
        if (config.setLibPath){
            LOG_DEBUG("setting LD_LIBRARY_PATH to %s",progDir);
            putenv(StringHelper::cloneData(StringHelper::format("LD_LIBRARY_PATH=%s",progDir)));
        }
        else{
            unsetenv("LD_LIBRARY_PATH");
        }
        if (config.preload != String("") && usePreload){
            String preload=FileHelper::concatPath(progDir,oexconfig.preload);
            putenv(StringHelper::cloneData(StringHelper::format("%s=%s","LD_PRELOAD",preload.c_str()))); 
        }
        if (Logger::instance()->HasLevel(LOG_LEVEL_DEBUG)){
            putenv(StringHelper::cloneData("AVNAV_OCHARTS_DEBUG=1"));
        }
        int numArgs=args.size();
        char * eargs[numArgs+2];
        eargs[0]=StringHelper::cloneData(exe);
        int idx=1;
        for (auto it=args.begin();it!=args.end();it++){
            eargs[idx]=StringHelper::cloneData(*it);
            idx++;
        }
        eargs[numArgs+1]=NULL;
        execv(exe.c_str(), eargs);
        std::cerr << "unable to exec: "<< SystemHelper::sysError() << std::endl;
        exit(1);
    }
    close(writePipe);
    if (pid < 0){
        rt.setError(FMT("unable to fork %s: %s",exe.c_str(),SystemHelper::sysError().c_str()));
        LOG_ERROR("%s",rt.error.c_str());
        return rt;
    }
    rt.pid=pid;
    rt.readFd=childRead;
    return rt;    
}

OexControl::Ptr OexControl::_instance;
OexControl::OexControl(String progDir,String tempDir,StringVector additionalParameters)
{
    this->progDir = progDir;
    this->tempDir = tempDir;
    this->additionalParameters=additionalParameters;
    lastDongleState=Timer::addMillis(Timer::steadyNow(),-QUERY_DONGLE_MS);
}
OexControl::~OexControl()
{
    if (GetState() == RUNNING)
    {
        Stop();
    }
}
OexControl::OexState OexControl::GetState() {
    Synchronized l(mutex);
    return _state;
}
OexControl::OexState OexControl::GetRequestedState() {
    Synchronized l(mutex);
    return _requestedState;
}

bool OexControl::SetState(OexControl::OexState state,String error) {
    Synchronized l(mutex);
    OexState oldState=_state;
    _state=state;
    lastError=error;
    return _state != oldState;
}
bool OexControl::SetRequestedState(OexControl::OexState state) {
    Synchronized l(mutex);
    OexState oldState=_requestedState;
    _requestedState=state;
    return _requestedState != oldState;
}
OexControl::Supervisor::Supervisor(OexControl::Ptr control) : Thread(){
    this->control=control;
}
void OexControl::Kill(){
    int spid=pid; //atomic
    if (spid > 0){
        LOG_INFOC("killing oexserver %d",spid);
        kill(spid,SIGKILL);
        kill(-spid,SIGKILL);
    }
}
void OexControl::StopServer(pid_t pid)
{
    LOG_INFO("stopping oexserver with pid %d",pid);
    kill(-pid, SIGTERM);
    kill(pid, SIGTERM);
    int rt = waitChild(-pid, STOPTIME);
    LOG_INFO("stopping oexserverd returned %d", rt);
    if (rt < 0)
    {
        LOG_INFO("trying kill oexserverd");
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
        rt = waitChild(pid, STOPTIME);
        LOG_INFO("kill oexserverd returned %d", rt);
        if (rt < 0)
        {
            LOG_ERROR("finally unable to stop oexserverd");
        }
    }
}
bool OexControl::PingOex(long waitTime)
{
    InputStream::Ptr stream;
    try
    {
        stream = DoSendOexCommand(CMD_TEST_AVAIL, "", "", waitTime, true);
    }
    catch (const OpenException &e)
    {
        LOG_DEBUG("opening oexserver stream failed with %s", e.what());
        return false;
    }
    if (stream)
    {
        LOG_DEBUG("successfully connected to oexserverd");
        char buffer[2];
        ssize_t rd = stream->read(buffer, 2, 500);
        stream->close();
        if (rd != 2)
        {
            LOG_DEBUG("could not read 4 bytes from oexserver");
        }
        else
        {
            if (memcmp(buffer, "OK", 2) == 0)
            {
                LOG_DEBUG("received OK from oexserver");
                return true;
            }
        }
    }
    return false;
}
void OexControl::Supervisor::run()
{
    static const std::map<OexState,int> waitTimes={
        {UNKNOWN,100},
        {ERROR,2000},
        {RUNNING,500},
        {STARTING,100}
    };
    LOG_DEBUG("oexserverd supervisor started");
    int pid = -1;
    Timer::SteadyTimePoint lastCheck=Timer::steadyNow();
    while (!this->shouldStop())
    {
        OexState state = control->GetState();
        auto it=waitTimes.find(state);
        this->waitMillis((it != waitTimes.end())?it->second:500);
        state = control->GetState();
        OexState requested = control->GetRequestedState();
        if (requested != state)
        {
            // actions
            switch (requested)
            {
            case RUNNING:
            {
                LOG_INFO("oexserverd start requested");
                StartResult res = runOexServerd(oexconfig, control->progDir, control->additionalParameters,true);
                if (res.hasError)
                {
                    control->SetState(ERROR,res.error);
                    pid = -1;
                    control->pid=pid;
                }
                else
                {
                    Thread reader([this, res]()
                                  { pipeReader(res.readFd, FMT("OEXSERVER:%d",res.pid), logWrite); });
                    reader.start();
                    reader.detach();
                    pid=res.pid;
                    control->pid=pid;
                    control->SetState(STARTING);
                    Timer::SteadyTimePoint start=Timer::steadyNow();
                    bool connectSuccess=false;
                    while (! connectSuccess && ! Timer::steadyPassedMillis(start,10000)){
                        connectSuccess=control->PingOex(3000);
                        if (! connectSuccess) Timer::microSleep(500000);
                    }
                    if (! connectSuccess){
                        control->SetState(ERROR,"unable to connect to oexserverd after start");
                        control->StopServer(pid);
                        pid=-1;
                        control->pid=pid;
                    }
                    else{
                        control->SetState(RUNNING);
                        LOG_INFO("oexserverd %d successfully started and connected",pid);
                    }
                }
            }
            break;
            default:
                if (state == RUNNING && pid > 0)
                {
                    LOG_INFO("stop oexserverd requested");
                    control->StopServer(pid);
                    pid = -1;
                }
                control->SetState(requested);
            }
        }
        if (pid > 0) {
            if (control->GetState() == RUNNING){
            bool available = false;
            if (Timer::steadyPassedMillis(lastCheck, 2000)) {
                available = control->PingOex(1000);
                lastCheck=Timer::steadyNow();
            } else {
                Timer::SteadyTimePoint start = Timer::steadyNow();
                while (!Timer::steadyPassedMillis(start, 200)) {
                    if ((kill(pid, 0) == 0) || (kill(-pid, 0) == 0)) {
                        available = true;
                        break;
                    }
                    Timer::microSleep(5000);
                }
                if (! available){
                    LOG_DEBUG("unable to kill -0 oexserver %d: %s", pid, SystemHelper::sysError());
                    //if kill did not succeed, we try to ping any way
                    available=control->PingOex(1000);
                }
            }
            int rt = waitpid(-pid, NULL, WNOHANG);
            if (!available) {
                bool changed = control->SetState(ERROR, FMT("stopped unexpectedly, code=%d", rt));
                if (changed) {
                    LOG_ERROR("oexserverd stopped unexpectedly, rt=%d", rt);
                    pid = -1;
                    control->pid=pid;
                }
            }
            }
            else{
                //just collect our zombies...
                waitpid(-pid, NULL, WNOHANG);
            }

        }
    }
    LOG_DEBUG("oexserverd supervisor stopped");
}

void OexControl::Start() {
    SetRequestedState(RUNNING);
    Synchronized l(mutex);
    if (supervisor){
        supervisor->wakeUp();
    }
    
}
void OexControl::Stop() {
    SetRequestedState(UNKNOWN);
    Synchronized l(mutex);
    if (supervisor){
        supervisor->wakeUp();
    }
}
void OexControl::Restart(String reason) {
    LOG_INFO("restarting oexserverd due to %s",reason.c_str());
    Stop();
    Start();
}
bool OexControl::WaitForState(OexControl::OexState state, long timeoutMillis){
    Timer::SteadyTimePoint start=Timer::steadyNow();
    while(GetState() != state){
        if (Timer::steadyPassedMillis(start,timeoutMillis)) return false;
        Timer::microSleep(10000);
    }
    return true;
}
void OexControl::Init(String progDir,String tempDir,StringVector additionalParameters)
{
    if (_instance.get() != NULL)
        _instance.reset();
    if (FileHelper::exists(tempDir,true)){    
        StringVector tempDirs=FileHelper::listDir(tempDir,TMP_PREFIX "*");
        for (auto it=tempDirs.begin();it != tempDirs.end();it++){
            if (FileHelper::exists(*it,true)){
                LOG_DEBUG("removing existing temp dir %s",*it);
                FileHelper::emptyDirectory(*it,true);
            }
        }      
    }
    _instance = OexControl::Ptr(new OexControl(progDir,tempDir,additionalParameters));
    _instance -> supervisor= new OexControl::Supervisor(_instance);
    _instance -> supervisor->start();
}
OexControl::Ptr OexControl::Instance()
{
    return _instance;
}
String OexControl::GetLastError(){
    Synchronized l(mutex);
    return lastError;
}
void OexControl::ToJson(StatusStream &stream){
    static const std::map<OexState,const char *> stateStrings={
        {UNKNOWN,"Down"},
        {ERROR,"Error"},
        {RUNNING,"Running"},
        {STARTING,"Starting"}
    };
    OexState state=GetState();
    auto it=stateStrings.find(state);
    stream["state"]=(it != stateStrings.end())?it->second:"Unknown";
    stream["version"]=OCHARTS_TEXT;
    if (state == ERROR){
        stream["info"]=GetLastError();
    }
}

int OexControl::OpenConnection(long timeoutMillis, bool ignoreState) {
    if ( !ignoreState && GetState() != RUNNING){
        throw OpenException("oexserverd not running");
    }
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    if (oexconfig.socketAddress.size() >= (sizeof(serv_addr.sun_path) -1)){
        throw OpenException(FMT("socket name %s too long",oexconfig.socketAddress.c_str()));
    }
    serv_addr.sun_path[0] = '\0';  /* abstract namespace */
    strcpy(serv_addr.sun_path+1, oexconfig.socketAddress.c_str());
    int slen=offsetof(struct sockaddr_un, sun_path)+strlen(serv_addr.sun_path+1)+1;
    serv_addr.sun_family = AF_LOCAL;
    int sockFd=socket(AF_LOCAL, SOCK_STREAM, PF_UNIX);
    if (sockFd < 0){
        throw OpenException(FMT("unable to create socket: %s",SystemHelper::sysError().c_str()));
    }
    int retval = fcntl( sockFd, F_SETFL, fcntl(sockFd, F_GETFL) | O_NONBLOCK);
    if (retval < 0){
        close(sockFd);
        throw OpenException(FMT("unable to set non blocking: %s",SystemHelper::sysError().c_str()));
    }
    retval=connect(sockFd,(sockaddr *)(&serv_addr),slen);
    if (retval < 0){
        if (errno != EINPROGRESS){
            close(sockFd);
            throw OpenException(FMT("unable to connect (errno %d): %s",errno,SystemHelper::sysError(errno).c_str()));
        }
        struct pollfd fds[1];
        fds[0].fd = sockFd;
        fds[0].events = POLLIN|POLLOUT;
        retval=poll(fds,1,timeoutMillis);
        if (! (fds[0].revents & (POLLIN|POLLOUT))){
            close(sockFd);
            throw OpenException("timeout connecting");
        }
        int errorCode=-2;
        socklen_t elen=sizeof(errorCode);
        getsockopt(sockFd, SOL_SOCKET, SO_ERROR,
               &errorCode, &elen);
        if (errorCode < 0){
            close(sockFd);
            throw OpenException(FMT("socket not connected after waiting: %s",SystemHelper::sysError(errorCode).c_str()));
        }
    }
    return sockFd;
}

InputStream::Ptr OexControl::SendOexCommand(OexCommands cmd,const String &fileName, const String &key, long waitTime){
    return DoSendOexCommand(cmd,fileName,key,waitTime);
}
InputStream::Ptr OexControl::DoSendOexCommand(OexCommands cmd,const String &fileName,
     const String &key, long waitTime, bool ignoreState){
    LOG_DEBUG("send oex command %d for file %s",(int)cmd,fileName.c_str());
    if (fileName.size()>= (sizeof(OexMessage::senc_name)-1)){
        throw OpenException(FMT("fileName %s too long",fileName.c_str()));
    }
    if (key.size() >= (sizeof(OexMessage::senc_key)-1)){
        throw OpenException(FMT("key %s too long for %s",key.c_str(),fileName.c_str()));
    }
    Timer::SteadyTimePoint start=Timer::steadyNow();
    int fd =OpenConnection(waitTime,ignoreState);
    OexMessage msg;
    msg.cmd=cmd;
    strncpy(msg.senc_key,key.c_str(),sizeof(OexMessage::senc_key));
    msg.senc_key[sizeof(OexMessage::senc_key)-1]=0;
    strncpy(msg.senc_name,fileName.c_str(),sizeof(OexMessage::senc_name));
    msg.senc_name[sizeof(OexMessage::senc_name)-1]=0;
    struct pollfd fds[1];
    fds[0].fd=fd;
    fds[0].events=POLLOUT;
    size_t written=0;
    String error;
    while (written < sizeof(OexMessage) && !Timer::steadyPassedMillis(start, waitTime))
    {
        int rt = poll(fds, 1, Timer::remainMillis(start, waitTime));
        if (rt < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                error = StringHelper::format("unable to write msg to oexserver for %s: %s", fileName.c_str(), SystemHelper::sysError().c_str());
                break;
            }
            continue;
        }
        if (fds[0].revents)
        {
            if (fds[0].revents & (POLLHUP | POLLERR))
            {
                error = StringHelper::format("oexserver closed connection for %s", fileName.c_str());
                break;
            }
            ssize_t wr = ::write(fd, &msg + written, sizeof(OexMessage) - written);
            if (wr < 0)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    error = StringHelper::format("unable to write msg to oexserver for %s: %s", fileName.c_str(), SystemHelper::sysError().c_str());
                    break;
                }
            }
            else
            {
                written += wr;
            }
        }
    }
    if (written != sizeof(OexMessage))
    {
        if (error.empty()){
            error=StringHelper::format("only %d bytes written to oexserver for %s",written,fileName.c_str());
        }
        LOG_ERROR("%s",error);
        close(fd);
        throw OpenException(error);
    }
    //the stream is in non blocking mode now, just create an inputStream
    LOG_DEBUG("opened oex stream for for %s",fileName.c_str());
    InputStream::Ptr rt=std::make_shared<InputStream>(fd,4096);
    bool check=rt->CheckRead(Timer::remainMillis(start,waitTime));
    if (! check){
        String error=FMT("unable to read from oexserverd for %s",fileName);
        LOG_ERROR("%s",error);
        throw OpenException(error);
    }
    return rt;
}

long OexControl::getNextTempIdx(){
    Synchronized l(mutex);
    tempDirIdx++;
    return tempDirIdx;
}

static String execOexCommand(const OexConfig &config,const String &progDir,const StringVector &args, long timeoutMillis){
    LOG_INFO("starting FPR command %s",StringHelper::concat(args));
    StartResult res = runOexServerd(config, progDir, args,false);
    if (res.hasError){
        throw OexControl::OexException(FMT("unable to run oex command %s: %s",StringHelper::concat(args), res.error));
    }
    String result;
    const size_t MAXRES=204800;
    Timer::SteadyTimePoint start=Timer::steadyNow();
    pipeReader(res.readFd, FMT("OEXCMD:%d", res.pid), [&result](const String &prfx, const char *text){
                      if (result.size() >= MAXRES) return;
                      result.append(text);
                  },
                  [start,timeoutMillis](){
                      return Timer::steadyPassedMillis(start,timeoutMillis);
                  }
                  );
    bool finished=false;
    while (! Timer::steadyPassedMillis(start,timeoutMillis)){
        int rt=waitpid(res.pid,NULL,WNOHANG);
        if (rt == res.pid){
            //success
            finished=true;
            break;
        }
        Timer::microSleep(50000);
    }
    if (! finished){
        kill(res.pid,9); //terminate oex
        throw OexControl::OexException("fpr command did not terminate");
    }
    return result;
}

OexControl::FPR OexControl::GetFpr(long timeoutMillis,bool forDongle,bool alternative){
    String fprDir;
    avnav::VoidGuard guard([&fprDir](){
        if (fprDir.empty()) return;
        if (FileHelper::exists(fprDir,true)){
            FileHelper::emptyDirectory(fprDir,true);
        }
    });
    StringVector args;
    String prefix;
    if (!oexconfig.fprStdout){
        //normal linux, not android
        fprDir=FileHelper::concatPath(tempDir,FMT("%s%d",TMP_PREFIX,getNextTempIdx()));
        if (FileHelper::exists(fprDir,true)){
            FileHelper::emptyDirectory(fprDir,true);
        }
        LOG_DEBUG("creating temp dir %s for fpr",fprDir);
        FileHelper::makeDirs(fprDir);
        args.push_back(forDongle?"-k":"-g");
        args.push_back(fprDir+"/");
    }
    else{
        //android see doUploadFpr in ochartsShop.cpp
        //older versions: additional parameters have -z
        //we must use -g and the prefix "oc03R_"
        //newer versions: additional parameters have -y
        //we must use -k and the prefix "oc04R_"
        String action="-g";
        prefix="oc03R_";
        String zParameter;
        String yParameter;
        if (additionalParameters.size() >0){
            //try to determine the -y or -z parameters
            //if -y is found this will win (except if alternative is set)
            String lastFlag;
            for (auto it=additionalParameters.begin();it != additionalParameters.end();it++){
                if (lastFlag == "-y") {
                    yParameter=*it;
                    lastFlag.clear();
                    continue;
                }
                if (lastFlag == "-z"){
                    zParameter=*it;
                    lastFlag.clear();
                    continue;
                }
                if (it->size()>0 && (*it)[0] == '-' ){
                    lastFlag=*it;
                }
                else{
                    lastFlag.clear();
                }
            }
            if (!yParameter.empty() && ! alternative){
                prefix="oc04R_";
                action="-k";
                args.push_back("-y");
                args.push_back(yParameter);
            }
            else{
                if (! zParameter.empty()){
                    args.push_back("-z");
                    args.push_back(zParameter);
                }
            }
        }
        args.push_back(action);
    }
    String result=execOexCommand(oexconfig,progDir,args,timeoutMillis);
    if (result.empty()){
        throw OexException("no data from fpr command");
    }
    FPR rt;
    if (oexconfig.fprStdout){
        //the data has directly been read
        StringHelper::replaceInline(result,"\n","");
        StringVector parts=StringHelper::split(result,";");
        if (parts.size() != 2){
            throw OexException("invalid fpr data returned from oex, missing ;");
        }
        //TODO: some checks
        rt.name=prefix+parts[0]+".fpr";
        rt.value=parts[1];
    }
    else{
        //get the fpr name...
        StringVector lines=StringHelper::split(result,"\n");
        String fprName;
        for (auto it=lines.begin();it!=lines.end();it++){
            
            if (it->find("ERROR") != String::npos || it->find("error") != String::npos){
                throw OexException(*it);
            }
            if (it->find("FPR") != String::npos || it->find("fpr") != String::npos){
                size_t delim=it->find(':');
                if (delim != String::npos && delim < (it->size()-1)){
                    fprName=it->substr(delim+1);
                    StringHelper::trimI(fprName,' ');
                    break;
                }
            }
        }
        if (fprName.empty()){
            throw OexException("no filename in oex output");
        }
        rt.name=FileHelper::fileName(fprName);
        FileHelper::FileInfo info=FileHelper::getFileInfo(fprName);
        if (info.size == 0){
            throw OexException(forDongle?"dongle not present":"no file created by oex");
        }
        std::ifstream fprStream(fprName);
        if (!fprStream.good()){
            throw OexException(FMT("cannot read fpr file %s",fprName));
        }
        char v;
        std::stringstream out;
        out.fill('0');
        while (fprStream.get(v)){
            out.width(2);
            out << std::hex << (unsigned int)v;
        }
        rt.value=out.str();
        if (rt.value.empty()){
            throw OexException("empty fpr file");
        }
        rt.forDongle=forDongle;
    }
    return rt;
}

bool OexControl::DonglePresent(long timeoutMillis){
    if (oexconfig.fprStdout){
        //ANDROID
        return false;
    }
    {
        Synchronized l(mutex);
        if (!Timer::steadyPassedMillis(lastDongleState,QUERY_DONGLE_MS)){
            return donglePresent;
        }
        lastDongleState=Timer::steadyNow(); //prevent parallel execution
    }
    Timer::SteadyTimePoint start=Timer::steadyNow();
    StringVector args({"-s"});
    String result=execOexCommand(oexconfig,progDir,args,timeoutMillis);
    StringVector lines=StringHelper::split(result,"\n");
    bool found=false;
    bool newState=false;
    for (auto it=lines.begin();it!=lines.end();it++){
        if (*it == "0") {
            LOG_INFO("oex dongle present false");
            found=true;
            newState=false;
            break;
        }
        if (*it == "1") {
            LOG_INFO("oex dongle present true");
            found=true;
            newState=true;
            break;
        }
    }
    String newName="";
    if (found){
        newName=GetDongleNameInternal(Timer::remainMillis(start,timeoutMillis));
    }
    {
        Synchronized l(mutex);
        donglePresent=newState;
        dongleName=newName;
        if (! found){
            throw OexException(StringHelper::concat(lines," "));
        }
    }
    return newState;
}
String OexControl::GetDongleName(long timeoutMillis){
    if (! DonglePresent(timeoutMillis)) {
       throw OexException("no dongle detected"); 
    }
    Synchronized l(mutex);
    return dongleName;
}
String OexControl::GetDongleNameInternal(long timeoutMillis){
    Timer::SteadyTimePoint start=Timer::steadyNow();
    StringVector args({"-t"});
    String result=execOexCommand(oexconfig,progDir,args,timeoutMillis);
    StringVector lines=StringHelper::split(result,"\n");
    String rt;
    for (auto it=lines.begin();it!=lines.end();it++){
        long sn=::atol(it->c_str());
        rt=FMT("sgl%08X",sn);
    }
    return rt;
}

String OexControl::GetOchartsVersion() const{
    return OCHARTS_TEXT;
}
bool OexControl::CanHaveDongle() const{
    return ! oexconfig.fprStdout; //not on android
}

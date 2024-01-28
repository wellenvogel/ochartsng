#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syscall.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "EnvDefines.h"


//simple automatic unlocking mutex
typedef std::unique_lock<std::mutex> Synchronized;

static std::mutex openLock;
typedef int (*OpenFunction)(const char *pathname, int flags,...);
typedef int (*CloseFunction)(int fd);
#define OCPN_PIPE "/tmp/OCPN_PIPEX"
OpenFunction o_open=NULL;
CloseFunction o_close=NULL;
int pipeFd=-1;
int writeFd=-1;
int listenFd=-1;
bool useSockets=false;
bool debug=false;

#define DPRINTF(...) {if (debug) {printf("(%d)",getpid());printf(__VA_ARGS__);fflush(stdout);}}

static std::thread *listener=NULL;

static const int REQUEST_SIZE=1025;
const char * PRFX="int://";
class Request{
    public:
    int fd=-1;
    unsigned char buffer[REQUEST_SIZE];
    size_t bytesReceived=0;
    bool requestSend=false;
    time_t starttime=0;
    Request(){
    }
    typedef enum{
        ERROR,
        EMPTY,
        WAITING,
        RECEIVED,
        SEND
    } Status;
    Status status;
    Status addReceivedData(const unsigned char *buffer, int sz){
        if (status != WAITING) return ERROR;
        int mx=REQUEST_SIZE-bytesReceived;
        if (sz > mx) sz=mx;
        memcpy(this->buffer+bytesReceived,buffer,sz);
        bytesReceived+=sz;
        if (bytesReceived >= REQUEST_SIZE) status=RECEIVED;
        return status;
    }
    void setFd(int fd){
        this->fd=fd;
        status=WAITING;
        starttime=time(NULL);
        bytesReceived=0;
    }
    void free(bool doClose=false){
        if (doClose && fd >= 0){
            close(fd);
        }
        fd=-1;
        status=EMPTY;
    }
    bool hasTimeout(time_t now){
        if (status == EMPTY) return false;
        return (now < starttime || now > (starttime+10));
    }
};

class RequestList{
    private:
    static const int MAX=10;
    Request requests[MAX];
    struct pollfd fds[MAX+1];
    std::mutex lock;
    int findFree(){
        for (int i=0;i<MAX;i++){
            if (requests[i].fd < 0) return i;
        }
        return -1;
    }
    int findByFd(int fd){
        for (int i=0;i<MAX;i++){
            if (requests[i].fd == fd) return i;
        }
        return -1; 
    }
    public:
    bool addRequest(int fd){
        Synchronized l(lock);
        int idx=findFree();
        if (idx < 0) return false;
        requests[idx].setFd(fd);
        return true;
    }
    Request::Status addReceivedData(int fd,const unsigned char *buffer, int sz){
        Synchronized l(lock);
        int idx=findByFd(fd);
        if (idx < 0) return Request::ERROR;
        Request::Status rt=requests[idx].addReceivedData(buffer,sz);
        DPRINTF("add %d bytes for %d res=%d\n",sz,fd,(int)rt);
        return rt;
    }
    //data buffer must have REQUEST_SIZE
    int getNextForSend(unsigned char *dataBuffer){
        Synchronized l(lock);
        for (int i=0;i<MAX;i++){
            if (requests[i].status == Request::RECEIVED){
                requests[i].status = Request::SEND;
                memcpy(dataBuffer,requests[i].buffer,REQUEST_SIZE);
                snprintf((char *)dataBuffer+1,256,"%s%d",PRFX,requests[i].fd);
                fcntl( requests[i].fd, F_SETFL, fcntl(requests[i].fd, F_GETFL) & ~O_NONBLOCK);
                return requests[i].fd;
            }
        }
        return -1;
    }
    void close(int fd){
        Synchronized l(lock);
        int idx=findByFd(fd);
        if (idx < 0) return;
        requests[idx].free(true);
    }
    void housekeeping(){
        Synchronized l(lock);
        time_t now=time(NULL);
        for (int i=0;i<MAX;i++){
            if (requests[i].hasTimeout(now)){
                requests[i].free(true);
            }
        }
    }
    void free(int fd, bool doClose=false){
        Synchronized l(lock);
        int idx=findByFd(fd);
        if (idx < 0) return;
        requests[idx].free(doClose);
    }

    struct pollfd *getFds(int listenFd,/*out*/int &count){
        Synchronized l(lock);
        int idx=0;
        if (listenFd >= 0 && findFree() >= 0){
            fds[idx].fd=listenFd;
            fds[idx].events=POLLIN;
            idx++;
        }
        for (int i=0;i<MAX;i++){
            if (requests[i].status == Request::WAITING){
                fds[idx].fd=requests[i].fd;
                fds[idx].events=POLLIN;
                idx++;
            }
        }
        count=idx;
        return &fds[0];
    }
};
static RequestList requests;

int createListener(int port){
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        DPRINTF("unable to bind to port %d\n",port);
        return -1;
    }
    listen(listenfd,5);
    return listenfd;
}
int createListenerLocal(const char* name){
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    int nameLen = strlen(name);
    if (nameLen >= (int) sizeof(serv_addr.sun_path) -1) { 
        DPRINTF("name %s too long\n",name);
        return -1;
    }
    serv_addr.sun_path[0] = '\0';  /* abstract namespace */
    strcpy(serv_addr.sun_path+1, name);
    serv_addr.sun_family = AF_LOCAL;
    int listenfd = socket(AF_LOCAL, SOCK_STREAM, PF_UNIX);
    if (listenfd < 0){
        DPRINTF("unable to create socket: %d\n",errno);
        return listenfd;
    }
    if (bind(listenfd, (struct sockaddr*)&serv_addr, offsetof(struct sockaddr_un, sun_path)+strlen(serv_addr.sun_path+1)+1) < 0){
        DPRINTF("unable to bind to name %s\n",name);
        return -1;
    }
    listen(listenfd,5);
    DPRINTF("listener created with fd %d\n",listenfd);
    return listenfd;
}


static const int BUFSIZE=512;
void listenFunction(){
    DPRINTF("listener started on fd %d\n",listenFd);
    unsigned char buffer[BUFSIZE];
    unsigned char sendBuffer[REQUEST_SIZE];
    while(true){
        if (!useSockets)
        {
            struct pollfd fds[1];
            fds[0].fd = listenFd;
            fds[0].events = POLLIN;
            int pres = poll(fds, 1, 1000);
            if (fds[0].revents & POLLIN)
            {
                ssize_t bRead = read(listenFd, buffer, BUFSIZE);
                if (bRead > 0)
                {
                    write(writeFd, buffer, bRead);
                }
                else
                {
                    break;
                }
            }
        }
        else{
            int numFd=0;
            pollfd * fds=requests.getFds(listenFd,numFd);
            int pres = poll(fds, numFd, 1000);
            for (int idx=0;idx < numFd;idx++){
                if (fds[idx].revents){
                    if (fds[idx].fd == listenFd){
                        struct sockaddr client_addr;
                        socklen_t addrlen=sizeof(client_addr);
                        //usleep(1000000);
                        int client=accept4(listenFd,&client_addr,&addrlen,SOCK_NONBLOCK);
                        if (client >= 0){
                            DPRINTF("new client connected, fd=%d\n",client);
                            if (!requests.addRequest(client)){
                                DPRINTF("too many clients\n");
                                close(client);
                            }
                        }
                    }
                    else{
                        ssize_t bRead = read(fds[idx].fd, buffer, BUFSIZE);
                        if (bRead == 0){
                            requests.free(fds[idx].fd,true);
                        }
                        else{
                            if (bRead > 0){
                                DPRINTF("received %d bytes for %d\n",(int)bRead,fds[idx].fd);
                                requests.addReceivedData(fds[idx].fd,buffer,bRead);
                            }
                        }
                    }

                }
            }
            int nextSend=-1;
            while ((nextSend=requests.getNextForSend(sendBuffer)) >= 0){
                DPRINTF("send request %d for fd %d, fn=%s\n",sendBuffer[0],nextSend,sendBuffer+257);
                write(writeFd,sendBuffer,REQUEST_SIZE);
            }
        }
        requests.housekeeping();
        
    }
    DPRINTF("listener stopped\n");
}

class Monitor{
    std::thread *monitorThread=NULL;
    public:
        Monitor(){
            const char *pidenv=getenv(ENV_AVNAV_PID);
            if (pidenv == NULL) return;
            int parent=atoi(pidenv);
            if (parent <= 0){
                fprintf(stderr,"invalid parent pid %d\n",parent);
                return;
            }
            monitorThread= new std::thread([parent]{
                fprintf(stderr, "(%d)start parent monitoring for %d\n",getpid(),parent);
                while(true){
                    int rt=kill(parent,0);
                    if (rt != 0){
                        fprintf(stderr,"(%d) parent %d is not running any more, stopping",getpid(),parent);
                        exit(0);
                        break;
                    }
                    sleep(1);
                }
                fprintf(stderr,"(%d) server monitoring stopped\n",getpid());
            });
            monitorThread->detach();
        }
};

static Monitor *monitor=NULL;


void setOpenFunction()
{
    if (o_open == NULL)
    {
        o_open = (OpenFunction)dlsym(RTLD_NEXT, "open");
    }
    if (o_close == NULL){
        o_close = (CloseFunction)dlsym(RTLD_NEXT,"close");
    }
}

extern "C" {

    int open(const char *pathname, int flags, ...)
    {
        Synchronized l(openLock);
        setOpenFunction();
        va_list argp;
        va_start(argp, flags);
        DPRINTF("###open: %s\n",pathname);
        if (strcmp(pathname, OCPN_PIPE) == 0)
        {
            const char *dbg=getenv("AVNAV_OCHARTS_DEBUG");
            if (dbg) debug=true;
            DPRINTF("preload debug enabled\n");
            DPRINTF("parent monitoring is %s\n",(monitor?"active":"not active"));
            if (! monitor){
                monitor=new Monitor();
            }
            if (pipeFd >= 0){
                return pipeFd;
            }
            const char *mp = getenv(TEST_PIPE_ENV);
            if (mp != NULL)
            {
                DPRINTF("open translate %s to %s\n", pathname, mp);
                pathname = mp;
                if (listener == NULL){
                    if (strncmp(mp,"TCP:",4) == 0){
                        int port=atoi(mp+4);
                        DPRINTF("creating socket listener at %d\n",port);
                        if (port <= 0){
                            DPRINTF("invalid port in %s\n",mp);
                            return -1;
                        }
                        listenFd=createListener(port);
                        useSockets=true;
                    }
                    else if (strncmp(mp,"LOCAL:",6) == 0){
                        DPRINTF("creating af_local socket listener at %s\n",mp+6);
                        listenFd=createListenerLocal(mp+6);
                        useSockets=true;
                    }
                    else{
                        listenFd=o_open(pathname, flags, va_arg(argp, int));
                    }
                    if (listenFd < 0) {
                        DPRINTF("unable to create listenFd\n");
                        return listenFd;
                    }
                    int fds[2];
                    int rt=pipe(fds);
                    if (rt < 0) {
                        DPRINTF("unable to open forward pipe\n");
                        return -1;
                    }
                    writeFd=fds[1];
                    int retval = fcntl( listenFd, F_SETFL, fcntl(listenFd, F_GETFL) | O_NONBLOCK);
                    pipeFd=fds[0];
                    listener= new std::thread(listenFunction);
                }
                return pipeFd;
            }
        }
        if (strncmp(pathname,PRFX,strlen(PRFX)) == 0){
            int fd=atoi(pathname+strlen(PRFX));
            DPRINTF("open internal: %d\n",fd);
            requests.free(fd);
            return fd;
        }
        return o_open(pathname, flags, va_arg(argp, int));
    }

    int close(int fd){
        Synchronized l(openLock);
        setOpenFunction();
        if (fd == pipeFd){
            return 0;
        }
        DPRINTF("###close %d\n",fd);
        return o_close(fd);
    }  
}
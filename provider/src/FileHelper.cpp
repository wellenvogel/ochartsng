/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  File Helper
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
#include "FileHelper.h"
#include "Testing.h"
#include "SystemHelper.h"
#include "Timer.h"
#include "Logger.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>

#include <sstream>
#include <filesystem.hpp>

#define FSNS ghc::filesystem

#define PATH_SEP "/"

#ifdef AVTEST
    namespace avtest{
      int poll(struct pollfd *fds, nfds_t nfds, int timeout);
      ssize_t read(int fd, void *buf, size_t count);
    } 
#endif 

String FileHelper::concatPath(const String &p1, const String &p2){
    std::stringstream out;
    out << p1;
    out << PATH_SEP;
    out << p2;
    return out.str();
}
String FileHelper::dirname(const String &path){
    FSNS::path p(path);
    return p.parent_path().string();
}
String FileHelper::fileName(const String &path, bool stripExtension){
    if (path.empty()) return path;
    FSNS::path p(path);
    if (stripExtension) return p.stem().string();
    return p.filename().string();
}
String FileHelper::realpath(const String &path){
    if (path.empty()) return path;
    FSNS::path p(path);
    FSNS::path abs=FSNS::absolute(p);
    if (FSNS::exists(abs)){
        return FSNS::canonical(abs).string();
    }
    else{
        return abs;
    }
}
String FileHelper::ext(const String &path){
    return StringHelper::afterLast(fileName(path),".");
}
bool FileHelper::exists(const String &filename, bool directory){
    struct stat64 buffer;
    if (stat64(filename.c_str(),&buffer) != 0) return false;
    if (S_ISDIR(buffer.st_mode)) return directory;
    return ! directory;
}
int64_t FileHelper::fileSize(const String &name){
    struct stat64 data;
    int rt=stat64(name.c_str(), &data);
    if (rt < 0) return -1;
    return data.st_size;
}

int64_t FileHelper::fileTime(const String &name){
    struct stat64 data;
    int rt=stat64(name.c_str(), &data);
    if (rt < 0) return 0;
    return data.st_mtim.tv_sec*1000 + data.st_mtim.tv_nsec / 1000000;
}
bool FileHelper::rename(const String &name, const String &newName){
    int rt=::rename(name.c_str(),newName.c_str());
    return rt == 0;
}
StringVector FileHelper::listDir(const String &name, const String &glob,bool fullNames){
    DIR *dp;
    struct dirent *e;
    StringVector rt;
    if (!(dp=opendir(name.c_str()))){
        return rt;
    }
    bool hasGlob=! glob.empty();
    while ((e = readdir (dp)) != NULL){
        if (strcmp(e->d_name,".") == 0) continue;
        if (strcmp(e->d_name,"..") == 0) continue;
        if (! hasGlob || fnmatch(glob.c_str(),e->d_name,0) == 0){
            rt.push_back(fullNames?concatPath(name,String(e->d_name)):String(e->d_name));
        }
    }
    closedir(dp);
    return rt;
}
bool FileHelper::unlink(const String &name){
    return ::unlink(name.c_str()) == 0;
}

bool FileHelper::makeDirs(const String &dir,bool modifyUmask, int mode){
    if (dir.empty()) return false;
    FSNS::path p(dir);
    FSNS::path current;
    std::error_code ec;
    mode_t origUmask;
    if (modifyUmask){
        origUmask=umask(0);
    }
    for (auto it=p.begin();it != p.end();it++){
        ec.clear();
        if (current.empty()) current=*it;
        else current.append(*it);
        auto stat=FSNS::status(current,ec);
        if (ec){
            //assume not existing
            //use mkdir directly as we want to set the mode
            int res=mkdir(current.c_str(),mode);
            if (res != 0) return false;
            if (! exists(current.string(),true)){
                if (modifyUmask) umask(origUmask);
                return false;
            }
        }
        else{
            if (!FSNS::is_directory(stat)){
                if (modifyUmask) umask(origUmask);
                return false;
            }
        }
    }
    if (modifyUmask) umask(origUmask);
    return true;
}

FileHelper::FileInfo FileHelper::getFileInfo(const String &path){
    FileInfo rt;
    struct stat64 data;
    int res=stat64(path.c_str(), &data);
    if (res < 0) return rt;
    rt.time=data.st_mtim.tv_sec*1000 + data.st_mtim.tv_nsec / 1000000;
    rt.existing=true;
    rt.size=data.st_size;
    rt.isDir=S_ISDIR(data.st_mode) != 0;
    return rt;
}
static bool canAccess(const String &name, int mode){
    return ::access(name.c_str(),mode) == 0;
}
bool FileHelper::canWrite(const String &name){
    return canAccess(name,W_OK);
}
bool FileHelper::canRead(const String &name){
    return canAccess(name,R_OK);
}
bool FileHelper::emptyDirectory(const String &name, bool removeSelf){
    if (! canWrite(name)) return false;
    if (removeSelf){
        FSNS::remove_all(name);
        return true;
    }
    StringVector entries=listDir(name);
    for (auto it=entries.begin();it != entries.end();it++){
        FSNS::remove_all(*it);
    }
    return true;
}
InputStream::Ptr FileHelper::openFileStream(const String &fileName, int bufferSize){
    if (fileName.empty()) throw AvException("filename is empty for open");
    int fd=::open(fileName.c_str(),O_RDONLY);
    if (fd < 0){
        throw AvException(FMT("unable to open file %s:%s",fileName.c_str(),SystemHelper::sysError().c_str()));
    }
    return std::make_shared<InputStream>(fd,bufferSize);
}

InputStream::~InputStream(){
    close();
}
InputStream::InputStream(int fd, size_t bufferSize){
    this->fd=fd;
    this->bufferSize=bufferSize;
    int res=fcntl(fd,F_GETFL);
    if (res & O_NONBLOCK) isNonBlocking=true;
    if (bufferSize > 0) buffer= std::make_unique<char[]>(bufferSize);
}
ssize_t InputStream::readInternal(char *buffer,size_t maxSize){
    if (isClosed) return 0;
    ssize_t bRead = _read(fd, buffer, maxSize);
    if (bRead < 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            return 0;
        }
        LOG_ERROR("read error on fd %d: %s",fd,SystemHelper::sysError().c_str());
        hasEof=true;
        close();
        return -1;
    }
    if (bRead == 0){
        hasEof=true;
        close();
    }    
    return bRead;
}    
ssize_t InputStream::read(char *buffer,size_t maxSize, long waitMillis){
    size_t bRet=0;
    if (isClosed){
        return 0;
    }
    if (bytesInBuffer > 0){
        size_t toCopy=(maxSize <= bytesInBuffer)?maxSize:bytesInBuffer;
        memcpy(buffer,this->buffer.get(),toCopy);
        bytesInBuffer-=toCopy;
        if (bytesInBuffer >= 0){
            memmove(this->buffer.get(),this->buffer.get()+toCopy,bytesInBuffer);
        }
        bRet+=toCopy;
    }
    if (bRet >= maxSize || hasEof){
        bytesRead+=bRet;
        return bRet;
    }
    //bytesInBuffer should be 0 now in any case...
    Timer::SteadyTimePoint start = Timer::steadyNow();
    bool firstLoop=true; //try only once if waitTime == 0
    while ((bRet < maxSize) && (waitMillis < 0 || (waitMillis > 0 && !Timer::steadyPassedMillis(start,waitMillis)) || firstLoop) && ! hasEof)
    {
        firstLoop=false;
        size_t toRead=maxSize-bRet;
        size_t toReadBuffer=toRead;
        bool toBuffer = false;
        if (bufferSize > 0 && ( isNonBlocking || waitMillis < 0) && toRead < bufferSize)
        {
            // we read as much as we can get up to buffer
            toReadBuffer = bufferSize;
            toBuffer = true;
        }
        if (isNonBlocking)
        {
            struct pollfd fds[1];
            fds[0].fd = fd;
            fds[0].events = POLLIN;
            int pollWait=0;
            if (waitMillis > 0){
                pollWait=Timer::remainMillis(start,waitMillis);
            }
            if (waitMillis < 0){
                pollWait=1000;
            }
            if (pollWait < 0) pollWait=0;
            int res = _poll(fds, 1, pollWait);
            if (res < 0)
            {
                if (errno == EAGAIN)
                {
                    continue;                        
                }
                LOG_ERROR("error reading %d: %s", fd, SystemHelper::sysError().c_str());
                close();
                return -1;
            }
            if (!(fds[0].revents))
            {
                continue;
            }
        }
        ssize_t bRead = readInternal(toBuffer?this->buffer.get():buffer+bRet,toBuffer?toReadBuffer:toRead);
        if (bRead < 0)
        {
            return bRead;
        }
        if (bRead > 0)
        {
            if (toBuffer)
            {
                bytesInBuffer += bRead;
                if (bytesInBuffer >= toRead)
                {
                    memcpy(buffer + bRet, this->buffer.get(), toRead);
                    memmove(this->buffer.get(), this->buffer.get() + toRead, bytesInBuffer - toRead);
                    bytesInBuffer -= toRead;
                    bRet += toRead;
                }
                else
                {
                    memcpy(buffer + bRet, this->buffer.get(), bytesInBuffer);
                    bRet += bytesInBuffer;
                    bytesInBuffer = 0;
                }
            }
            else
            {
                bRet += bRead;
            }
        }
    }
    bytesRead += bRet;
    return bRet;
}
int InputStream::read(){
    if (isClosed) return -1;
    char ret[1];
    ssize_t rd=read(ret,1);
    if (rd < 1) return -1;
    return ret[0];
}
void InputStream::close(){
    if (isClosed) return;
    ::close(fd);
    isClosed=true;
}
size_t InputStream::BytesRead(){
    return bytesRead;
}
bool InputStream::IsOpen(){
    return ! isClosed;
}
bool InputStream::HasEof(){
    return hasEof && (bytesInBuffer == 0);
}
bool InputStream::IsNonBlocking(){
    return isNonBlocking;
}
bool InputStream::CheckRead(long waitMillis)
{
    if (bytesRead > 0 || bytesInBuffer > 0)
        return true;
    if (bufferSize < 1)
        throw AvException("can only check read if a buffer is defined");
    char tmp[1];
    Timer::SteadyTimePoint start = Timer::steadyNow();
    while (isNonBlocking)
    {
        struct pollfd fds[1];
        fds[0].fd = fd;
        fds[0].events = POLLIN;
        int pollWait = 0;
        if (waitMillis > 0)
        {
            pollWait = Timer::remainMillis(start, waitMillis);
        }
        if (waitMillis < 0)
        {
            pollWait = 1000;
        }
        if (pollWait < 0)
            pollWait = 0;
        int res = _poll(fds, 1, pollWait);
        if (res < 0)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            LOG_ERROR("error reading %d: %s", fd, SystemHelper::sysError());
            close();
            return false;
        }
        if (!(fds[0].revents))
        {
            continue;
        }
        break;
    }
    ssize_t rt = readInternal(tmp, 1);
    if (rt <= 0)
    {
        return false;
    }
    buffer[0] = tmp[0];
    bytesInBuffer++;
    return true;
}
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Info
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
#include "Logger.h"
#include "FileHelper.h"
#include "Timer.h"
#include "SystemHelper.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <algorithm>
#include <fcntl.h>
#include <thread>
#include <string>
#include <iostream>
#include <functional>
#include <stdio.h>

static const String FILENAME="provider.log";

class MStreamBuf : public std::streambuf
{
    typedef std::ptrdiff_t streamsize;

public:
    MStreamBuf(FILE *fp, bool lineFlush=true) : std::streambuf()
    {
        this->fp = fp;
        this->lineFlush=lineFlush;
    }
    virtual streamsize xsputn(const char *s, streamsize n)
    {
        if (! fp) return 0;
        return fwrite(s, 1,n, fp);
    }
    bool is_open()
    {
        return fp != NULL;
    }
    void close()
    {
        fclose(fp);
        fp = NULL;
    }
    virtual int_type
      overflow(int_type c  = traits_type::eof())
      { 
          if (! fp) return traits_type::eof();
          fputc(c,fp);
          if (lineFlush && c == '\n') {
              fflush(fp);
          }
          return c;
      }

private:
    FILE *fp;
    bool lineFlush=false;
};

Logger * Logger::_instance = NULL;

Logger::Logger(String logDir){ 
    initialized=false;
    level=LOG_LEVEL_INFO;
    currentLines=0;
    maxLines=10000;
    this->logDir=logDir;
    fileName=FILENAME;
}
bool Logger::Close(){
    Synchronized l(mutex);
    CloseNoLock();
    return true;
}
bool Logger::CloseNoLock()
{
    if (logStream)
    {
        logStream->flush();
        delete logStream;
        logStream = NULL;
    }
    if (logBuffer)
    {
        if (logBuffer->is_open())
        {
            logBuffer->close();
        }
        delete logBuffer;
        logBuffer = NULL;
    }
    return true;
}
bool Logger::OpenLogFile(bool rename){
    CloseNoLock();
    if (rename) RenameFile();
    String finalName=FileHelper::concatPath(logDir,fileName);
    int fd=::open(finalName.c_str(),O_WRONLY|O_CLOEXEC|O_CREAT|O_TRUNC,0664);
    if (fd < 0){
        std::cerr << "unable to open logfile "<< finalName << ": " << SystemHelper::sysError() << std::endl;
        return false; 
    }
    FILE *fp = fdopen(fd,"wb");
    if (! fp){
        std::cerr << "unable to open logfile "<< finalName << std::endl;
        return false;
    }
    logBuffer=new MStreamBuf(fp);
    logStream = new std::ostream(logBuffer);
    return true;
}
bool Logger::isOpen() {
    Synchronized l(mutex);
    return logBuffer  && logBuffer->is_open();
}
void Logger::SetLevel(int l){
    level=l;
}


void Logger::Flush() {
}

void Logger::WriteHeader(const char *cat){
    if (!initialized)
        return;
    if (!logStream)
        return;
    struct timeval tv;
    struct tm tm;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);
    *logStream << StringHelper::format("%04d/%02d/%02d-%02d:%02d:%02d.%03d-",
                                       tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                       tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000L));
    *logStream << std::this_thread::get_id();
    *logStream << "-";
    *logStream << cat;
    *logStream << "-";
}

String Logger::GetBackupName(){
    struct timeval tv;
    struct tm tm;
    gettimeofday(&tv,NULL);
    localtime_r(&tv.tv_sec,&tm);
    //"%Y-%m-%d-%H-%M-%S"
    return StringHelper::format(".%04d-%02d-%02d-%02d-%02d-%02d",
            tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
            tm.tm_hour,tm.tm_min,tm.tm_sec);
}
void Logger::RenameFile(){
    String finalName=FileHelper::concatPath(logDir,fileName);
    if (FileHelper::exists(finalName)){
        String newName=FileHelper::concatPath(logDir,fileName+GetBackupName());
        if (! FileHelper::rename(finalName,newName)){
            std::cerr << "unable to rename logfile from "<<finalName << "to" << newName << std::endl;
        }
    }
    
}
#define MAX_KEEP 20
void Logger::HouseKeeping(bool force) {
    bool hasNewFile=false;
    {
        Synchronized l(mutex);
        if (currentLines > maxLines){
            OpenLogFile(true);
            currentLines=0;
            hasNewFile=true;
        }
    }
    if (hasNewFile || force)
    {
        StringVector logFiles = FileHelper::listDir(logDir, fileName + String(".*"), true);
        if (logFiles.size() <= MAX_KEEP)
            return;
        std::sort(logFiles.begin(), logFiles.end(), std::greater<String>());
        logFiles.erase(logFiles.begin(), logFiles.begin() + MAX_KEEP);
        for (auto it = logFiles.begin(); it != logFiles.end(); it++)
        {
            LOG_DEBUG("removing log file %s", it->c_str());
            FileHelper::unlink(*it);
        }
    }
}


Logger *Logger::instance(){
    return _instance!=NULL?_instance:new Logger("");
}
void Logger::CreateInstance(String logDir,long maxLines){
    if (_instance != NULL) delete _instance;
    _instance=new Logger(logDir);
    _instance->HouseKeeping(true);
    _instance->OpenLogFile();
    _instance->initialized=true;
    _instance->maxLines=maxLines;
}
void Logger::SetMaxLines(long maxLines){
    this->maxLines=maxLines;
}
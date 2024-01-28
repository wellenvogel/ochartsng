/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Info
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
#ifndef _LOGGER_H
#define _LOGGER_H

#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <utility>
#include "SimpleThread.h"
#include "StringHelper.h"

#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_DEBUG 2

#define LOG_INFO(...) if(Logger::instance()->HasLevel(LOG_LEVEL_INFO)) Logger::instance()->Log(LOG_LEVEL_INFO,"INFO",__VA_ARGS__);
#define LOG_INFOC(...) {StringHelper::StreamFormat(std::cout,__VA_ARGS__);std::cout << std::endl;if(Logger::instance()->HasLevel(LOG_LEVEL_INFO)) Logger::instance()->Log(LOG_LEVEL_INFO,"INFO",__VA_ARGS__);}
#define LOG_DEBUG(...) if(Logger::instance()->HasLevel(LOG_LEVEL_DEBUG)) Logger::instance()->Log(LOG_LEVEL_DEBUG,"DEBUG",__VA_ARGS__)
#define LOG_ERROR(...) if(Logger::instance()->HasLevel(LOG_LEVEL_ERROR)) Logger::instance()->Log(LOG_LEVEL_ERROR,"ERROR",__VA_ARGS__)
#define LOG_ERRORC(...) {StringHelper::StreamFormat(std::cout,__VA_ARGS__);std::cout << std::endl;if(Logger::instance()->HasLevel(LOG_LEVEL_ERROR)) Logger::instance()->Log(LOG_LEVEL_ERROR,"ERROR", __VA_ARGS__);}
class MStreamBuf;
class Logger {
private:
    static Logger *_instance;
    bool initialized;
    long maxLines;
    long currentLines;
    int level;
    String fileName;
    MStreamBuf *logBuffer=NULL;
    std::ostream *logStream=NULL;
    std::mutex mutex;
    String logDir;
    String GetBackupName();
    Logger(String logDir);
    void WriteHeader(const char *cat);
    void HouseKeeping(bool force=false);
    void RenameFile();
    bool OpenLogFile(bool rename=true);
    bool CloseNoLock();
public:
    bool Close();
    bool isOpen();
    template<typename ...Args>
    void Log(int level, const char *cat,const char * fmt, Args&& ...args){
        if (! initialized) return;
        {
        Synchronized l(mutex);
        if (! logStream) return;
        if (!this->HasLevel(level)) return;
        WriteHeader(cat);
        StringHelper::StreamFormat(*logStream,fmt,std::forward<Args>(args)...);
        *logStream << std::endl;
        currentLines++;
        }
        HouseKeeping();
    }
    void SetLevel(int level);
    inline bool HasLevel(int level){
        return this->level >= level;
    }
    void Flush();
    void SetMaxLines(long maxLines);
    //timestmap in 0.1ms
    static inline long MicroSeconds100()
    {
        timeval tv;
        gettimeofday(&tv,0);
        return ((long)tv.tv_sec) * 10000L +
            ((long)tv.tv_usec) / 100L;
    }
    static Logger *instance();
    static void CreateInstance(String logDir,long maxLines=10000);

    
};


#endif 

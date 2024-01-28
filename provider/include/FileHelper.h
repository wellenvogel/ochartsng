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

#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <vector>
#include <memory>
#include <stdarg.h>
#include <poll.h>
#include <unistd.h>
#include "StringHelper.h"
#include "Testing.h"
#include "Exception.h"

class InputStream{
    public:
    typedef std::shared_ptr<InputStream> Ptr;
        InputStream(int fd, size_t bufferSize=512);
        ~InputStream();
        /**
         * read data
         * return 0 either on eof or when in nonblocking mode and
         * no data has been received
         * use hasEof to check for real eof
         * waitMillis: 0 - no wait, -1 wait forever - only for non blocking
         *             >=0 only read at most the requested amount, -1 - always fill buffer 
         */
        ssize_t read(char *buffer,size_t maxSize, long waitMillis=-1);
        int read();
        void close();
        size_t BytesRead();
        bool IsOpen();
        bool HasEof();
        bool IsNonBlocking();
        /**
         * check if we can read at least one byte from the input
         * will only work if a buffer is set, otherwise an exception is thrown
         * if data already has been read it will return true immediately
         */
        bool CheckRead(long waitMillis=-1);
    private:
        /**
         * internal read method
         * it will fill the provided buffer with data read from the stream
         * additionally it will handle EOF and blocking/non-blocking
         * The internal buffer is not considered any more
         * For non blocking mode it expects the fd to be readable already
         **/
        ssize_t readInternal(char *buffer, size_t maxSize);
        int fd=-1;
        typedef std::unique_ptr<char[]> BufferType;
        BufferType buffer;
        size_t bufferSize=0;
        size_t bytesInBuffer=0;
        size_t bytesRead=0;
        bool isNonBlocking=false;
        bool hasEof=false;
        bool isClosed=false;
    protected:
        TESTVIRT int _poll(struct pollfd *fds, nfds_t nfds, int timeout){
            return ::poll(fds,nfds,timeout);
        }
        TESTVIRT ssize_t _read(int fd, void *buf, size_t count){
            return ::read(fd,buf,count);
        }
   
};


class FileHelper{
public:
    static String concatPath(const String &p1, const String &p2);
    static String dirname(const String &path);
    static String fileName(const String &path, bool stripExtension=false);
    static String ext(const String &path);
    static String realpath(const String &path);
    static bool exists(const String &filename,bool directory=false);
    static int64_t fileSize(const String &name);
    static int64_t fileTime(const String &name); //milliseconds
    static bool rename(const String &name, const String &newName);
    /**
     * empty glob - match all
     */
    static StringVector listDir(const String &name, const String &glob="",bool fullNames=true);
    static bool unlink(const String &name);
    static bool makeDirs(const String &dir, bool modifyUmask=false,int mode=0755);
    static bool canWrite(const String &name);
    static bool canRead(const String &name);
    static bool emptyDirectory(const String &name, bool removeSelf=false);
    class FileInfo{
        public:
        int64_t size=-1;
        int64_t time=-1;
        bool isDir=false;
        bool existing=false;
    };
    static FileInfo getFileInfo(const String &path);
    static InputStream::Ptr openFileStream(const String &fileName, int bufferSize=512);
};




#endif /* FILEHELPER_H */


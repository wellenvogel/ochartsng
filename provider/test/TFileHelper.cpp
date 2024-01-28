/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test FileHelper
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

#include <gtest/gtest.h>
#include <stdio.h>
#include "FileHelper.h"
#include "SystemHelper.h"
#include "Timer.h"
#include "SimpleThread.h"
#include "TestHelper.h"
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <filesystem.hpp>
#include <fstream>
#include <algorithm>

#define FSNS ghc::filesystem



class StreamTester{
    public:
    int fd;
    int writeFd;
    bool closed=false;
    StreamTester(){
        int fds[2];
        pipe(fds);
        fd=fds[0];
        writeFd=fds[1];
    }
    bool writeBuffer(char *buffer, size_t bufferSize){
        if (closed) return false;
        size_t written=0;
        while (written < bufferSize){
            ssize_t res=write(writeFd,buffer,bufferSize);
            if (res < 0){
                fprintf(stderr,"write error %d\n",errno);
                return res;
            }
            written+=res;
            if (written < bufferSize) usleep(100000);
        }
        return written == bufferSize;
    }
    void close(){
        if (closed) return;
        closed=true;
        ::close(writeFd);
    }
    ~StreamTester(){
        ::close(fd);
        ::close(writeFd);
    }
    bool setNonBlock(){
        int retval = fcntl( fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
        return retval == 0;
    }

};


static TestCounter *counter=TestCounter::instance();

class TInputStream:public InputStream{
    public:
        TInputStream(int fd, size_t bufferSize):InputStream(fd,bufferSize){}
    protected:
        TESTVIRT int _poll(struct pollfd *fds, nfds_t nfds, int timeout){
            int rt=::poll(fds,nfds,timeout);
            std::cerr << "poll fds=" << fds << ", nfds=" << nfds << ", timeout=" << timeout << ", rt=" << rt << std::endl;
            TestCounter::instance()->increment("poll");
            return rt;
        }
        TESTVIRT ssize_t _read(int fd, void *buf, size_t count){
            ssize_t rt=::read(fd,buf,count);
            std::cerr << "read fd=" << fd << ", count=" << count << ", rt=" << rt << std::endl;
            TestCounter::instance()->increment("read");
            return rt;
        }  
};


class Buffer{
    public:
        char *buffer=NULL;
        char *testBuffer=NULL;
        int size=0;
        Buffer(int size, const char *pattern){
            this->size=size;
            int plen=strlen(pattern);
            buffer=new char[size+1];
            testBuffer=new char[size+1];
            for(int wr=0;wr<size;){
                int remain=size-wr;
                if (remain < plen){
                    memcpy(buffer+wr,pattern,remain);
                    wr+=remain;
                }
                else{
                    memcpy(buffer+wr,pattern,plen);
                    wr+=plen;
                }
            }
            buffer[size]=0;
            testBuffer[size]=0;
        }
        ~Buffer(){
            delete buffer;
            delete testBuffer;
        }
        bool checkEqual(int len=-1){
            if (len < 0) len=size;
            return memcmp(buffer,testBuffer,len) == 0;
        }
};

#define PV1(sz,type) StreamTester tester; \
    ASSERT_NE(tester.fd,-1); \
    type tstream(tester.fd,sz); 
#define P1(sz) PV1(sz,InputStream)    
#define PVN(sz,type) StreamTester tester; \
    ASSERT_NE(tester.fd,-1); \
    ASSERT_TRUE(tester.setNonBlock()) << SystemHelper::sysError(); \
    type tstream(tester.fd,sz);     
#define PN(sz) PVN(sz,InputStream)    

// Demonstrate some basic assertions.
TEST(InputStream, BlockingFullReadNoBuffer) {
    P1(0);
    EXPECT_EQ(tstream.IsOpen(),true);
    EXPECT_EQ(tstream.BytesRead(),0);
    const int bs=200;
    Buffer cb(200,"test1");
    ASSERT_TRUE(tester.writeBuffer(cb.buffer,cb.size));
    ssize_t rd=tstream.read(cb.testBuffer,cb.size);
    EXPECT_EQ(rd,bs) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
    //expect read 0 when closed
    tester.close();
    EXPECT_FALSE(tstream.HasEof());
    rd=tstream.read(cb.testBuffer,cb.size);
    EXPECT_EQ(rd,0) << "rd=" << rd;
    EXPECT_TRUE(tstream.HasEof());
    EXPECT_FALSE(tstream.IsOpen());
}
TEST(InputStream,Blocking2Reads){
    P1(0);
    Buffer cb(200,"test2reads");
    ASSERT_TRUE(tester.writeBuffer(cb.buffer,cb.size));
    ssize_t rd=tstream.read(cb.testBuffer,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    rd=tstream.read(cb.testBuffer+100,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
}

TEST(InputStream,NonBlockingNoBufferNoWait){
    PN(0);
    EXPECT_TRUE(tstream.IsNonBlocking());
    Buffer cb(200,"testNB1");
    ASSERT_TRUE(tester.writeBuffer(cb.buffer,cb.size));
    ssize_t rd=tstream.read(cb.testBuffer,cb.size,0);
    EXPECT_EQ(rd,cb.size) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
}
TEST(InputStream,NonBlockingNoBufferNoWaitNoData){
    PN(0);
    EXPECT_TRUE(tstream.IsNonBlocking());
    Buffer cb(200,"testNB1.1");
    ssize_t rd=tstream.read(cb.testBuffer,cb.size,0);
    EXPECT_EQ(rd,0) << "not all/too many bytes read " << rd;
    EXPECT_FALSE(tstream.HasEof());
    EXPECT_TRUE(tstream.IsOpen());
    ASSERT_TRUE(tester.writeBuffer(cb.buffer,cb.size));
    rd=tstream.read(cb.testBuffer,cb.size,0);
    EXPECT_EQ(rd,cb.size) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
}

//a file that we wait for time changes
//use "touch xxxx to trigger continuation"
const char * DebugFileEnv="AVTEST_TRIGGER_FILE";

static String getDebugFileName(){
    const char * n=getenv(DebugFileEnv);
    if (!n) return String();
    return String(n);
}
class DelayedWriter{
    public:
    int size=0;
    char *buffer=NULL;
    StreamTester *tester=NULL;
    bool written=false;
    DelayedWriter(StreamTester *tester,char *buffer,int size){
        this->size=size;
        this->buffer=buffer;
        this->tester=tester;
    }
    void run(int sleepTime,bool waitDebug=false){
        String DebugFile=getDebugFileName();
        uint64_t start=FileHelper::fileTime(DebugFile);
        Thread writer([this,sleepTime,waitDebug,start,DebugFile]{
            if (waitDebug){
                uint64_t current=0;
                while ((current=FileHelper::fileTime(DebugFile)) <= start){
                    usleep(100000);
                }
            }
            else{
                usleep(sleepTime*1000);
            }
            ASSERT_TRUE(this->tester->writeBuffer(this->buffer,this->size));
            written=true;
            std::cerr << "buffer written" << std::endl;
        });
        writer.start();
        writer.detach();
    }
    void close(int sleepTime,bool waitDebug=false){
        String DebugFile=getDebugFileName();
        uint64_t start=FileHelper::fileTime(DebugFile);
        Thread writer([this,sleepTime,waitDebug,start,DebugFile]{
            if (waitDebug){
                uint64_t current=0;
                while ((current=FileHelper::fileTime(DebugFile)) <= start){
                    usleep(100000);
                }
            }
            else{
                usleep(sleepTime*1000);
            }
            this->tester->close();
            written=true;
            std::cerr << "closed" << std::endl;
        });
        writer.start();
        writer.detach();
    }
    void waitDone(){
        while (! written){
            usleep(100);
        }
    }
};

TEST(InputStream,NonBlockingNoBufferWaitTime){
    PN(0);
    EXPECT_TRUE(tstream.IsNonBlocking());
    Buffer cb(200,"testB1");
    DelayedWriter writer(&tester,cb.buffer,cb.size);
    writer.run(100);
    ssize_t rd=tstream.read(cb.testBuffer,cb.size,200);
    EXPECT_EQ(rd,cb.size) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
    writer.waitDone();
}
TEST(InputStream,NonBlockingNoBufferWaitTime2Parts){
    PN(0);
    EXPECT_TRUE(tstream.IsNonBlocking());
    Buffer cb(200,"testB2");
    DelayedWriter writer(&tester,cb.buffer,100);
    writer.run(100);
    DelayedWriter writer2(&tester,cb.buffer+100,100);
    writer2.run(200);
    ssize_t rd=tstream.read(cb.testBuffer,cb.size,300);
    EXPECT_EQ(rd,cb.size) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
    writer.waitDone();
    writer2.waitDone();
}
TEST(InputStream,NonBlockingNoBufferWaitForever2Parts){
    PN(0);
    EXPECT_TRUE(tstream.IsNonBlocking());
    Buffer cb(200,"testB3");
    DelayedWriter writer(&tester,cb.buffer,100);
    writer.run(100);
    DelayedWriter writer2(&tester,cb.buffer+100,100);
    writer2.run(200);
    ssize_t rd=tstream.read(cb.testBuffer,cb.size);
    EXPECT_EQ(rd,cb.size) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
    writer.waitDone();
    writer2.waitDone();
}
TEST(InputStream,NonBlockingNoBufferWaitTooShort2Parts){
    PN(0);
    EXPECT_TRUE(tstream.IsNonBlocking());
    Buffer cb(200,"testB3.1");
    DelayedWriter writer(&tester,cb.buffer,100);
    writer.run(100);
    DelayedWriter writer2(&tester,cb.buffer+100,100);
    writer2.run(400);
    ssize_t rd=tstream.read(cb.testBuffer,cb.size,300);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(100)) << "data not equal";
    writer.waitDone();
    writer2.waitDone();
    rd=tstream.read(cb.testBuffer+100,100,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual()) << "data not equal";
}
TEST(InputStream,NonBlockingNoBufferWaitForeverEof){
    PN(0);
    EXPECT_TRUE(tstream.IsNonBlocking());
    Buffer cb(200,"testB4");
    DelayedWriter writer(&tester,cb.buffer,100);
    writer.run(100);
    DelayedWriter writer2(&tester,cb.buffer+100,100);
    writer2.close(200);
    ssize_t rd=tstream.read(cb.testBuffer,cb.size,-1);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(100)) << "data not equal";
    writer.waitDone();
    writer2.waitDone();
}

TEST(InputStream,BlockingBufferSmallerOneRead){
    PV1(200,TInputStream);
    counter->reset();
    Buffer cb(200,"testNB201");
    ASSERT_TRUE(tester.writeBuffer(cb.buffer,cb.size));
    ssize_t rd=tstream.read(cb.testBuffer,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(100)) << "data not equal";
    EXPECT_EQ(counter->getValue("read"),1) << "invalid number of read calls";
    rd=tstream.read(cb.testBuffer+100,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(200)) << "data not equal";
    //we should have been reading from the buffer
    EXPECT_EQ(counter->getValue("read"),1) << "invalid number of read calls";
}
TEST(InputStream,BlockingBufferBiggerTwoReads){
    PV1(100,TInputStream);
    counter->reset();
    Buffer cb(200,"testNB201");
    ASSERT_TRUE(tester.writeBuffer(cb.buffer,cb.size));
    ssize_t rd=tstream.read(cb.testBuffer,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(100)) << "data not equal";
    EXPECT_EQ(counter->getValue("read"),1) << "invalid number of read calls";
    rd=tstream.read(cb.testBuffer+100,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(200)) << "data not equal";
    EXPECT_EQ(counter->getValue("read"),2) << "invalid number of read calls";
}

TEST(InputStream,NonBlockingBufferSmaller){
    PVN(150,TInputStream);
    counter->reset();
    Buffer cb(200,"testNB201");
    ASSERT_TRUE(tester.writeBuffer(cb.buffer,cb.size));
    ssize_t rd=tstream.read(cb.testBuffer,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(100)) << "data not equal";
    EXPECT_EQ(counter->getValue("read"),1) << "invalid number of read calls";
    rd=tstream.read(cb.testBuffer+100,100);
    EXPECT_EQ(rd,100) << "not all/too many bytes read " << rd;
    EXPECT_TRUE(cb.checkEqual(200)) << "data not equal";
    EXPECT_EQ(counter->getValue("read"),2) << "invalid number of read calls";
}


TEST(FileHelper,dirname){
    String f("a/b.c");
    String t=FileHelper::dirname(f);
    EXPECT_EQ(t,String("a"));
}
TEST(FileHelper,basename){
    String f("a/b.c");
    String t=FileHelper::fileName(f);
    EXPECT_EQ(t,String("b.c"));
}
TEST(FileHelper,basenameStrip){
    String f("a/b.c");
    String t=FileHelper::fileName(f,true);
    EXPECT_EQ(t,String("b"));
}
TEST(FileHelper,ext){
    String f("a/b.c");
    String t=FileHelper::ext(f);
    EXPECT_EQ(t,String("c"));
}
class FileSystem : public ::testing::Test {
    protected:
        FSNS::path TESTDIR;
        void SetUp() override{
            const char * testdir=getenv("TESTDIR");
            ASSERT_NE(testdir,nullptr);
            ASSERT_STRNE(testdir,"");
            ASSERT_STRNE(testdir,".");
            ASSERT_STRNE(testdir,"..");
            TESTDIR=FSNS::canonical(testdir);
            std::cerr << "removing and recreating testdir " << TESTDIR << std::endl;
            FSNS::remove_all(TESTDIR);
            FSNS::create_directories(TESTDIR);
            std::cerr << "preparation done" << std::endl;
        }   
};
TEST_F(FileSystem,realpath){
    std::cerr << "realpath start" << std::endl;
    FSNS::create_directories(TESTDIR / "a");
    {
        std::ofstream test((TESTDIR /"a"/"b.c").string());
    }
    String f=StringHelper::format("%s/a/b.c",TESTDIR.c_str());
    String t=FileHelper::realpath(f);
    char *p=realpath(f.c_str(),NULL);
    std::cerr << "realpath:" << t << std::endl;
    EXPECT_STREQ(p,t.c_str());
    free(p);
}
TEST_F(FileSystem,realpathNonExist){
    String f=StringHelper::format("%s/a/b.c",TESTDIR.c_str());
    String t=FileHelper::realpath(f);
    std::cerr << f << ", realpath:" << t << std::endl;
    EXPECT_EQ(f,t);
}
TEST_F(FileSystem,makeDirs){
    String f(StringHelper::format("%s/x/y/z",TESTDIR.c_str()));
    EXPECT_TRUE(FileHelper::makeDirs(f));
    EXPECT_TRUE(FileHelper::exists(f,true));
}
TEST_F(FileSystem,listDir){
    StringVector files={
        "a",
        "b.x",
        "c"
    };
    StringVector pathes;
    for (auto it=files.begin();it != files.end();it++){
        pathes.push_back(FileHelper::concatPath(TESTDIR,*it));
    }
    for (auto it=pathes.begin();it!=pathes.end();it++){
        std::ofstream s(*it);
    }
    StringVector listed=FileHelper::listDir(TESTDIR,"",false);
    EXPECT_EQ(listed.size(),pathes.size());
    for (auto it=listed.begin();it != listed.end();it++){
        EXPECT_NE(std::find(files.begin(),files.end(),*it),files.end());
    }
}
TEST_F(FileSystem,listDirFullPath){
    StringVector files={
        "a",
        "b.x",
        "c"
    };
    StringVector pathes;
    for (auto it=files.begin();it != files.end();it++){
        pathes.push_back(FileHelper::concatPath(TESTDIR,*it));
    }
    for (auto it=pathes.begin();it!=pathes.end();it++){
        std::ofstream s(*it);
    }
    StringVector listed=FileHelper::listDir(TESTDIR,"",true);
    EXPECT_EQ(listed.size(),pathes.size());
    for (auto it=listed.begin();it != listed.end();it++){
        EXPECT_NE(std::find(pathes.begin(),pathes.end(),*it),pathes.end());
    }
}

TEST_F(FileSystem,listDirGlob){
    StringVector files={
        "a",
        "b.x",
        "c"
    };
    StringVector pathes;
    for (auto it=files.begin();it != files.end();it++){
        pathes.push_back(FileHelper::concatPath(TESTDIR,*it));
    }
    for (auto it=pathes.begin();it!=pathes.end();it++){
        std::ofstream s(*it);
    }
    StringVector listed=FileHelper::listDir(TESTDIR,"*.x",false);
    EXPECT_EQ(listed.size(),1);
    EXPECT_EQ(listed[0],files[1]);
}

/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test ChartCache
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
#include <functional>
#include "OcAllocatorImpl.h"

using namespace ocalloc;
class T1Malloc{
    public:
    std::unique_ptr<uint8_t[]> buffer;
    size_t size=0;
    int count=0;
    int cleanup=0;
    T1Malloc(size_t s):size(s+sizeof(Block)){
        buffer=std::make_unique<uint8_t[]>(s);
    }
    Block * markAlloc(size_t s){
        count ++;
        Block *rt=(Block *)(buffer.get());
        rt->capacity=size;
        return rt;
    }
    void cleanUp(){
        cleanup++;
    }

    Block * getBlock(){
        Block *rt=(Block *)(buffer.get());
        rt->capacity=size;
        return rt;
    }
};

class T2Malloc{
    public:
    using BT=uint8_t[];
    using BPTR=std::unique_ptr<BT>;
    std::vector<BPTR> buffer;
    size_t size=0;
    int count=0;
    int cleanup=0;
    T2Malloc(size_t s):size(s+sizeof(Block)){
    }
    Block * markAlloc(size_t s){
        count ++;
        auto bp=std::make_unique<BT>(size);
        Block *rt=(Block *)(bp.get());
        buffer.push_back(std::move(bp));
        rt->capacity=size;
        return rt;
    }
    void cleanUp(){
        cleanup++;
    }

    Block * getBlock(size_t idx){
        if (idx >= buffer.size()) return nullptr;
        Block *rt=(Block *)(buffer[idx].get());
        return rt;
    }
};

TEST(FixedSizeAlloc,initialAlloc){
    const size_t size=16;
    std::unique_ptr<T1Malloc> m=std::make_unique<T1Malloc>(size*4);
    FixedSizeAlloc<T1Malloc> alloc(size);
    EXPECT_EQ(m->count,0);
    EXPECT_EQ(alloc.getSize(),size);
    EXPECT_EQ(alloc.getAllocations(),0);
    void *data=alloc.allocate(m.get());
    EXPECT_NE(data,nullptr);
    EXPECT_EQ(m->count,1);
    EXPECT_EQ(alloc.getAllocations(),1);
    const char *pattern="abcdef";
    strcpy((char *)data,pattern);
    //pattern should be now in the buffer of malloc after the header
    Block *b=m->getBlock();
    EXPECT_EQ((void *)(b->getUser()),data);
    EXPECT_STREQ((char *)(b->getUser()),pattern);
};

TEST(FixedSizeAlloc,multipleAlloc){
    const size_t size=16;
    std::unique_ptr<T1Malloc> m=std::make_unique<T1Malloc>(size*4);
    FixedSizeAlloc<T1Malloc> alloc(size);
    void *data1=alloc.allocate(m.get());
    void *data2=alloc.allocate(m.get());
    void *data3=alloc.allocate(m.get());
    void *data4=alloc.allocate(m.get());
    EXPECT_EQ(m->count,1);
    Block *b=m->getBlock();
    EXPECT_EQ((void*)(b->getUser()),data1);
    EXPECT_EQ((void*)(b->getUser()+size),data2);
    EXPECT_EQ((void*)(b->getUser()+ 2 * size),data3);
    EXPECT_EQ((void*)(b->getUser()+ 3 * size),data4);
}

TEST(FixedSizeAlloc,allocAndFree){
    const size_t size=16;
    std::unique_ptr<T1Malloc> m=std::make_unique<T1Malloc>(size*4);
    FixedSizeAlloc<T1Malloc> alloc(size);
    void *data1=alloc.allocate(m.get());
    void *data2=alloc.allocate(m.get());
    alloc.free(data1);
    alloc.free(data2);
    void *data3=alloc.allocate(m.get());
    void *data4=alloc.allocate(m.get());
    void *data5=alloc.allocate(m.get());
    void *data6=alloc.allocate(m.get());
    EXPECT_EQ(m->count,1);
    Block *b=m->getBlock();
    EXPECT_EQ((void*)(b->getUser()),data1);
    EXPECT_EQ((void*)(b->getUser()+size),data2);
    EXPECT_EQ((void*)(b->getUser()),data4);
    EXPECT_EQ((void*)(b->getUser()+size),data3);
    EXPECT_EQ((void*)(b->getUser()+ 2 * size),data5);
    EXPECT_EQ((void*)(b->getUser()+ 3 * size),data6);
}

TEST(FixedSizeAlloc,multipleBuffer){
    const size_t size=16;
    const size_t bufferFactor=4;
    std::unique_ptr<T2Malloc> m=std::make_unique<T2Malloc>(size*bufferFactor);
    FixedSizeAlloc<T2Malloc> alloc(size);
    void *data1=alloc.allocate(m.get());
    EXPECT_EQ(m->count,1);
    EXPECT_NE(data1,nullptr);
    Block *b=m->getBlock(0);
    EXPECT_EQ((void *)(b->getUser()),data1);
    for (int i=1;i< bufferFactor;i++){
        void *data=alloc.allocate(m.get());
        EXPECT_EQ(m->count,1);
        EXPECT_NE(data,nullptr);
    }
    void *data2=alloc.allocate(m.get());
    EXPECT_EQ(m->count,2);
    EXPECT_NE(data2,nullptr);
    Block *b2=m->getBlock(1);
    EXPECT_EQ((void *)(b2->getUser()),data2);
}

TEST(FixedSizeAlloc,spareBuffer){
    const size_t size=16;
    const size_t bufferFactor=4;
    std::unique_ptr<T2Malloc> m=std::make_unique<T2Malloc>(size*bufferFactor+size-3);
    FixedSizeAlloc<T2Malloc> alloc(size);
    for (int i=0;i< bufferFactor;i++){
        void *data=alloc.allocate(m.get());
        EXPECT_EQ(m->count,1);
        EXPECT_NE(data,nullptr);
    }
    Block *b=m->getBlock(0);
    void *data2=alloc.allocate(m.get());
    EXPECT_EQ(m->count,2);
    EXPECT_NE(data2,nullptr);
    Block *b2=m->getBlock(1);
    EXPECT_EQ((void *)(b2->getUser()),data2);
    EXPECT_NE(b,b2);
}

class T1Map{
    public:
    using PTR=std::unique_ptr<Block>;
    const size_t HDR=sizeof(Block);
    std::vector<PTR> blocks;
    size_t mapped=0;
    size_t unmapped=0;
    size_t failed_unmapped=0;
    size_t minSize=0;
    void reset(){
        blocks.clear();
        mapped=0;
        unmapped=0;
        failed_unmapped=0;
    }
    Block *map(size_t size){
        mapped++;
        size+=HDR;
        if (minSize > 0){
            if (size < minSize) size=minSize;
        }
        uint8_t* data=new uint8_t[size];
        Block *rt=(Block *)data;
        rt->capacity=size;
        blocks.push_back(std::unique_ptr<Block>(rt));
        return rt;
    }
    void unmap(Block *b){
        auto it=blocks.begin();
        for (;it!=blocks.end();it++){
            if (it->get() == b) break;
        }
        if (it == blocks.end()){
            std::cerr << "invalid unmap" << b << std::endl;
            failed_unmapped++;
            return;
        }
        unmapped++;
        blocks.erase(it);
    }
};
TEST(MarkedAlloc,initial){
    const size_t size=16;
    MarkedAlloc<T1Map> marker;
    Block *b1=marker.markAlloc(size);
    EXPECT_NE(b1,nullptr);
    EXPECT_EQ(b1->capacity,size+sizeof(Block));
    EXPECT_EQ(marker.mapped,1);
    EXPECT_EQ(marker.blocks[0].get(),b1);
    Block *b2=marker.markAlloc(size);
    EXPECT_NE(b2,nullptr);
    EXPECT_EQ(b2->capacity,size+sizeof(Block));
    EXPECT_EQ(marker.mapped,2);
    EXPECT_EQ(marker.blocks.size(),2);
    EXPECT_EQ(b1->lastBlock,0);
    EXPECT_EQ((Block*)(b2->lastBlock),b1);
}

TEST(MarkedAlloc,cleanup){
    const size_t size=16;
    MarkedAlloc<T1Map> marker;
    Block *b1=marker.markAlloc(size);
    EXPECT_NE(b1,nullptr);
    Block *b2=marker.markAlloc(size);   
    EXPECT_NE(b2,nullptr);
    EXPECT_EQ(marker.mapped,2);
    marker.cleanUp();
    EXPECT_EQ(marker.unmapped,2);
    EXPECT_EQ(marker.blocks.size(),0);
}

TEST(MMap,initial){
    const size_t size=24;
    const size_t expected=ALLOC_PAGES*sysconf(_SC_PAGE_SIZE);
    MMap mmap;
    Block *b=mmap.map(size);
    EXPECT_NE(b,nullptr);
    EXPECT_GE(b->getCapacity(),size);
    EXPECT_EQ(b->capacity,expected);
    EXPECT_EQ(mmap.getSize(),expected);
    size_t cap=b->getCapacity();
    for (int i=0;i< cap/2;i++){
        *(b->getUser()+i)=1;
    }
    Block *b2=mmap.map(size);
    EXPECT_EQ(mmap.getSize(),2*expected);
    mmap.unmap(b);
    mmap.unmap(b2);
    EXPECT_EQ(mmap.getSize(),0);
}
TEST(MMap,large){
    const size_t expected=ALLOC_PAGES*sysconf(_SC_PAGE_SIZE);
    MMap mmap;
    const size_t large=expected+1;
    Block *b=mmap.map(large);
    EXPECT_NE(b,nullptr);
    EXPECT_GE(b->getCapacity(),large);
    mmap.unmap(b);
}

TEST(MMap,large2){
    const size_t expected=ALLOC_PAGES*sysconf(_SC_PAGE_SIZE);
    MMap mmap;
    const size_t large=3*expected+256;
    Block *b=mmap.map(large);
    EXPECT_NE(b,nullptr);
    EXPECT_GE(b->getCapacity(),large);
    mmap.unmap(b);
}

TEST(SeparatingAlloc,initialSmall){
    using TCLASS=SeparatingAlloc<T1Map,MarkedAlloc>;
    auto mapper=std::make_shared<TCLASS::MARKED_ALLOC>();
    mapper->minSize=ALLOC_PAGES*sysconf(_SC_PAGE_SIZE);
    {
        TCLASS alloc("test1",mapper);
        void *p1=alloc.allocate(1,1);
        EXPECT_NE(p1,nullptr);
        EXPECT_EQ(mapper->blocks.size(),1);
        EXPECT_EQ(mapper->mapped,1);
    }
    EXPECT_EQ(mapper->blocks.size(),0);
    EXPECT_EQ(mapper->unmapped,1);
}
TEST(SeparatingAlloc,Large){
    using TCLASS=SeparatingAlloc<T1Map,MarkedAlloc>;
    auto mapper=std::make_shared<TCLASS::MARKED_ALLOC>();
    mapper->minSize=ALLOC_PAGES*sysconf(_SC_PAGE_SIZE);
    {
        TCLASS alloc("test1",mapper);
        size_t testSize=alloc.maxSmall()+1;
        void *p1=alloc.allocate(testSize,16);
        EXPECT_NE(p1,nullptr);
        EXPECT_EQ(mapper->blocks.size(),1);
        EXPECT_EQ(mapper->mapped,1);
    }
    //we leaked the block here!
    EXPECT_EQ(mapper->blocks.size(),1);
    EXPECT_EQ(mapper->unmapped,0);
    mapper->reset();
    {
        TCLASS alloc("test1",mapper);
        size_t testSize=alloc.maxSmall()+1;
        void *p1=alloc.allocate(testSize,16);
        EXPECT_NE(p1,nullptr);
        EXPECT_EQ(mapper->blocks.size(),1);
        EXPECT_EQ(mapper->mapped,1);
        alloc.deallocate(p1,testSize);
        EXPECT_EQ(mapper->blocks.size(),0);
        EXPECT_EQ(mapper->unmapped,1);
    }
}
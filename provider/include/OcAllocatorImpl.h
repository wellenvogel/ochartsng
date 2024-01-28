/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  own allocator
 * Author:   Andreas Vogel
 *
 * inspired by https://github.com/emeryberger/Heap-Layers.git
 * and by https://github.com/foonathan/memory
 ***************************************************************************
 *   Copyright (C) 2023 by Andreas Vogel   *
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
#ifndef _OCALLOCATORIMPL_H
#define _OCALLOCATORIMPL_H
#include "OcAllocator.h"
#include <cstddef>
#include <sys/mman.h>
#define MMAP_DEBUG 1
namespace ocalloc{
    struct Block{
        uint64_t lastBlock=0;
        uint64_t capacity=0;
        Block * getPrev() const{
            return (Block*)lastBlock;
        }
        static Block * fromUser(void *p){
            Block *rt=(Block *)p;
            return rt-1;
        }
        uint8_t * getUser() const{
            uint8_t *start=(uint8_t *)this;
            return start+sizeof(Block);
        }
        size_t getCapacity() const{
            return capacity - sizeof(Block);
        }
        void setPrev(Block *p){
            lastBlock=(uint64_t)p;
        }
    };

    static_assert((sizeof(Block) == alignof(std::max_align_t)) ||
        (sizeof(Block) == (2*alignof(std::max_align_t))),"invalid alignment for Block");

    static constexpr const size_t ALLOC_PAGES=6;
    class MMap{
        size_t pageSize=0;
        size_t minSize=0;
        size_t mapped=0;
        public:
            MMap(){
                pageSize=sysconf(_SC_PAGE_SIZE);
                minSize=ALLOC_PAGES*pageSize;
            }
            Block *map(size_t size);
            void unmap(Block *p);
            size_t getSize(){ return mapped;}
    };

    template<class Alloc>
    class MarkedAlloc : public Alloc{
        Block *last=nullptr;
        public:
        using Alloc::Alloc;
        Block * markAlloc(size_t size){
            Block *rt=Alloc::map(size);
            if (rt){
                rt->setPrev(last);
                last=rt;
            }
            return rt;
        }
        void cleanUp(){
            Block *current=last;
            while (current != nullptr){
                Block *next=current->getPrev();
                Alloc::unmap(current);
                current=next;
            }
        }

    };

    struct FreeEntry{
        FreeEntry *next=nullptr;
    };
    template<class MAlloc>
    class FixedSizeAlloc {
        FreeEntry *freeList=nullptr;
        Block *pool=nullptr;
        size_t poolUsed=0;
        size_t size=0;
        size_t allocations=0;
        size_t frees=0;
        size_t blocks=0;
        public:
        FixedSizeAlloc(size_t s):size(s){
            POOL_ASSERT(size >= sizeof(FreeEntry));
        }
        void *allocate(MAlloc *allocator){
            allocations++;
            if (freeList != nullptr){
                FreeEntry *e=freeList;
                freeList=e->next;
                return e;
            }
            if (pool != nullptr){
                if ((poolUsed+size) <= pool->getCapacity()){
                    uint8_t *rt=pool->getUser()+poolUsed;
                    poolUsed+=size;
                    return rt;
                }
            }
            blocks++;
            poolUsed=size;
            pool=allocator->markAlloc(size);
            POOL_ASSERT(pool != nullptr);
            return pool->getUser();
        }
        void free(void *p){
            FreeEntry *fe=(FreeEntry*)p;
            fe->next=freeList;
            freeList=fe;
            frees++;
        }
        size_t getAllocations() const {
            return allocations;
        }
        size_t getSize() const {
            return size;
        }
        void writeStats(std::stringstream &str) const {
            str << size << '/' << allocations << '/';
            str << frees << '/' << blocks;
            if (pool != nullptr){
                str << '/' << (pool->getCapacity() - poolUsed);
            } 
        }
        static void writeStatHeader(std::stringstream &str){
            str << "(sz/all/fre/blo/rem)";
        }
    };
    static constexpr const size_t MIN_SIZE=std::max(sizeof(FreeEntry),(size_t)8);
    static_assert(MIN_SIZE >= 8);
    

    template<class MMap, template<class> class MarkedAlloc>
    class SeparatingAlloc : public BaseAllocator{
        public:
        using MARKED_ALLOC=MarkedAlloc<MMap>;
        using MARKED_ALLOC_PTR=std::shared_ptr<MARKED_ALLOC>;
        using FIXED_ALLOC=FixedSizeAlloc<MarkedAlloc<MMap>>;
        private:
        MARKED_ALLOC_PTR markedPtr;
        MARKED_ALLOC *marked=nullptr;
        size_t largeAllocs=0;
        FIXED_ALLOC bins[10]{
            FIXED_ALLOC(MIN_SIZE),
            FIXED_ALLOC(MIN_SIZE << 1),
            FIXED_ALLOC(MIN_SIZE << 2),
            FIXED_ALLOC(MIN_SIZE << 3),
            FIXED_ALLOC(MIN_SIZE << 4),
            FIXED_ALLOC(MIN_SIZE << 5),
            FIXED_ALLOC(MIN_SIZE << 6),
            FIXED_ALLOC(MIN_SIZE << 7),
            FIXED_ALLOC(MIN_SIZE << 8),
            FIXED_ALLOC(MIN_SIZE << 9) //4096 (8<<9)
            };
        const size_t NUMBINS=sizeof(bins)/sizeof(FIXED_ALLOC);
        public:
        size_t maxSmall() const {
            return bins[NUMBINS-1].getSize();
        }
        int sizeToIdx(size_t size){
            if (size > maxSmall()) return -1;
            for (int i=0;i < NUMBINS;i++){
                if (size <= bins[i].getSize()) return i;
            }
            return 0;
        }
        virtual void *do_allocate(size_t n,size_t align){
            int id=sizeToIdx(n);
            if (id < 0) {
                largeAllocs++;
                POOL_ASSERT(marked);
                Block *rt=marked->map(n);
                if (rt == nullptr) return rt;
                return rt->getUser();
            }
            if (bins[id].getSize() < n) id++;
            POOL_ASSERT(bins[id].getSize() >= n);
            return bins[id].allocate(marked);
        }
        virtual void do_deallocate(void *p,size_t n){
            int id=sizeToIdx(n);
            if (id < 0) {
                Block *b=Block::fromUser(p);
                POOL_ASSERT(marked);
                marked->unmap(b);
                return;
            }
            bins[id].free(p);
        }
        SeparatingAlloc(const std::string &id,MARKED_ALLOC_PTR &a ):BaseAllocator(id){
            markedPtr=a;
            marked=markedPtr.get(); //faster access later w/o lock
        }  
        virtual ~SeparatingAlloc(){
            if (marked) marked->cleanUp();
        }
        virtual void logStatistics(const std::string &prefix) const override{
            LOG_DEBUG("%s: separatingAllocator %s allocated=%lld, max=%lld",prefix,id,allocated,max);
            if(Logger::instance()->HasLevel(LOG_LEVEL_DEBUG)){
                std::stringstream ss;
                FIXED_ALLOC::writeStatHeader(ss);
                ss << ':';
                for (int i=0;i<NUMBINS;i++){
                    bins[i].writeStats(ss);
                    ss << ',';
                }
                ss << "large/" << largeAllocs;
                LOG_DEBUG("allocator stats %s",ss.str());
            }
        }

    };
}
#endif
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  own allocator
 * Author:   Andreas Vogel
 *
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
#include "OcAllocatorImpl.h"
namespace ocalloc
{


    Block *MMap::map(size_t size)
    {
        size += sizeof(Block);
        size_t length = minSize;
        if (size > minSize)
        {
            size_t pages = (size % pageSize) ? size / pageSize +1 : size / pageSize ;
            length = pageSize * pages;
        }
        POOL_ASSERT(length >= size);
        Block *p = (Block *)mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        POOL_ASSERT(p != nullptr);
        p->capacity = length;
        mapped += length;
        return p;
    }
    void MMap::unmap(Block *p)
    {
        if (mapped >= p->capacity)
        {
            mapped -= p->capacity;
        }
        else
        {
            mapped = 0;
        }
        int rt = munmap(p, p->capacity);
        POOL_ASSERT(rt == 0);
    }
    class TestAlloc : public BaseAllocator
    {
    public:
        virtual void *do_allocate(size_t n, size_t align)
        {
            #ifdef AVNAV_ANDROID
                return nullptr;
            #else
                return aligned_alloc(align, n);
            #endif
        }
        virtual void do_deallocate(void *p, size_t n)
        {
            free(p);
        }
        using BaseAllocator::BaseAllocator;
    };

    Pool *makePool(const std::string &id)
    {
        auto mapper=std::make_shared<SeparatingAlloc<MMap,MarkedAlloc>::MARKED_ALLOC>();
        return new SeparatingAlloc<MMap,MarkedAlloc>(id,mapper);
    }

    void *BaseAllocator::do_allocate(size_t n, size_t align)
    {
        #ifdef AVNAV_ANDROID
            return nullptr;
        #else
            return aligned_alloc(align, n);
        #endif
    }
    void BaseAllocator::do_deallocate(void *p, size_t n)
    {
        free(p);
    }
    void BaseAllocator::deallocate(void *p, size_t n)
    {
        do_deallocate(p, n);
        if (allocated >= n)
            allocated -= n;
        else
            allocated = 0;
        POOL_LOG("[%s:%p] deallocate %lld->%p sum %lld, max %lld", id, this, n, p, allocated, max);
    }
    void *BaseAllocator::allocate(size_t n, size_t align)
    {
        void *rt = do_allocate(n, align);
        if (rt != nullptr)
        {
            allocated += n;
            if (allocated > max)
                max = allocated;
        }
        POOL_LOG("[%s:%p] allocate %lld(%lld)->%p sum %lld, max %lld", id, this, n, align, rt, allocated, max);
        return rt;
    }
    BaseAllocator::~BaseAllocator()
    {
        if (allocated == 0)
        {
            POOL_LOG("[%s:%p] destroy max %lld", id, this, max);
        }
        else
        {
            LOG_ERROR("[%s:%p] destroy not empty remain %lld max %lld", id, this, allocated, max);
        }
    }

    void BaseAllocator::logStatistics(const std::string &prefix) const{
        LOG_DEBUG("%s: baseAllocator %s allocated=%lld, max=%lld",prefix,id,allocated,max);
    }
}
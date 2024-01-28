/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  allocator using foonathan/memory
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
#ifndef _OCALLOCATOR_H
#define _OCALLOCATOR_H

#include "Logger.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unordered_set>
#include <map>
#include <memory>

//#define POOL_DEBUG 1
#ifdef POOL_DEBUG
#define POOL_LOG(...) LOG_DEBUG(__VA_ARGS__)
#else
#define POOL_LOG(...)
#endif

#define POOL_ASSERT(exp) {if (! (exp)) { std::cerr << "Assert failed" << #exp << __FILE__ << ':' << __LINE__ <<std::endl; abort();}}

namespace ocalloc{
    class BaseAllocator {
        protected:
            std::string id;
            size_t allocated=0;
            size_t max=0;
        public:
        BaseAllocator(const std::string &i):id(i){}
        BaseAllocator(const BaseAllocator &other) = delete;
        BaseAllocator(BaseAllocator &&other) =delete;
        protected:
        virtual void *do_allocate(size_t n,size_t align);
        virtual void do_deallocate(void *p,size_t n);
        public:
        void * allocate(size_t n,size_t align);
        void deallocate(void *p,size_t n);

        size_t currentSize() const {
            return allocated;
        }
        size_t maxSize() const {
            return max;
        }
        virtual ~BaseAllocator();
        virtual void logStatistics(const std::string &prefix) const;
    };

    template<typename Base>
    class AllocRef {
        private:
            Base *ptr=nullptr;
        public:
          void * allocate(size_t n,size_t align){
            POOL_ASSERT(ptr);
            return ptr->allocate(n,align);
          }
          void deallocate(void *p,size_t n){
            POOL_ASSERT(ptr);
            ptr->deallocate(p,n);
          }
          AllocRef(Base *b):ptr(b){}
          AllocRef(const AllocRef &o):ptr(o.ptr){}
          AllocRef(std::unique_ptr<Base> &up):ptr(up.get()){}
          AllocRef(AllocRef &&o):ptr(std::exchange(o.ptr,nullptr)){}
          AllocRef& operator=(const AllocRef &o){
            ptr=o.ptr;
            return *this;
          }
          AllocRef& operator=(AllocRef &&o){
            ptr=std::swap(o.ptr);
            return this;
          }
          bool operator==(const AllocRef &o){
            return ptr == o.ptr;
          }
    };

    template<typename T, class Alloc>
    class std_alloc{
        private:
            Alloc alloc;
        public:
        Alloc get_allocator() const{
            return alloc;
        }
        T* allocate(size_t n){
            return (T*)alloc.allocate(n *sizeof(T),alignof(T));
        }
        void deallocate(void *p,size_t n){
            alloc.deallocate(p,n * sizeof(T));
        }
        using value_type=T;
        std_alloc(Alloc &a):alloc(a){}
        std_alloc(const std_alloc &o):alloc(o.alloc){}
        std_alloc(std_alloc &&o):alloc(o.alloc){}
        template<typename O>
        std_alloc(const std_alloc<O,Alloc> &o):alloc(o.get_allocator()){}
        std_alloc & operator=(const std_alloc &o){
            alloc=o.alloc;
            return *this;
        }
        std_alloc select_on_container_copy_construction(){
            return std_alloc(*this);
        }
        bool operator==(const std_alloc &l){return alloc == l.alloc;}
        bool operator!=(const std_alloc &l){return ! (alloc == l.alloc);}
        using propagate_on_container_copy_assignment=std::true_type;
        using propagate_on_container_move_assignment=std::true_type;

    };
    
    using Pool=BaseAllocator;
    using PoolRef=AllocRef<Pool>;
    class String: public std::basic_string<char,std::char_traits<char>,std_alloc<char,PoolRef>>{
        public:
        using std::basic_string<char,std::char_traits<char>,std_alloc<char,PoolRef>>::basic_string;
        std::string str() const{
            return std::string(begin(),end());
        }
    };
    
    template<typename T>
    using Vector=std::vector<T,std_alloc<T,PoolRef>>;
    template<typename Key, typename Value>
    class Map: public std::map<Key,Value,std::less<Key>,std_alloc<std::pair<const Key,Value>,PoolRef>>{
        public:
        using std::map<Key,Value,std::less<Key>,std_alloc<std::pair<const Key,Value>,PoolRef>>::map;
        void set(const Key &k,const Value &v){
            auto it=this->find(k);
            if (it == this->end()) this-> insert(std::make_pair(k,v));
            else it->second=v;
        }
    };

    template<typename Key, typename Hash>
    using UnorderedSet=std::unordered_set<Key, Hash, std::equal_to<Key>, std_alloc<Key, PoolRef>>;

    template<typename T, typename ...Args>
    std::shared_ptr<T> allocate_shared(PoolRef& alloc, Args&&... args){
        return std::allocate_shared<T,std_alloc<T,PoolRef>>(alloc,std::forward<Args>(args)...);
    }
    template<typename T, typename ...Args>
    std::shared_ptr<T> allocate_shared_pool(PoolRef& alloc, Args&&... args){
        return std::allocate_shared<T,std_alloc<T,PoolRef>>(
                                               alloc,alloc,std::forward<Args>(args)...);
        //return memory::allocate_shared<T>(alloc,alloc,std::forward<Args>(args)...);
    }
    

    Pool* makePool(const std::string &id);
}

#endif

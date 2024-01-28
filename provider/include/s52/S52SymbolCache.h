/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Symbol Cache
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
#ifndef _S52_SYMBOLCACHE_H
#define _S52_SYMBOLCACHE_H
#include "Types.h"
#include "SimpleThread.h"
#include "DrawingContext.h"
#include "PngHandler.h"
#include "ItemStatus.h"
#include <atomic>
namespace s52{
    class SymbolData{
        public:
        typedef std::vector<DrawingContext::ColorAndAlpha> BufferType;
        typedef std::shared_ptr<BufferType> DataPtr;
        int width=0;
        int height=0;
        int minDist=0;
        int maxDist=0;
        int pivot_x=0;
        int pivot_y=0;
        int x=0; //offset UL
        int y=0;
        int rotation=0;
        double scale=1.0; 
        bool stagger=true;
        DrawingContext::ColorAndAlpha defaultColor=DrawingContext::convertColor(0,0,0); //for vector symbols
        String name;
        Coord::PixelBox relativeExtent;
        void compute(){
            relativeExtent.xmin=-pivot_x;
            relativeExtent.xmax=width-pivot_x;
            relativeExtent.ymin=-pivot_y;
            relativeExtent.ymax=height-pivot_y;
            relativeExtent.valid=true;
        }
        void createBuffer(){
            buffer = std::make_shared<SymbolData::BufferType>();
            buffer->resize(width * height);
        }
        uint8_t * getBufferPointer(){
            if (! buffer) return nullptr;
            return (uint8_t *)(buffer->data());
        }
        size_t getStride(){
            return width * sizeof(DrawingContext::ColorAndAlpha);
        }
        uint64_t numBytes() const{
            uint64_t rt=sizeof(this);
            if (buffer){
                rt+=buffer->size()*sizeof(DrawingContext::ColorAndAlpha);
            }
            return rt;
        }
        DataPtr buffer;
    };
    using SymbolPtr=std::shared_ptr<SymbolData>;
    //-- SYMBOLISATION MODULE STRUCTURE -----------------------------
    // position parameter:  LINE,       PATTERN, SYMBOL
    class SymbolPosition
    {
        public:
        int minDist=0;
        int maxDist=0;
        int pivot_x=0;
        int pivot_y=0;
        int bnbox_w;
        int bnbox_h;
        int bnbox_x=0; // UpLft crnr
        int bnbox_y=0; // UpLft crnr
        int glx;        //graphics location
        int gly;
        bool stagger=true; //filltype S->true, pattern only
    };

    class VectorSymbol
    {
        public:
        int minDist=0;
        int maxDist=0;
        int pivot_x=0;
        int pivot_y=0;
        int bnbox_w=0;
        int bnbox_h=0;
        int glx=0;
        int gly=0;
        String hpgl;
        String colorRef;
        bool stagger=true; //filltype S->true, pattern only

    };

    class SymbolCache : public ItemStatus{
        public:
        using GetColorFunction=std::function<DrawingContext::ColorAndAlpha(const String)>;
        private:
        class SymbolBase{
            protected:
            std::mutex lock;
            using SymList=std::vector<SymbolPtr>;
            SymList otherSymbols;
            public:
            class CreateParam{
                public:
                double scaleTolerance=0.1;
                int rotationTolerance=10;
            };
            using Ptr=std::shared_ptr<SymbolBase>;
            SymbolPtr baseSymbol=std::make_shared<SymbolData>();
            virtual SymbolPtr getOrCreate(SymbolCache::GetColorFunction colorGet, uint64_t &addedBytes, const CreateParam &param, int rotation=0,double scale=-1);
            virtual uint64_t numBytes() const{ return sizeof(this)+baseSymbol->numBytes()+otherSymbols.capacity()*sizeof(SymbolPtr);}
            virtual ~SymbolBase(){};
            protected:
            virtual SymbolPtr create(SymbolCache::GetColorFunction colorGet,const CreateParam &param, int rotation=0,double scale=-1);
        };
        class SymbolBaseVector : public SymbolBase{
            protected:
            virtual SymbolPtr create(SymbolCache::GetColorFunction colorGet, const CreateParam &param, int rotation=0,double scale=-1);
            public:
            String hpgl;
            String colorRef;
            using Ptr=std::shared_ptr<SymbolBaseVector>;
            virtual uint64_t numBytes() const{
                return SymbolBase::numBytes()+hpgl.capacity()+colorRef.capacity();
            }
        };
        public:
        using Ptr=std::shared_ptr<SymbolCache>;
        SymbolPtr getSymbol(const String &name,GetColorFunction colorGet, int rotation=0,double scale=-1);
        String checkSymbol(const String &name);
        bool fillRasterSymbol(const String &name,const SymbolPosition &position, PngReader *reader, double scale);
        bool fillVectorSymbol(const String &name,const VectorSymbol &position,double scale);
        SymbolCache(double scaleT, int rotationT);
        virtual void ToJson(StatusStream &stream);
        private:
        double scaleTolerance=0.1;
        int rotationTolerance=5;
        using BaseMap=std::map<String,SymbolBase::Ptr>;
        std::mutex lock;
        BaseMap baseMap;
        std::atomic<uint32_t> symbolEntries={0};
        std::atomic<uint64_t> memUsage={0};
    };

}
#endif

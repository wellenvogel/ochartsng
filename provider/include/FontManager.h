/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Font Manager
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
#include "Types.h"
#include "SimpleThread.h"
#include "memory"
#include "map"
#include "atomic"
#ifndef _FONTMANAGER_H
#define _FONTMANAGER_H

class FontFileHolder{
    public:
    using Ptr=std::shared_ptr<FontFileHolder>;
    class FontFileData;
    FontFileHolder(const String &name);
    ~FontFileHolder();
    void init();
    FontFileData *data;
    private:
    String fileName;
};
class FontManager{
    public:
    class FontInfo;
    class Glyph{
        public:
        typedef std::shared_ptr<Glyph> Ptr;
        typedef std::shared_ptr<const Glyph> ConstPtr;
        typedef uint8_t BType;
        int width=0;
        int height=0;
        int pivotX=0;
        int pivotY=0;
        int advanceX=0;
        BType *buffer=nullptr;
        Glyph(int w, int h, int pX=0, int pY=0):width(w),height(h),pivotX(pX),pivotY(pY){
            buffer=new BType[w*h*sizeof(BType)];
            advanceX=w;
        }
        Glyph(const Glyph &other)=delete;
        Glyph& operator = (const Glyph &) = delete;
        ~Glyph(){
            if (buffer) delete[] buffer;
        }

    };
    typedef std::shared_ptr<FontManager> Ptr;
    FontManager(FontFileHolder::Ptr font,int fontSize=16);
    void init();
    Glyph::ConstPtr getGlyph(int code);
    ~FontManager();
    int getLineHeight();
    int getAscend();
    int getDescend();
    int getCharWidth(uint32_t c='X');
    int getKernX(int code1, int code2);
    int getTextWidth(const std::u32string &str);
    protected:
    FontFileHolder::Ptr fontFile;
    FontInfo *info=nullptr;
    typedef std::map<int,Glyph::Ptr> GlyphCache;
    GlyphCache glyphCache;
    int fontSize=16;
    std::atomic<bool> initialized={false};
    std::mutex lock;
};
#endif
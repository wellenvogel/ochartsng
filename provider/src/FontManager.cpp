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
#include "FontManager.h"
#include "Logger.h"
#include "FileHelper.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define MAX_FONT_SIZE (1024*1024)

class FontManager::FontInfo{
    public:
    stbtt_fontinfo info;
    int ascent=0;
    int descent=0;
    int lineGap=0;
    int charWidth=0;
    float fontScale=1;
};

class FontFileHolder::FontFileData{
    public:
    stbtt_fontinfo info;
    unsigned char *fontData=nullptr;
    bool initialized=false;
    ~FontFileData(){
        delete fontData;
        //TODO: deinit font
    }
};

FontFileHolder::FontFileHolder(const String &fn){
    fileName=fn;
    data=new FontFileData();
}
FontFileHolder::~FontFileHolder(){
    delete data;
}
void FontFileHolder::init(){
    if (data->initialized) return;
    LOG_DEBUG("init font %s",fileName);
    if (! FileHelper::canRead(fileName)){
        throw FileException(fileName,"cannot read");
    }
    size_t fileSize=FileHelper::fileSize(fileName);
    if (fileSize > MAX_FONT_SIZE){
        throw FileException(fileName, FMT("to big: %d (max is %d)",fileSize,MAX_FONT_SIZE));
    }
    FILE *fp=fopen(fileName.c_str(),"rb");
    if (! fp){
        throw FileException(fileName,"unable to open");
    }
    data->fontData=new unsigned char[fileSize];
    size_t rd=fread(data->fontData,1,fileSize,fp);
    fclose(fp);
    if (rd != fileSize){
        throw FileException(fileName,FMT("only %d of %d bytes read",rd, fileSize));
    }
    int rt=stbtt_InitFont(&(data->info),data->fontData,stbtt_GetFontOffsetForIndex(data->fontData,0));
    if (!rt){
        throw FileException(fileName,"unable to init font");
    }
    data->initialized=true;
}

FontManager::FontManager(FontFileHolder::Ptr f, int fontSize): fontFile(f) {
    this->fontSize=fontSize;
}
FontManager::~FontManager(){
    if (info) delete info;
}
void FontManager::init() {
    if (initialized) return;
    info=new FontInfo();
    info->info=fontFile->data->info;
    info->fontScale=stbtt_ScaleForPixelHeight(&(info->info),fontSize);
    stbtt_GetFontVMetrics(&(info->info), &(info->ascent), &(info->descent), &(info->lineGap));
    info->ascent=roundf(info->fontScale * info->ascent);
    info->descent=roundf(info->fontScale * info->descent);
    info->lineGap=roundf(info->fontScale * info->lineGap);
    int ax;
	int lsb;
    stbtt_GetCodepointHMetrics(&(info->info),'X', &ax, &lsb);
    info->charWidth=roundf(ax*info->fontScale);
    initialized=true;
}
int FontManager::getCharWidth(uint32_t c){
    if (! initialized) throw AvException("font manager not initialized"); 
    if (c == 'X') return info->charWidth;
    int ax;
	int lsb;
    stbtt_GetCodepointHMetrics(&(info->info),c, &ax, &lsb);
    return roundf(ax*info->fontScale);
}
FontManager::Glyph::ConstPtr FontManager::getGlyph(int code) {
    if (! initialized) throw AvException("font manager not initialized"); 
    Synchronized l(lock);
    auto it=glyphCache.find(code);
    if (it != glyphCache.end()){
        return it->second;
    }
    int ax;
	int lsb;
    stbtt_GetCodepointHMetrics(&(info->info),code, &ax, &lsb);
    int c_x1, c_y1, c_x2, c_y2;
    stbtt_GetCodepointBitmapBox(&(info->info), code, info->fontScale, info->fontScale, &c_x1, &c_y1, &c_x2, &c_y2);
    Glyph::Ptr glyph= std::make_shared<Glyph>(c_x2-c_x1,c_y2-c_y1,c_x1,c_y1);
    glyph->advanceX=roundf(ax*info->fontScale);
    stbtt_MakeCodepointBitmap(&(info->info),glyph->buffer,glyph->width,glyph->height,glyph->width,info->fontScale,info->fontScale,code);
    glyphCache[code]=glyph;
    return glyph; 
    }
int FontManager::getLineHeight(){
    if (! initialized) throw AvException("font manager not initialized");
    return info->ascent-info->descent+info->lineGap;
}
int FontManager::getAscend(){
    if (! initialized) throw AvException("font manager not initialized");
    return info->ascent;
}
int FontManager::getDescend(){
    if (! initialized) throw AvException("font manager not initialized");
    return info->descent;
}
int FontManager::getKernX(int code1, int code2){
    if (! initialized) throw AvException("font manager not initialized");
    int kern=stbtt_GetCodepointKernAdvance(&(info->info), code1, code2);
    return roundf(kern*info->fontScale);
}
int FontManager::getTextWidth(const std::u32string &str){
    if (! initialized) throw AvException("font manager not initialized");
    int width=0;
    int ax,lsb;
    for (auto it=str.begin();it != str.end();it++){
        stbtt_GetCodepointHMetrics(&(info->info),*it, &ax, &lsb); 
        width+=roundf(ax * info->fontScale);
        if ((it+1) != str.end()){
           int kern=stbtt_GetCodepointKernAdvance(&(info->info), *it, *(it+1));
           width+=roundf(kern*info->fontScale); 
        }
    }
    return width;
}   
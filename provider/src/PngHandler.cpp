/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  PNG Encoder
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
#include "PngHandler.h"
#include "Exception.h"
#include "Logger.h"
#include <map>
#include <functional>
typedef std::function<PngEncoder*()> encoderCreator;
typedef std::function<PngReader*()> readerCreator;
static std::map<String,encoderCreator> creators;
static std::map<String,readerCreator> readerCreators;

class PngCreatorInstance{
    public:
        PngCreatorInstance(String name,encoderCreator f){
            creators[name]=f;
        }
        PngCreatorInstance(String name,encoderCreator f, readerCreator r){
            creators[name]=f;
            readerCreators[name]=r;
        }
};

#ifdef USE_FPNG
#include "fpng.h"
class FpngEncoder : public PngEncoder{
    public:
    FpngEncoder(): PngEncoder(){}
    virtual bool encode(DataPtr out) override{
        if (! context) return false;
        return fpng::fpng_encode_image_to_memory(
            context->getBuffer(),
            context->getWidth(),
            context->getHeight(),
            context->getBpp(),
            *out);
    }
};
static PngCreatorInstance fpngCreator("fpng",[](){
    return new FpngEncoder();
});

#endif
#ifdef USE_SPNG
#include "spng.h"
class PngOutStream{
    public:
    DataVector *data;
    int write(void *src,size_t length){
        data->insert(data->end(),(uint8_t *)src,(uint8_t *)src+length);
        return 0;
    }
};
int write_fn(spng_ctx *ctx, void *user, void *src, size_t length){
    PngOutStream *stream=(PngOutStream *)user;
    return stream->write(src,length);
}
class SpngEncoder : public PngEncoder
{
    public: 
    SpngEncoder():
        PngEncoder(){    
    }
    virtual bool encode(DataPtr out) override
    {
        if (! context) return false;
        PngOutStream stream;
        stream.data = out.get();
        spng_ctx *ctx = NULL;
        struct spng_ihdr ihdr = {0}; /* zero-initialize to set valid defaults */

        /* Creating an encoder context requires a flag */
        ctx = spng_ctx_new(SPNG_CTX_ENCODER);
        spng_set_png_stream(ctx, write_fn, &stream);
        ihdr.width = context->getWidth();
        ihdr.height = context->getHeight();
        ihdr.color_type = 6;
        ihdr.bit_depth = 8;
        spng_set_ihdr(ctx, &ihdr);
        int fmt = SPNG_FMT_PNG;

        /* SPNG_ENCODE_FINALIZE will finalize the PNG with the end-of-file marker */
        int ret = spng_encode_image(
            ctx, 
            context->getBuffer(),
            context->getBufferSize()*context->getBpp(), 
            fmt, 
            SPNG_ENCODE_FINALIZE);
        spng_ctx_free(ctx);
        if (ret)
        {
            return false;
        }
        return true;
    }
};

class SpngReader : public PngReader{
    using PngReader::PngReader;
    public:
    virtual bool read(const String &fileName) override{
        spng_ctx *ctx = NULL;
        FILE *png=NULL;
        avnav::VoidGuard cl([&ctx,&png](){
            if (ctx) spng_ctx_free(ctx);
            if (png) fclose(png);
        });
        png=fopen(fileName.c_str(),"rb");
        if (! png) throw FileException(fileName,"unable to open for read");
        ctx = spng_ctx_new(0);
        if(ctx == NULL) throw FileException(fileName,"unable to create spng context");
        spng_set_crc_action(ctx, SPNG_CRC_USE, SPNG_CRC_USE);
        size_t limit = 1024 * 1024 * 64;
        spng_set_chunk_limits(ctx, limit, limit);
        spng_set_png_file(ctx, png);
        struct spng_ihdr ihdr;
        int ret = spng_get_ihdr(ctx, &ihdr);
        if (ret) throw FileException(fileName,FMT("spng_get_ihdr: %s",spng_strerror(ret)));
        size_t image_size=0;
        int fmt=SPNG_FMT_RGBA8;
        ret = spng_decoded_image_size(ctx, fmt, &image_size);
        if (ret) throw FileException(fileName,FMT("spng_decoded_image_size: %s",spng_strerror(ret)));
        size_t bufferSize=image_size/sizeof(DrawingContext::ColorAndAlpha);
        buffer=new DrawingContext::ColorAndAlpha[bufferSize];
        ret = spng_decode_image(ctx, buffer, image_size, fmt, 0);
        if (ret) throw FileException(fileName,FMT("spng_decode_image: %s",spng_strerror(ret)));
        width=ihdr.width;
        height=ihdr.height;
        LOG_DEBUG("spng decode success for %s, witdh=%d, height=%d",fileName,width,height);
        return true;
    }
    virtual ~SpngReader(){
        delete [] buffer;
    }
    protected:

};
static PngCreatorInstance spngCreator("spng",[](){
    return new SpngEncoder();
},
[](){ return new SpngReader();}
);

#endif

PngEncoder *PngEncoder::createEncoder(String name){
    auto it=creators.find(name);
    if (it == creators.end()) return NULL;
    return it->second();
}
PngReader *PngReader::createReader(String name){
    auto it=readerCreators.find(name);
    if (it == readerCreators.end()) return NULL;
    return it->second();
}
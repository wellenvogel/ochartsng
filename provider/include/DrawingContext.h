/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Draw
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

#ifndef _DRAWINGCONTEXTH
#define _DRAWINGCONTEXTH
#include "Types.h"
#include "memory.h"
#include "Coordinates.h"

class DrawingContext
{

public:
    typedef uint32_t ColorAndAlpha;
    uint64_t numTriangles = 0;
    uint64_t triHitCount = 0;
    uint64_t triUnhitCount = 0;
    typedef struct
    {
        int draw = 0;
        int gap = 0;
        bool start = true; // if false start with gap
    } Dash;
    typedef enum{
        LINE_THICKNESS_DRAW_COUNTERCLOCKWISE,
        LINE_THICKNESS_DRAW_CLOCKWISE
    }ThicknessMode;

protected:
    class DashHandler
    {
        const Dash *dash;
        bool isDrawing = false;
        bool hasPoint = false;
        Coord::Pixel lastX = 0;
        Coord::Pixel lastY = 0;
        Coord::Pixel drawDist = 0;
        Coord::Pixel gapDist = 0;

    public:
        DashHandler(const Dash *d) : dash(d)
        {
            if (d)
            {
                isDrawing = d->start;
                drawDist = d->draw * d->draw;
                gapDist = d->gap * d->gap;
            }
        }
        bool shouldDraw(Coord::Pixel x, Coord::Pixel y)
        {
            if (drawDist == 0 || gapDist == 0)
                return true;
            if (!hasPoint)
            {
                lastX = x;
                lastY = y;
                hasPoint = true;
                return isDrawing;
            }
            Coord::Pixel dx = x - lastX;
            Coord::Pixel dy = y - lastY;
            Coord::Pixel dst = dx * dx + dy * dy;
            if (isDrawing)
            {
                if (dst >= drawDist)
                {
                    lastX = x;
                    lastY = y;
                    isDrawing = false;
                }
            }
            else
            {
                if (dst >= gapDist)
                {
                    lastX = x;
                    lastY = y;
                    isDrawing = true;
                }
            }
            return isDrawing;
        }
    };

public:
    DrawingContext(int width, int height);
    inline ColorAndAlpha *pixel(int x, int y)
    {
        if (x < 0)
            return NULL;
        if (x >= width)
            return NULL;
        if (y < 0)
            return NULL;
        if (y >= height)
            return NULL;
        int px = y * linelen + x;
        return buffer.get() + px;
    }
    inline void drawHLine(Coord::Pixel y, Coord::Pixel x0, Coord::Pixel x1, const ColorAndAlpha &color, bool useAlpha = false, const Dash *dash = nullptr)
    {
        if (y < 0 || y >= height)
            return;
        if (x1 < x0)
            std::swap(x0, x1);
        x0 = std::max(0, x0);
        x1 = std::min(x1, width - 1);
        ColorAndAlpha *dst = buffer.get() + y * linelen + x0;
        if (!dash)
        {
            for (Coord::Pixel x = x0; x <= x1; x++)
            {
                if (useAlpha)
                {
                    *dst = alpha(*dst, color);
                }
                else
                {
                    *dst = color;
                }
                dst++;
            }
        }
        else
        {
            DashHandler dh(dash);
            for (Coord::Pixel x = x0; x <= x1; x++)
            {
                if (dh.shouldDraw(x, y))
                {
                    if (useAlpha)
                    {
                        *dst = alpha(*dst, color);
                    }
                    else
                    {
                        *dst = color;
                    }
                }
                dst++;
            }
        }
    }
    inline void drawVLine(Coord::Pixel x, Coord::Pixel y0, Coord::Pixel y1, const ColorAndAlpha &color, bool useAlpha = false, const Dash *dash = nullptr)
    {
        if (x < 0 || x >= width)
            return;
        if (y1 < y0)
            std::swap(y0, y1);
        y0 = std::max(0, y0);
        y1 = std::min(y1, height - 1);
        ColorAndAlpha *dst = buffer.get() + y0 * linelen + x;
        if (!dash)
        {
            for (Coord::Pixel y = y0; y <= y1; y++)
            {
                if (useAlpha)
                {
                    *dst = alpha(*dst, color);
                }
                else
                {
                    *dst = color;
                }
                dst += linelen;
            }
        }
        else
        {
            DashHandler dh(dash);
            for (Coord::Pixel y = y0; y <= y1; y++)
            {
                if (dh.shouldDraw(x, y))
                {
                    if (useAlpha)
                    {
                        *dst = alpha(*dst, color);
                    }
                    else
                    {
                        *dst = color;
                    }
                }
                dst += linelen;
            }
        }
    }

protected:
    std::unique_ptr<ColorAndAlpha[]> buffer;
    int width;
    int height;
    int bufSize;
    int linelen;
    bool hasDrawn=false;
    bool checkOnly=false;
    static ColorAndAlpha factors[256];
    static ColorAndAlpha alphaImpl(ColorAndAlpha dst, const ColorAndAlpha &src);
    inline void setPixInt(int x, int y, ColorAndAlpha ca, bool swap = false)
    {
        ColorAndAlpha *px = NULL;
        if (!swap)
            px = pixel(x, y);
        else
            px = pixel(y, x);
        if (!px)
            return;
        hasDrawn = true;
        *px = ca;
    }
    inline void setPixAlpha(int x, int y, ColorAndAlpha ca, bool swap = false)
    {
        ColorAndAlpha *px = NULL;
        if (!swap)
            px = pixel(x, y);
        else
            px = pixel(y, x);
        if (!px)
            return;
        hasDrawn = true;
        *px = alpha(*px, ca);
    }
    inline void setPixWeight(int x, int y, ColorAndAlpha c, uint32_t weight, bool swap = false)
    {
        uint32_t alpha = (c & 0xFF000000) >> 24;
        alpha = ((alpha * weight) >> 8);
        if (alpha > 255)
            alpha = 255;
        else
            alpha = (alpha & 0xff);
        c &= 0xffffff;
        c |= ((ColorAndAlpha)alpha) << 24;
        return setPixAlpha(x, y, c, swap);
    }
    inline ColorAndAlpha alpha(ColorAndAlpha dst, const ColorAndAlpha &src)
    {
        //some initial checks to speed up for common cases
        if (src == 0) return dst;
        if (dst == 0) return src;
        if ((src & 0xff000000) == 0xff000000) return src;
        return alphaImpl(dst,src);
    }

public:
    virtual ~DrawingContext() {}
    class PatternSpec{
        /**
         * pattern are drawn by a fixed raster on the tile
         * (and by using xoffset/yoffset) on the world on a particular
         * zoom level
         * we ignore any pivot
         * the raster x distance = width + distance
         * the raster y distance = height + distance
         * so for any y coordinate the symbol coordinate is:
         *   (y + yoffset) % (height+distance)
         *   if (y >= height) - do not draw
         * for x similar:
         *   (x + xoffset) % (width + distance)
         *   if (x >= width) - do not draw
         * if stagger is set, every second row is shifted
         *   if ((y + yoffset)/(height + distance) & 1) - add (width+distance)/2 to x
         * the x/y offset are computed by the (tile x/y * TILE_SIZE) % (raster x/y distance)
         * see dda_tri in s52plib.cpp
        */
        public:
        //symbol data
        int width;
        int height;
        const ColorAndAlpha *buffer;
        //pattern info
        Coord::Pixel xoffset=0; //the x offset of the context x=0
        Coord::Pixel yoffset=0; //the y offset of the context y=0
        Coord::Pixel distance=0;
        bool stagger=false; //if true, shift every second symbol row by (width+distance)/2 
        PatternSpec(const ColorAndAlpha *b,int w, int h):
            buffer(b),width(w),height(h){}
        
    };
    static ColorAndAlpha convertColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha = 255);
    static inline uint8_t getAlpha(const ColorAndAlpha c)
    {
        return (c >> 24) & 0xff;
    }
    static inline ColorAndAlpha setAlpha(ColorAndAlpha c,uint8_t alpha){
        return (c & 0xffffff) | (((ColorAndAlpha)alpha) << 24);
    }
    void setPix(Coord::Pixel x, Coord::Pixel y, ColorAndAlpha color);
    virtual void drawRect(Coord::Pixel x0, Coord::Pixel y0, Coord::Pixel x1, Coord::Pixel y1, ColorAndAlpha color);
    virtual void drawLine(const Coord::PixelXy &p0, const Coord::PixelXy &p1, const ColorAndAlpha &color, bool alpha = false, const Dash *dash = nullptr, bool overlapMajor=false);
    virtual void drawThickLine(const Coord::PixelXy &p0, const Coord::PixelXy &p1, const DrawingContext::ColorAndAlpha &color, bool useAlpha, const DrawingContext::Dash *dash, unsigned int aThickness,
        ThicknessMode aThicknessMode=DrawingContext::LINE_THICKNESS_DRAW_CLOCKWISE);
    virtual void drawAaLine(const Coord::PixelXy &p0, const Coord::PixelXy &p1, DrawingContext::ColorAndAlpha color, const Dash *dash = nullptr);
    virtual void drawTriangle(const Coord::PixelXy &p0, const Coord::PixelXy &p1, const Coord::PixelXy &p2, const ColorAndAlpha &color, const PatternSpec *pattern=nullptr);
    virtual void drawSymbol(const Coord::PixelXy &p0 /*upper left*/, int width, int height, const ColorAndAlpha *buffer);
    virtual void drawGlyph(const Coord::PixelXy &p0 /*upper left*/, int width, int height, const uint8_t *buffer, ColorAndAlpha c);
    // draw filled if radiusInner >= 0
    virtual void drawArc(const Coord::PixelXy &center, const ColorAndAlpha &color, int radius, int radiusInner = -1, double startAngle = 0, double endAngle = 360);
    ColorAndAlpha *getBuffer() { return buffer.get(); }
    int getBufferSize() { return bufSize; }
    int getWidth() { return width; }
    int getHeight() { return height; }
    int getBpp() { return sizeof(ColorAndAlpha); }
    bool getDrawn() { return hasDrawn; }
    void resetDrawn(){ hasDrawn=false;}
    void setCheckOnly(bool v){checkOnly=v;}
    virtual void reset(ColorAndAlpha pattern = 0);
    virtual String getStatistics() const;
    static DrawingContext *create(int width, int height);
};
#endif
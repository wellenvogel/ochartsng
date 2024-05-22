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

#include "DrawingContext.h"
#include "Exception.h"
#include <memory.h>
#include <cmath>
#include <cstdlib>
#include "Logger.h"
namespace extthick{
    #include <math.h>
    using pbuffer_t=DrawingContext;
    void pixel(pbuffer_t *ctx,int x, int y, unsigned int color){
        ctx->setPix(x,y,color);
    }
/***********************************************************************
 *                                                                     *
 *                            X BASED LINES                            *
 *                                                                     *
 ***********************************************************************/

static void x_perpendicular(pbuffer_t *B, unsigned int color,
                            int x0,int y0,int dx,int dy,int xstep, int ystep,
                            int einit,int w_left, int w_right,int winit)
{
    if (dx <= 0) return;
  int x,y,threshold,E_diag,E_square;
  int tk;
  int error;
  int p,q;

  threshold = dx - 2*dy;
  E_diag= -2*dx;
  E_square= 2*dy;
  p=q=0;

  y= y0;
  x= x0;
  error= einit;
  tk= dx+dy-winit; 

  while(tk<=w_left)
  {
     pixel(B,x,y, color);
     if (error>=threshold)
     {
       x= x + xstep;
       error = error + E_diag;
       tk= tk + 2*dy;
     }
     error = error + E_square;
     y= y + ystep;
     tk= tk + 2*dx;
     q++;
  }

  y= y0;
  x= x0;
  error= -einit;
  tk= dx+dy+winit;

  while(tk<=w_right)
  {
     if (p)
       pixel(B,x,y, color);
     if (error>threshold)
     {
       x= x - xstep;
       error = error + E_diag;
       tk= tk + 2*dy;
     }
     error = error + E_square;
     y= y - ystep;
     tk= tk + 2*dx;
     p++;
  }

  if (q==0 && p<2) pixel(B,x0,y0,color); // we need this for very thin lines
}


static void x_varthick_line
   (pbuffer_t *B, unsigned int color,
    int x0,int y0,int dx,int dy,int xstep, int ystep,
    double (*left)(void *,int ,int), void *argL,
    double (*right)(void *,int ,int),void *argR, int pxstep,int pystep)
{
  int p_error, error, x,y, threshold, E_diag, E_square, length, p;
  int w_left, w_right;
  double D;


  p_error= 0;
  error= 0;
  y= y0;
  x= x0;
  threshold = dx - 2*dy;
  E_diag= -2*dx;
  E_square= 2*dy;
  length = dx+1;
  D= sqrt(dx*dx+dy*dy);

  for(p=0;p<length;p++)
  {
    w_left=  (*left)(argL, p, length)*2*D;
    w_right= (*right)(argR,p, length)*2*D;
    x_perpendicular(B,color,x,y, dx, dy, pxstep, pystep,
                                      p_error,w_left,w_right,error);
    if (error>=threshold)
    {
      y= y + ystep;
      error = error + E_diag;
      if (p_error>=threshold) 
      {
        x_perpendicular(B,color,x,y, dx, dy, pxstep, pystep,
                                    (p_error+E_diag+E_square), 
                                     w_left,w_right,error);
        p_error= p_error + E_diag;
      }
      p_error= p_error + E_square;
    }
    error = error + E_square;
    x= x + xstep;
  }
}

/***********************************************************************
 *                                                                     *
 *                            Y BASED LINES                            *
 *                                                                     *
 ***********************************************************************/

static void y_perpendicular(pbuffer_t *B, unsigned int color,
                            int x0,int y0,int dx,int dy,int xstep, int ystep,
                            int einit,int w_left, int w_right,int winit)
{
  if (dy <=  0) return;
  int x,y,threshold,E_diag,E_square;
  int tk;
  int error;
  int p,q;

  p=q= 0;
  threshold = dy - 2*dx;
  E_diag= -2*dy;
  E_square= 2*dx;

  y= y0;
  x= x0;
  error= -einit;
  tk= dx+dy+winit; 

  while(tk<=w_left)
  {
     pixel(B,x,y, color);
     if (error>threshold)
     {
       y= y + ystep;
       error = error + E_diag;
       tk= tk + 2*dx;
     }
     error = error + E_square;
     x= x + xstep;
     tk= tk + 2*dy;
     q++;
  }


  y= y0;
  x= x0;
  error= einit;
  tk= dx+dy-winit; 

  while(tk<=w_right)
  {
     if (p)
       pixel(B,x,y, color);
     if (error>=threshold)
     {
       y= y - ystep;
       error = error + E_diag;
       tk= tk + 2*dx;
     }
     error = error + E_square;
     x= x - xstep;
     tk= tk + 2*dy;
     p++;
  }

  if (q==0 && p<2) pixel(B,x0,y0,color); // we need this for very thin lines
}


static void y_varthick_line
   (pbuffer_t *B, unsigned int color,
    int x0,int y0,int dx,int dy,int xstep, int ystep,
    double (*left)(void *,int ,int), void *argL,
    double (*right)(void *,int ,int),void *argR,int pxstep,int pystep)
{
  int p_error, error, x,y, threshold, E_diag, E_square, length, p;
  int w_left, w_right;
  double D;

  p_error= 0;
  error= 0;
  y= y0;
  x= x0;
  threshold = dy - 2*dx;
  E_diag= -2*dy;
  E_square= 2*dx;
  length = dy+1;
  D= sqrt(dx*dx+dy*dy);

  for(p=0;p<length;p++)
  {
    w_left=  (*left)(argL, p, length)*2*D;
    w_right= (*right)(argR,p, length)*2*D;
    y_perpendicular(B,color,x,y, dx, dy, pxstep, pystep,
                                      p_error,w_left,w_right,error);
    if (error>=threshold)
    {
      x= x + xstep;
      error = error + E_diag;
      if (p_error>=threshold)
      {
        y_perpendicular(B,color,x,y, dx, dy, pxstep, pystep,
                                     p_error+E_diag+E_square,
                                     w_left,w_right,error);
        p_error= p_error + E_diag;
      }
      p_error= p_error + E_square;
    }
    error = error + E_square;
    y= y + ystep;
  }
}


/***********************************************************************
 *                                                                     *
 *                                ENTRY                                *
 *                                                                     *
 ***********************************************************************/

pbuffer_t* draw_varthick_line
      (pbuffer_t *B, unsigned int color,
       int x0,int y0,int x1, int y1,
       double (*left)(void *,int ,int), void *argL,
       double (*right)(void *,int ,int), void *argR)
{
  int dx,dy,xstep,ystep;
  int pxstep, pystep;
  int xch; // whether left and right get switched.

  dx= x1-x0;
  dy= y1-y0;
  xstep= ystep= 1;

  if (dx<0) { dx= -dx; xstep= -1; }
  if (dy<0) { dy= -dy; ystep= -1; }

  if (dx==0) xstep= 0;
  if (dy==0) ystep= 0;


  xch= 0;
  switch(xstep + ystep*4)
  {
    case -1 + -1*4 :  pystep= -1; pxstep= 1; xch= 1; break;   // -5
    case -1 +  0*4 :  pystep= -1; pxstep= 0; xch= 1; break;   // -1
    case -1 +  1*4 :  pystep=  1; pxstep= 1; break;   // 3
    case  0 + -1*4 :  pystep=  0; pxstep= -1; break;  // -4
    case  0 +  0*4 :  pystep=  0; pxstep= 0; break;   // 0
    case  0 +  1*4 :  pystep=  0; pxstep= 1; break;   // 4
    case  1 + -1*4 :  pystep= -1; pxstep= -1; break;  // -3
    case  1 +  0*4 :  pystep= -1; pxstep= 0;  break;  // 1
    case  1 +  1*4 :  pystep=  1; pxstep= -1; xch=1; break;  // 5
  }

  if (xch) { 
    std::swap(argL,argR);
    std::swap(left,right);
    }

  if (dx>dy) x_varthick_line(B,color,x0,y0,dx,dy,xstep,ystep,
                                                left,argL,right,argR,
                                                pxstep,pystep);
        else y_varthick_line(B,color,x0,y0,dx,dy,xstep,ystep,
                                                left,argL,right,argR,
                                                pxstep,pystep);
  return B;
}

double thick(void *p,int x, int y){
    return *((double *)p);
}

};

DrawingContext::ColorAndAlpha DrawingContext::alphaImpl(DrawingContext::ColorAndAlpha dst, const DrawingContext::ColorAndAlpha &src){
    // A over B (A: src, B: dst)
        // https://de.wikipedia.org/wiki/Alpha_Blending
        // https://stackoverflow.com/questions/1102692/how-to-alpha-blend-rgba-unsigned-byte-color-fast
        // https://arxiv.org/pdf/2202.02864.pdf
        ColorAndAlpha aA = (src & 0xFF000000) >> 24;
        if (aA == 255 ) return src;
        ColorAndAlpha aB = (dst & 0xFF000000) >> 24;
        if (aB == 0)
            return src;
        if (aA == 0) return dst;
        ColorAndAlpha bF=(((255-aA)*aB) + (1<<7)) >> 8;
        ColorAndAlpha anew = aA+bF;
        ColorAndAlpha f=factors[anew];
        const int shifts[3]={0,8,16};
        ColorAndAlpha rt= (anew << 24);
        for (int i=0;i<3;i++){
            ColorAndAlpha c=((src >> shifts[i]) &0xff)*f*aA;
            c+=((dst >> shifts[i])&0xff)*f*bF;
            c+=1<<15; //+0.5 for better rounding
            c=c>>16;
            if (c > 255){
                c=255;
            }
            rt|=c << shifts[i];
        }
        return rt;
}

DrawingContext::ColorAndAlpha DrawingContext::convertColor(uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
    ColorAndAlpha rt = alpha;
    rt <<= 8;
    rt |= b;
    rt <<= 8;
    rt |= g;
    rt <<= 8;
    rt |= r;
    return rt;
}
DrawingContext::DrawingContext(int width, int height)
{
    this->hasDrawn = false;
    this->width = width;
    this->height = height;
    this->bufSize = width * height;
    this->linelen = width;
    this->buffer = std::make_unique<ColorAndAlpha[]>(this->bufSize);
    reset();
}
static inline Coord::Pixel limit(Coord::Pixel v, Coord::Pixel max)
{
    if (v < 0)
        return 0;
    if (v >= max)
        return max - 1;
    return v;
}
void DrawingContext::setPix(int x, int y, ColorAndAlpha color)
{
    setPixInt(x, y, color);
}

void DrawingContext::drawRect(Coord::Pixel x0, Coord::Pixel y0, Coord::Pixel x1, Coord::Pixel y1, ColorAndAlpha color)
{
    x0 = limit(x0, width);
    x1 = limit(x1, width);
    if (x0 > x1)
        std::swap(x0, x1);
    y0 = limit(y0, height);
    y1 = limit(y1, height);
    if (y0 > y1)
        std::swap(y0, y1);
    ColorAndAlpha *start = buffer.get() + y0 * linelen + x0;
    ColorAndAlpha *vp = start;
    for (int y = y0; y <= y1; y++)
    {
        vp = start;
        for (int x = x0; x <= x1; x++)
        {
            *vp = color;
            vp++;
            hasDrawn=true;
            if (checkOnly) return;
        }
        start += linelen;
    }
}
void DrawingContext::reset(ColorAndAlpha pattern)
{
    ColorAndAlpha *p = buffer.get();
    ColorAndAlpha *pe = buffer.get() + width * height;
    while (p < pe)
    {
        *p = pattern;
        p++;
    }
    hasDrawn=false;
}


/*!
\brief function to draw an anti-aliased line with alpha blending.

This implementation of the Wu antialiasing code is based on Mike Abrash's
DDJ article which was reprinted as Chapter 42 of his Graphics Programming
Black Book, but has been optimized to work with 32-bit
fixed-point arithmetic

*/
void DrawingContext::drawAaLine(const Coord::PixelXy &pp0, const Coord::PixelXy &pp1, DrawingContext::ColorAndAlpha color, const DrawingContext::Dash *dash)
{
    if (pp0.y == pp1.y)
    {
        drawHLine(pp0.y, pp0.x, pp1.x, color, true);
        return;
    }
    if (pp0.x == pp1.x)
    {
        drawVLine(pp0.x, pp0.y, pp1.y, color,true);
        return;
    }
    Coord::PixelXy p0=pp0;
    Coord::PixelXy p1=pp1;
    Coord::Pixel xx0, yy0,y0p1;
    
    // TODO: range check
    int32_t dx = std::abs(p0.x - p1.x);
    int32_t dy = std::abs(p0.y - p1.y);
    //ensure fixed octant for line
    //dy < dx, p0.x < p1.x
    bool steep = false;
    if (dy > dx)
    {
        steep = true;
        std::swap(p0.x, p0.y);
        std::swap(p1.x, p1.y);
        std::swap(dx,dy);
    }
    if (p0.x > p1.x)
    {
        std::swap(p0, p1);
    }
    int32_t dx2 = dx << 1;
    int32_t dy2 = dy << 1;
    int32_t stepy=1;
    if (p0.y > p1.y){
        stepy=-1;
    }
    int32_t error = -dx;
    //weight runs in sync with error
    //error of -dx -> weight 0 (after step)
    //error of +dx -> weight 255 (before step)
    //weight is (255 *(error+dx)/(2 * dx)) = 127 + 127 *error/dx 
    int32_t weight = 0;
    int32_t weightIncX = -255; //when we substract 2*dx from error
                              //we substract 255 from weight
    int32_t weightIncY = (255*dy)/dx;                          

    /*
     * Draw the initial pixel in the foreground color
     */
    DashHandler dh(dash);
    if (dh.shouldDraw(p0.x, p0.y)){
        setPixAlpha(p0.x, p0.y, color, steep);
        if (checkOnly && hasDrawn) return;
    }

    yy0 = p0.y;
    xx0 = p0.x;

    /*
     * draw all pixels other than the first and last
     */
    y0p1 = yy0 + stepy;
    int numX=dx;
    while (--numX)
    {

        error += dy2;
        weight+=weightIncY;
        if (error > dx)
        {
            //should step y now
            yy0 = y0p1;
            y0p1+=stepy;
            error -= dx2;
            weight += weightIncX;
            if (weight < 0) weight=0;
        }
        if (weight > 255) weight=255; //avoid weight running out of sync
        xx0 += 1; /* x-major so always advance X */
        
        //determine weight for current pixel
        //and pixel with currentx and last y
        //after a step the weight must be minimal, increasing
        //towards the next step
        if (dh.shouldDraw(xx0,y0p1)){
            setPixWeight(xx0, yy0, color, 255-weight, steep);
            setPixWeight(xx0, y0p1, color, weight, steep);
        }
        if (checkOnly && hasDrawn) return;
    }
    if (dh.shouldDraw(p1.x,p1.y)){
        setPixAlpha(p1.x, p1.y, color, steep);
    }
}

void DrawingContext::drawArc(const Coord::PixelXy &center, const DrawingContext::ColorAndAlpha &color, int radius, int radiusInner, double start, double end)
{
    /*
     * Sanity check radius
     */
    if (radius < 0)
    {
        return;
    }

    /*
     * Special case for rad=0 - draw a point
     */
    if (radius == 0)
    {
        setPixInt(center.x, center.y, color);
        return;
    }
    bool drawFull = false;
    if (start == 0 && end == 360)
    {
        drawFull = true;
    }
    start -= 90;
    end -= 90;

    /*
     * Fixup angles
     */
    int istart = start;
    istart %= 360;
    int iend = end;
    iend %= 360;
    // 0 <= start & end < 360; note that sometimes start > end - if so, arc goes back through 0.
    while (istart < 0)
        istart += 360;
    while (iend < 0)
        iend += 360;
    istart %= 360;
    iend %= 360;

    // we can do the computation of the octants
    // once for all different radiuses
    // as the determination of the octets does not depend on it
    // we finally simply have to multiply the dstart/dend values with the
    // radius to get the start/stop pixel values

    uint8_t octetMask;
    int startoct, endoct, oct, stopval_start = 0, stopval_end = 0;
    // normalized pixel values to start/stop iteration
    // must be multiplied with the radius to get real pixel values
    double dstart = 0., dend = 0.;

    // Octant labelling
    //
    //  \ 5 | 6 /
    //   \  |  /
    //  4 \ | / 7
    //     \|/
    //------+------ +x
    //     /|\
	//  3 / | \ 0
    //   /  |  \
	//  / 2 | 1 \
	//      +y

    // Initially reset bitmask to 0x00000000
    // the set whether or not to keep drawing a given octant.
    // For example: 0x00111100 means we're drawing in octants 2-5
    octetMask = 0;
    if (drawFull)
    {
        octetMask = 255;
    }
    else
    {

        // now, we find which octants we're drawing in.
        startoct = istart / 45;
        endoct = iend / 45;
        oct = startoct - 1; // we increment as first step in loop

        // stopval_start, stopval_end;
        // what values of cx to stop at.
        do
        {
            oct = (oct + 1) % 8;

            if (oct == startoct)
            {
                // need to compute stopval_start for this octant.  Look at picture above if this is unclear
                switch (oct)
                {
                case 0:
                case 3:
                    dstart = sin(start * M_PI / 180.);
                    break;
                case 1:
                case 6:
                    dstart = cos(start * M_PI / 180.);
                    break;
                case 2:
                case 5:
                    dstart = -cos(start * M_PI / 180.);
                    break;
                case 4:
                case 7:
                    dstart = -sin(start * M_PI / 180.);
                    break;
                }

                // This isn't arbitrary, but requires graph paper to explain well.
                // The basic idea is that we're always changing octetMask after we draw, so we
                // stop immediately after we render the last sensible pixel at x = ((int)temp).

                // and whether to draw in this octant initially
                if (oct % 2)
                    octetMask |= (1 << oct); // this is basically like saying octetMask[oct] = true, if octetMask were a bool array
                else
                    octetMask &= 255 - (1 << oct); // this is basically like saying octetMask[oct] = false
            }
            if (oct == endoct)
            {
                // need to compute stopval_end for this octant
                switch (oct)
                {
                case 0:
                case 3:
                    dend = sin(end * M_PI / 180);
                    break;
                case 1:
                case 6:
                    dend = cos(end * M_PI / 180);
                    break;
                case 2:
                case 5:
                    dend = -cos(end * M_PI / 180);
                    break;
                case 4:
                case 7:
                    dend = -sin(end * M_PI / 180);
                    break;
                }

                // and whether to draw in this octant initially
                if (startoct == endoct)
                {
                    // note:      we start drawing, stop, then start again in this case
                    // otherwise: we only draw in this octant, so initialize it to false, it will get set back to true
                    if (start > end)
                    {
                        // unfortunately, if we're in the same octant and need to draw over the whole circle,
                        // we need to set the rest to true, because the while loop will end at the bottom.
                        octetMask = 255;
                    }
                    else
                    {
                        octetMask &= 255 - (1 << oct);
                    }
                }
                else if (oct % 2)
                    octetMask &= 255 - (1 << oct);
                else
                    octetMask |= (1 << oct);
            }
            else if (oct != startoct)
            {                            // already verified that it's != endoct
                octetMask |= (1 << oct); // draw this entire segment
            }
        } while (oct != endoct);
    }
    // so now we have what octants to draw and when to draw them. all that's left is the actual raster code.

    do
    {
        Coord::Pixel cx = 0;
        Coord::Pixel cxi = 0;
        Coord::Pixel cy = radius;
        Coord::Pixel df = 1 - radius;
        Coord::Pixel d_e = 3;
        Coord::Pixel d_se = -2 * radius + 5;
        Coord::Pixel xpcx, xmcx, xpcy, xmcy;
        Coord::Pixel ypcy, ymcy, ypcx, ymcx;
        uint8_t drawoct = octetMask;
        stopval_start = (int)(dstart * radius); // always round down
        stopval_end = (int)(dend * radius);

        /*
         * Draw arc
         */
        do
        {
            ypcy = center.y + cy;
            ymcy = center.y - cy;
            if (cx > 0)
            {
                xpcx = center.x + cx;
                xmcx = center.x - cx;

                // always check if we're drawing a certain octant before adding a pixel to that octant.
                if (drawoct & 4)
                    setPixInt(xmcx, ypcy, color);
                if (drawoct & 2)
                    setPixInt(xpcx, ypcy, color);
                if (drawoct & 32)
                    setPixInt(xmcx, ymcy, color);
                if (drawoct & 64)
                    setPixInt(xpcx, ymcy, color);
            }
            else
            {
                if (drawoct & 96)
                    setPixInt(center.x, ymcy, color);
                if (drawoct & 6)
                    setPixInt(center.x, ypcy, color);
            }
            if (checkOnly && hasDrawn) return;
            xpcy = center.x + cy;
            xmcy = center.x - cy;
            if (cx > 0 && cx != cy)
            {
                ypcx = center.y + cx;
                ymcx = center.y - cx;
                if (drawoct & 8)
                    setPixInt(xmcy, ypcx, color);
                if (drawoct & 1)
                    setPixInt(xpcy, ypcx, color);
                if (drawoct & 16)
                    setPixInt(xmcy, ymcx, color);
                if (drawoct & 128)
                    setPixInt(xpcy, ymcx, color);
            }
            else if (cx == 0)
            {
                if (drawoct & 24)
                    setPixInt(xmcy, center.y, color);
                if (drawoct & 129)
                    setPixInt(xpcy, center.y, color);
            }
            if (checkOnly && hasDrawn) return;
            if (!drawFull)
            {
                /*
                 * check if we passed startval or stopval
                 * and toggle the octant mask for the start or stop octant
                 */
                if (stopval_start == cx)
                {
                    // works like an on-off switch.
                    // This is just in case start & end are in the same octant.
                    if (drawoct & (1 << startoct))
                        drawoct &= 255 - (1 << startoct);
                    else
                        drawoct |= (1 << startoct);
                }
                if (stopval_end == cx)
                {
                    if (drawoct & (1 << endoct))
                        drawoct &= 255 - (1 << endoct);
                    else
                        drawoct |= (1 << endoct);
                }
            }
            /*
             * Update pixels
             */
            if (df < 0)
            {
                df += d_e;
                d_e += 2;
                d_se += 2;
            }
            else
            {
                df += d_se;
                d_e += 2;
                d_se += 4;
                cy--;
            }
            cx++;
        } while (cx <= cy);
        radius--;
    } while (radiusInner >= 0 && radius >= radiusInner && radius > 0);
}


void DrawingContext::drawLine(const Coord::PixelXy &p0, const Coord::PixelXy &p1, const DrawingContext::ColorAndAlpha &color, bool useAlpha, const DrawingContext::Dash *dash, bool overlap){
    //see https://stackoverflow.com/questions/40884680/how-to-use-bresenhams-line-drawing-algorithm-with-clipping
    //sort by y
    Coord::Pixel x1=p0.x;
    Coord::Pixel y1 = p0.y;
    Coord::Pixel x2=p1.x;
    Coord::Pixel y2 = p1.y;
    //Vertical line
    ColorAndAlpha *dst=buffer.get();
    ColorAndAlpha *start=dst;
    ColorAndAlpha *end=start+(width*height);
    if (x1 == x2)
    {
        drawVLine(x1,y1,y2,color,useAlpha,dash);
        return;
    }
    //Horizontal line
    if (y1 == y2)
    {
        drawHLine(y1,x1,x2,color,useAlpha,dash);
        return;
    }
    //sort by y
    if (y1 > y2){
        std::swap(x1,x2);
        std::swap(y1,y2);
    }
    //Now simple cases are handled, perform clipping checks.
    Coord::Pixel sign_x=1;
    if (x1 < x2){
        if (x1 >= width || x2 < 0){
            return;
        }
    }
    else{
        if (x2 >= width || x1 < 0){
            return;
        }
        sign_x = -1;
    }
    Coord::Pixel sign_y=1;
        if (y1 >=height || y2 < 0){
            return;
        }
    Coord::Pixel dx = ::abs(x2 - x1);
    Coord::Pixel dy = ::abs(y2 - y1);
    bool majorX=dx>dy;
    dy=0-dy;

    Coord::Pixel dx2 = 2 * dx;
    Coord::Pixel dy2 = 2 * dy;

    // Plotting values
    Coord::Pixel x = x1;
    Coord::Pixel y = y1;
    DashHandler dh(dash);
    dh.shouldDraw(x,y);
    Coord::Pixel error=dx2+dy2;
    //get us to the intersecting start point
    //first y
    if (y < 0){
        Coord::Pixel step=0-y;
        y+=step;
        error+=step*dx2;
        if (error > dy){
           Coord::Pixel stepx=(error-dy-dy/2)/-dy2; 
           if (stepx > 0){
               x+=stepx*sign_x;
               error+=stepx*dy2;
           }
        }
        if (sign_x > 0 && x >=width) {
            return;
        }
        if (sign_x < 0 && x < 0){
            return;
        }
    }
    Coord::Pixel stepx=0;
    if (sign_x > 0 && x < 0){
        stepx=0-x;
    }
    if (sign_x < 0 && x >= width){
        stepx=x-width+1;
    }
    if (stepx){
        x+=sign_x*stepx;
        error+=stepx*dy2;
        if (error < dx){
            Coord::Pixel stepy=(dx+dx/2-error)/dx2;
            y+=stepy;
            if (y >= height) {
                return;
            }
            error+=stepy*dx2;
        }
    }
    dst=buffer.get()+y*linelen+x;
    while (y <= y2 && y < height && x >= 0 && x < width &&
           ((sign_x > 0 && x <= x2) || (sign_x < 0 && x >= x2)))
    {
        hasDrawn=true;
        if (checkOnly) return;
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
        Coord::Pixel lastx=x;
        Coord::Pixel lasty=y;
        if (error > dy)
        {
            error += dy2;
            x += sign_x;
            dst += sign_x;
        }
        if (error < dx)
        {
            error += dx2;
            y += 1;
            dst += linelen;
        }
        if (overlap && lastx != x && lasty != y ){
            //we just draw the pixel with the minor and major value before the step
            //this could draw pixels twice and will this way not correctly work with alpha!
            ColorAndAlpha *md=dst;
            int offsets[]={linelen,sign_x};
            for (int i = 0; i < 2; i++)
            {
                ColorAndAlpha *md2 = dst;
                md -= offsets[i];
                if (md >= start & md < end)
                {
                    if (useAlpha)
                    {
                        *md = alpha(*md, color);
                    }
                    else
                    {
                        *md = color;
                    }
                }
            }
        }
    }
}
/**
 * taken from https://github.com/ArminJo/STMF3-Discovery-Demos/blob/master/lib/graphics/src/thickLine.cpp.
 *            https://github.com/ArminJo/STMF3-Discovery-Demos/blob/master/lib/BlueDisplay/LocalGUI/ThickLine.hpp
 * GPL
 * Bresenham with thickness
 * No pixel missed and every pixel only drawn once!
 * The code is bigger and more complicated than drawThickLineSimple() but it tends to be faster, since drawing a pixel is often a slow operation.
 * aThicknessMode can be one of LINE_THICKNESS_MIDDLE, LINE_THICKNESS_DRAW_CLOCKWISE, LINE_THICKNESS_DRAW_COUNTERCLOCKWISE
 */
void DrawingContext::drawThickLine(const Coord::PixelXy &p0, const Coord::PixelXy &p1, const DrawingContext::ColorAndAlpha &color, bool useAlpha, const DrawingContext::Dash *dash, unsigned int aThickness,
        DrawingContext::ThicknessMode aThicknessMode) {
    double thickness=(double)aThickness;
    extthick::draw_varthick_line(this,color,p0.x,p0.y,p1.x,p1.y,
        extthick::thick,&thickness,extthick::thick,&thickness
        );
    return;        
    Coord::Pixel i, tDeltaX, tDeltaY, tDeltaXTimes2, tDeltaYTimes2, tError, tStepX, tStepY;
    Coord::PixelXy pstart=p0;
    Coord::PixelXy pend=p1;

    if (aThickness <= 1) {
        drawLine(p0,p1,color,useAlpha,dash);
        return;
    }
    /**
     * For coordinate system with 0.0 top left
     * Swap X and Y delta and calculate clockwise (new delta X inverted)
     * or counterclockwise (new delta Y inverted) rectangular direction.
     * The right rectangular direction for LINE_OVERLAP_MAJOR toggles with each octant
     */
    tDeltaY = pend.x - pstart.x;
    tDeltaX = pend.y - pstart.y;
    // mirror 4 quadrants to one and adjust deltas and stepping direction
    bool tSwap = true; // count effective mirroring
    if (tDeltaX < 0) {
        tDeltaX = -tDeltaX;
        tStepX = -1;
        tSwap = !tSwap;
    } else {
        tStepX = +1;
    }
    if (tDeltaY < 0) {
        tDeltaY = -tDeltaY;
        tStepY = -1;
        tSwap = !tSwap;
    } else {
        tStepY = +1;
    }
    tDeltaXTimes2 = tDeltaX << 1;
    tDeltaYTimes2 = tDeltaY << 1;
    bool tOverlap;
    // adjust for right direction of thickness from line origin
    int tDrawStartAdjustCount = aThickness / 2;
    /*
    if (aThicknessMode == LINE_THICKNESS_DRAW_COUNTERCLOCKWISE) {
        tDrawStartAdjustCount = aThickness - 1;
    } else if (aThicknessMode == LINE_THICKNESS_DRAW_CLOCKWISE) {
        tDrawStartAdjustCount = 0;
    }
    */

    /*
     * Now tDelta* are positive and tStep* define the direction
     * tSwap is false if we mirrored only once
     */
    // which octant are we now
    if (tDeltaX >= tDeltaY) {
        // Octant 1, 3, 5, 7 (between 0 and 45, 90 and 135, ... degree)
        if (tSwap) {
            tDrawStartAdjustCount = (aThickness - 1) - tDrawStartAdjustCount;
            tStepY = -tStepY;
        } else {
            tStepX = -tStepX;
        }
        /*
         * Vector for draw direction of the starting points of lines is rectangular and counterclockwise to main line direction
         * Therefore no pixel will be missed if LINE_OVERLAP_MAJOR is used on change in minor rectangular direction
         */
        // adjust draw start point
        tError = tDeltaYTimes2 - tDeltaX;
        for (i = tDrawStartAdjustCount; i > 0; i--) {
            // change X (main direction here)
            pstart.x -= tStepX;
            pend.x -= tStepX;
            if (tError >= 0) {
                // change Y
                pstart.y -= tStepY;
                pend.y -= tStepY;
                tError -= tDeltaXTimes2;
            }
            tError += tDeltaYTimes2;
        }
        // draw start line. We can alternatively use drawLineOverlap(aXStart, aYStart, aXEnd, aYEnd, LINE_OVERLAP_NONE, aColor) here.
        drawLine(pstart, pend, color,useAlpha,dash);
        if (checkOnly && hasDrawn) return;
        // draw aThickness number of lines
        tError = tDeltaYTimes2 - tDeltaX;
        for (i = aThickness; i > 1; i--) {
            // change X (main direction here)
            pstart.x += tStepX;
            pend.x += tStepX;
            tOverlap = false;
            if (tError >= 0) {
                // change Y
                pstart.y += tStepY;
                pend.y += tStepY;
                tError -= tDeltaXTimes2;
                /*
                 * Change minor direction reverse to line (main) direction
                 * because of choosing the right (counter)clockwise draw vector
                 * Use LINE_OVERLAP_MAJOR to fill all pixel
                 *
                 * EXAMPLE:
                 * 1,2 = Pixel of first 2 lines
                 * 3 = Pixel of third line in normal line mode
                 * - = Pixel which will additionally be drawn in LINE_OVERLAP_MAJOR mode
                 *           33
                 *       3333-22
                 *   3333-222211
                 * 33-22221111
                 *  221111                     /\
                 *  11                          Main direction of start of lines draw vector
                 *  -> Line main direction
                 *  <- Minor direction of counterclockwise of start of lines draw vector
                 */
                tOverlap = true;
            }
            tError += tDeltaYTimes2;
            drawLine(pstart, pend, color,useAlpha,dash,tOverlap);
            if (checkOnly && hasDrawn) return;
        }
    } else {
        // the other octant 2, 4, 6, 8 (between 45 and 90, 135 and 180, ... degree)
        if (tSwap) {
            tStepX = -tStepX;
        } else {
            tDrawStartAdjustCount = (aThickness - 1) - tDrawStartAdjustCount;
            tStepY = -tStepY;
        }
        // adjust draw start point
        tError = tDeltaXTimes2 - tDeltaY;
        for (i = tDrawStartAdjustCount; i > 0; i--) {
            pstart.y -= tStepY;
            pend.y -= tStepY;
            if (tError >= 0) {
                pstart.x -= tStepX;
                pend.x -= tStepX;
                tError -= tDeltaYTimes2;
            }
            tError += tDeltaXTimes2;
        }
        //draw start line
        drawLine(pstart,pend,color,useAlpha,dash);
        if (checkOnly && hasDrawn) return;
        // draw aThickness number of lines
        tError = tDeltaXTimes2 - tDeltaY;
        for (i = aThickness; i > 1; i--) {
            pstart.y += tStepY;
            pend.y += tStepY;
            tOverlap = false;
            if (tError >= 0) {
                pstart.x += tStepX;
                pend.x += tStepX;
                tError -= tDeltaYTimes2;
                tOverlap = true;
            }
            tError += tDeltaXTimes2;
            drawLine(pstart, pend,color,useAlpha,dash,tOverlap);
            if (checkOnly && hasDrawn) return;
        }
    }
}

class SBresenHam
{
    // see https://de.wikipedia.org/wiki/Bresenham-Algorithmus
    // simplified version
    // we rely on points being sorted by y
public:
    Coord::Pixel dx;
    Coord::Pixel sx;
    Coord::Pixel dy;
    Coord::Pixel dy2;
    Coord::Pixel dx2;
    Coord::Pixel sy;
    Coord::Pixel x;
    Coord::Pixel y;
    Coord::PixelXy p1;
    int64_t err;
    typedef Coord::Box<Coord::Pixel> Clip;
    SBresenHam(Coord::PixelXy p0, Coord::PixelXy p1)
    {
        this->p1 = p1;
        dx = ::abs(p1.x - p0.x);
        dx2 = dx *2;
        sx = p0.x < p1.x ? 1 : -1;
        dy = -::abs(p1.y - p0.y);
        dy2= dy << 1;
        sy = 1; // we rely on sorted points
        x = p0.x;
        y = p0.y;
        err = dx2 + dy2;
    }
    inline void stepy(Coord::Pixel newY)
    {
        if (dy == 0) return;
        Coord::Pixel step = (newY > p1.y) ? p1.y - y : newY - y;
        if (step == 0) return;
        y += step;
        err += step * dx2;
        if (err <= dy)
            return; // no x step 
        /*       
        while (err > dy)
        {
            if (x == p1.x)
                break;
            x += sx;
            err += dy2;
        }
        */
        Coord::Pixel stepx=(err-dy-dy/2)/-dy2;
        if (stepx <= 0) {
            return;
        }
        x+= stepx *sx;
        if ((sx > 0 && x > p1.x) || (sx < 0 && x < p1.x)){
            x=p1.x;
        }
        err+=stepx*dy2;
        if (err > dy)
        {
            int x = 1; // debug error
        }
    }
};

int64_t orient2d(const Coord::PixelXy &a, const Coord::PixelXy &b, const Coord::PixelXy &c)
{
    return (int64_t)(b.x - a.x) * (int64_t)(c.y - a.y) - (int64_t)(b.y - a.y) * (int64_t)(c.x - a.x);
}

void DrawingContext::drawSymbol(const Coord::PixelXy &p0 /*upper left*/,int swidth, int sheight, const DrawingContext::ColorAndAlpha *sbuf){
    if (p0.y >= height) return;
    if (p0.x >= width) return;
    int xmin=std::max(0,p0.x);
    int ymin=std::max(0,p0.y);
    int ymax=std::min(p0.y+sheight,height);
    int xmax=std::min(p0.x+swidth,width);
    ColorAndAlpha *dstLine=buffer.get()+ymin*linelen+xmin;
    const ColorAndAlpha *srcLine=sbuf+(ymin-p0.y)*swidth+(xmin-p0.x);
    for (int y=ymin;y<ymax;y++){
        const ColorAndAlpha *src=srcLine;
        ColorAndAlpha *dst=dstLine;
        for (int x=xmin;x<xmax;x++){
            *dst=alpha(*dst,*src);
            hasDrawn=true;
            if (checkOnly) return;
            dst++;
            src++;
        }
        srcLine+=swidth;
        dstLine+=linelen;
    }
}
void DrawingContext::drawGlyph(const Coord::PixelXy &p0 /*baseline left*/,int swidth, int sheight, const uint8_t *sbuf, ColorAndAlpha c){
    if (p0.y >= height) return;
    if (p0.x >= width) return;
    int xmin=std::max(0,p0.x);
    int ymin=std::max(0,p0.y);
    int ymax=std::min(p0.y+sheight,height);
    int xmax=std::min(p0.x+swidth,width);
    ColorAndAlpha *dstLine=buffer.get()+ymin*linelen+xmin;
    const uint8_t *srcLine=sbuf+(ymin-p0.y)*swidth+(xmin-p0.x);
    for (int y=ymin;y<ymax;y++){
        const uint8_t *src=srcLine;
        ColorAndAlpha *dst=dstLine;
        for (int x=xmin;x<xmax;x++){
            
            ColorAndAlpha dc=c & 0xffffff;
            dc|=((ColorAndAlpha)(*src)) << 24;
            *dst=alpha(*dst,dc);
            hasDrawn=true;
            if (checkOnly) return;
            dst++;
            src++;
        }
        srcLine+=swidth;
        dstLine+=linelen;
    }
}
void DrawingContext::drawTriangle(const Coord::PixelXy &p00, const Coord::PixelXy &p10, const Coord::PixelXy &p20, const ColorAndAlpha &color,const PatternSpec *pattern)
{
    numTriangles++;
    Coord::Pixel pxRaster=pattern?pattern->width+pattern->distance:0;
    Coord::Pixel pyRaster=pattern?pattern->height+pattern->distance:0;
    Coord::Box<Coord::Pixel> extent;
    Coord::PixelXy p0 = p00;
    Coord::PixelXy p1 = p10;
    Coord::PixelXy p2 = p20;
    extent.extend(p0);
    extent.extend(p1);
    extent.extend(p2);
    //TODO: normalize! extent
    if (p0.y > p1.y)
        std::swap(p0, p1);
    if (p0.y > p2.y)
        std::swap(p0, p2);
    if (p1.y > p2.y)
        std::swap(p1, p2);
    SBresenHam p0p1(p0, p1);
    SBresenHam p1p2(p1, p2);
    SBresenHam p0p2(p0, p2);
    Coord::Pixel ymin = std::max(0, extent.ymin);
    Coord::Pixel ymax = std::min(height - 1, extent.ymax);
    int specialCase = 0;
    if (p0.y == p1.y)
        specialCase = 1;
    if (p1.y == p2.y)
        specialCase = 2;
    for (Coord::Pixel y = ymin; y <= ymax; y++)
    {
        Coord::Pixel xmin = 0;
        Coord::Pixel xmax = 0;
        switch (specialCase)
        {
        case 1:
            p0p2.stepy(y);
            p1p2.stepy(y);
            xmin=p0p2.x;
            xmax=p1p2.x;
            break;
        case 2:
            p0p1.stepy(y);
            p0p2.stepy(y);
            xmin=p0p1.x;
            xmax=p0p2.x;
            break;
        default:
            p0p2.stepy(y);
            if (y <= p1.y)
            {
                p0p1.stepy(y);
                xmin = p0p1.x;
                xmax = p0p2.x;
            }
            else
            {
                p1p2.stepy(y);
                xmin = p1p2.x;
                xmax = p0p2.x;
            }
        }
        if (xmin > xmax)
            std::swap(xmin, xmax);
        if (xmax >= 0 && xmin < width)
        {
            // draw line
            xmin = std::max(0, xmin);
            xmax = std::min(width - 1, xmax);
            if (pattern == nullptr){
                ColorAndAlpha *ptr = buffer.get() + y * linelen + xmin;
                for (Coord::Pixel x = xmin; x <= xmax; x++)
                {
                    hasDrawn=true;
                    if (checkOnly) return;
                    *ptr = color;
                    ptr++;
                }
            }
            else{
                if (pyRaster > 0 && pxRaster > 0)
                {
                    Coord::Pixel patterny = (y + pattern->yoffset) % pyRaster;
                    if (patterny < pattern->height && patterny >= 0)
                    {
                        Coord::Pixel pxOffset = 0;
                        if (pattern->stagger)
                        {
                            if (((y + pattern->yoffset) / pyRaster) & 1) //shift pattern in every second row
                            {
                                pxOffset=pxRaster/2;
                            }
                        }
                        const ColorAndAlpha *psrc= pattern->buffer+patterny*pattern->width;
                        ColorAndAlpha *ptr = buffer.get() + y * linelen;
                        for (Coord::Pixel x=xmin;x <= xmax; x++){
                            Coord::Pixel patternx= (x+pattern->xoffset+pxOffset)%pxRaster;
                            if (patternx < pattern->width && patternx >= 0){
                                ColorAndAlpha *target = ptr + x;
                                ColorAndAlpha src= *(psrc+patternx);
                                hasDrawn=true;
                                *target=alpha(*target,src);
                                if (checkOnly) return;
                            }
                        }
                    }
                }
            }
        }
    }
    return;
}

String DrawingContext::getStatistics() const
{
    std::stringstream out;
    out << "Stat: numTri=" << numTriangles;
    if (numTriangles > 0)
    {
        out << " counts:" << triHitCount << "," << triUnhitCount;
        if ((triHitCount + triUnhitCount) > 0)
        {
            out << "(" << (int)(triHitCount * 100 / (triHitCount + triUnhitCount)) << ")";
        }
    }
    return out.str();
}


DrawingContext *DrawingContext::create(int width, int height)
{
    return new DrawingContext(width, height);
}

DrawingContext::ColorAndAlpha DrawingContext::factors[256]={
    //table for 255*256/x
    1, //never used
    65280,
    32640,
    21760,
    16320,
    13056,
    10880,
    9325,
    8160,
    7253,
    6528,
    5934,
    5440,
    5021,
    4662,
    4352,
    4080,
    3840,
    3626,
    3435,
    3264,
    3108,
    2967,
    2838,
    2720,
    2611,
    2510,
    2417,
    2331,
    2251,
    2176,
    2105,
    2040,
    1978,
    1920,
    1865,
    1813,
    1764,
    1717,
    1673,
    1632,
    1592,
    1554,
    1518,
    1483,
    1450,
    1419,
    1388,
    1360,
    1332,
    1305,
    1280,
    1255,
    1231,
    1208,
    1186,
    1165,
    1145,
    1125,
    1106,
    1088,
    1070,
    1052,
    1036,
    1020,
    1004,
    989,
    974,
    960,
    946,
    932,
    919,
    906,
    894,
    882,
    870,
    858,
    847,
    836,
    826,
    816,
    805,
    796,
    786,
    777,
    768,
    759,
    750,
    741,
    733,
    725,
    717,
    709,
    701,
    694,
    687,
    680,
    672,
    666,
    659,
    652,
    646,
    640,
    633,
    627,
    621,
    615,
    610,
    604,
    598,
    593,
    588,
    582,
    577,
    572,
    567,
    562,
    557,
    553,
    548,
    544,
    539,
    535,
    530,
    526,
    522,
    518,
    514,
    510,
    506,
    502,
    498,
    494,
    490,
    487,
    483,
    480,
    476,
    473,
    469,
    466,
    462,
    459,
    456,
    453,
    450,
    447,
    444,
    441,
    438,
    435,
    432,
    429,
    426,
    423,
    421,
    418,
    415,
    413,
    410,
    408,
    405,
    402,
    400,
    398,
    395,
    393,
    390,
    388,
    386,
    384,
    381,
    379,
    377,
    375,
    373,
    370,
    368,
    366,
    364,
    362,
    360,
    358,
    356,
    354,
    352,
    350,
    349,
    347,
    345,
    343,
    341,
    340,
    338,
    336,
    334,
    333,
    331,
    329,
    328,
    326,
    324,
    323,
    321,
    320,
    318,
    316,
    315,
    313,
    312,
    310,
    309,
    307,
    306,
    305,
    303,
    302,
    300,
    299,
    298,
    296,
    295,
    294,
    292,
    291,
    290,
    288,
    287,
    286,
    285,
    283,
    282,
    281,
    280,
    278,
    277,
    276,
    275,
    274,
    273,
    272,
    270,
    269,
    268,
    267,
    266,
    265,
    264,
    263,
    262,
    261,
    260,
    259,
    258,
    257,
    256
};

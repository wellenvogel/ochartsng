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
#include "S52SymbolCache.h"
#include "S52Data.h"
#include "Logger.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
namespace s52{

    static int scalef(int value, double scale){
        return ::round((double)value * scale);
    }

    

    /**
     * rotating a symbol
     * see e.g. https://wuecampus.uni-wuerzburg.de/moodle/mod/book/view.php?id=958001&chapterid=10071
     * for the rotation (using clockwise and sin(-a)=-sin(a), cos(-a)=cos(a))
     * xr=xr0+(x-x0)*cos(a)-(y-y0)*sin(a)
     * yr=yr0+(x-x0)*sin(a)+(y-y0)*cos(a)
     * and for the reverse function (a -> -a)
     * xi=x0+(x-rx0)*cos(a)+(y-yr0)*sin(a)
     * yi=y0-(x-xr0)*sin(a)+(y-yr0)*cos(a)
     * Steps:
     * (1) compute rotate corner coordinates
     *     determine minx,mniy,maxx,maxy
     *     and compute new width, height and the shift of the 
     *     center (i.e. pivot) by bringing minx->0,miny->0
     * (2) create destination buffer
     * (3) iterate over detsination buffer
     *     using the ideas from https://github.com/DanBloomberg/leptonica/blob/master/src/rotateam.c
     *     to compute the target pixel from a 2x2 source
    */

    static Coord::Point<float> rotf(Coord::PixelXy xy, Coord::PixelXy xyr0, Coord::PixelXy xy0,float sina,float cosa){
        float x= (float)xyr0.x+(float)(xy.x-xy0.x)*cosa-(float)(xy.y-xy0.y)*sina;
        float y= (float)xyr0.y+(float)(xy.x-xy0.x)*sina+(float)(xy.y-xy0.y)*cosa;
        return Coord::Point<float>(x,y);
    }
    /**
     * rotation function
     * that uses int arithmetics
     * shifted by 4 bit
    */
    static Coord::PixelXy rotf16(Coord::PixelXy xy, Coord::PixelXy xyr0, Coord::PixelXy xy0,float sina,float cosa){
        Coord::Pixel x= (xyr0.x << 4) +(float)(xy.x-xy0.x)*cosa-(float)(xy.y-xy0.y)*sina;
        Coord::Pixel y= (xyr0.y << 4) +(float)(xy.x-xy0.x)*sina+(float)(xy.y-xy0.y)*cosa;
        return Coord::PixelXy(x,y);
    }

    class RotationParam{
        public:
        int rotation=0;
        float sina=0;
        float cosa=1;
        int width=0;
        int height=0;
        Coord::PixelXy pivot;
        Coord::PixelXy pivot_in;
        bool valid=false;
        template<typename T>
        Coord::Point<T> rotf(Coord::PixelXy xy, Coord::PixelXy xyr0){
            float x= (float)xyr0.x+(float)(xy.x-pivot_in.x)*cosa-(float)(xy.y-pivot_in.y)*sina;
            float y= (float)xyr0.y+(float)(xy.x-pivot_in.x)*sina+(float)(xy.y-pivot_in.y)*cosa;
            return Coord::Point<T>(x,y);
        }
        template<typename T>
        Coord::Point<T> rotf(Coord::PixelXy xy){
            return rotf<T>(xy,pivot);
        }
        void computeBox(){
            Coord::Point<float> ul=rotf<float>(Coord::PixelXy(0,0));
            Coord::Point<float> ur=rotf<float>(Coord::PixelXy(width,0));
            Coord::Point<float> ll=rotf<float>(Coord::PixelXy(0,height));
            Coord::Point<float> lr=rotf<float>(Coord::PixelXy(width,height));
            float minx=std::fmin(std::fmin(ul.x,ur.x),std::fmin(ll.x,lr.x));
            float maxx=std::fmax(std::fmax(ul.x,ur.x),std::fmax(ll.x,lr.x));        
            float miny=std::fmin(std::fmin(ul.y,ur.y),std::fmin(ll.y,lr.y));
            float maxy=std::fmax(std::fmax(ul.y,ur.y),std::fmax(ll.y,lr.y));
            width=ceil(maxx)-floor(minx)+1;
            height=ceil(maxy)-floor(miny)+1;
            //shift the pivot to make minx=0,miny=0
            pivot.x=pivot_in.x-ceil(minx);
            pivot.y=pivot_in.y-ceil(miny);    
        }
        RotationParam(){}
        RotationParam(int rot, const Coord::PixelXy &pivot_in, int w=0, int h=0 ){
            this->pivot_in=pivot_in;
            pivot=pivot_in;
            width=w;
            height=h;
            rotation=rot;
            if (rot == 0) return;
            valid=true;
            double angle=M_PI/180.0*rotation;
            sina=sin(angle);
            cosa=cos(angle);
            if (w != 0 && h != 0){
                computeBox();
            }
        }
    };

    static SymbolPtr rotateSymbol(const SymbolPtr in,int rotationDeg){
        rotationDeg=rotationDeg%360;
        if (rotationDeg == 0){
            return in;
        }
        if (! in->buffer) return in;
        Coord::PixelXy pivot(in->pivot_x,in->pivot_y);
        RotationParam rotp(rotationDeg,pivot,in->width,in->height);
        SymbolPtr rot=std::make_shared<SymbolData>(*in);
        rot->rotation=rotationDeg;
        rot->width=rotp.width;
        rot->height=rotp.height;
        //shift the pivot to make minx=0,miny=0
        rot->pivot_x=rotp.pivot.x;
        rot->pivot_y=rotp.pivot.y;
        rot->createBuffer();
        rot->compute();

        //invers rotation - we use integers multiplied by 16
        //for sub pixel accuracy
        float rsin16=(-rotp.sina * 16.0);
        float rcos16=(rotp.cosa * 16.0);
        Coord::PixelXy pivot_r(rot->pivot_x,rot->pivot_y);
        DrawingContext::ColorAndAlpha *target=rot->buffer->data();
        DrawingContext::ColorAndAlpha *src=in->buffer->data();
        for (Coord::Pixel y=0;y<rot->height;y++){
            for (Coord::Pixel x=0;x<rot->width;x++){
                Coord::PixelXy xy(x,y);
                Coord::PixelXy orig16=rotf16(xy,pivot,pivot_r,rsin16,rcos16);
                Coord::Pixel orix=orig16.x >> 4;
                Coord::Pixel oriy=orig16.y >> 4;
                DrawingContext::ColorAndAlpha *dst=target+y*rot->width+x;
                if (orix <0 || orix >= in->width || oriy < 0 || oriy >= in->height){
                    *dst=0;
                    continue;
                }
                uint32_t xf=orig16.x & 0xf;
                uint32_t yf=orig16.y & 0xf;
                DrawingContext::ColorAndAlpha *scpix=src+oriy*in->width+orix;
                DrawingContext::ColorAndAlpha word00 = *scpix;
                DrawingContext::ColorAndAlpha word10=(orix < (in->width-1))?*(scpix+1):0;
                DrawingContext::ColorAndAlpha word01 = 0;
                DrawingContext::ColorAndAlpha word11 = 0;
                if (oriy < (in->height-1)){
                    word01=*(scpix+in->width);
                    if (orix < (in->width-1)){
                        word11=*(scpix+in->width+1);
                    }
                }
                DrawingContext::ColorAndAlpha dv=0;
                size_t shifts[4]={0,8,16,24};
                for (int i=0;i<4;i++){
                    size_t shift=shifts[i];
                    uint32_t v = ((16 - xf) * (16 - yf) * ((word00 >> shift) & 0xff) +
                    xf * (16 - yf) * ((word10 >> shift) & 0xff) +
                    (16 - xf) * yf * ((word01 >> shift) & 0xff) +
                    xf * yf * ((word11 >> shift) & 0xff) + 128) / 256;
                    dv|= (v & 0xff) << shift;
                }
                *dst=dv;    
            }
        }
    return rot;
    }

    static SymbolPtr resizeSymbol(const SymbolPtr in,double factor,DrawingContext::ColorAndAlpha * readerBuffer=NULL, size_t readerWidth=0){
        int scaledW=scalef(in->width,factor);
        int scaledH=scalef(in->height,factor);
        bool mustScale=true;
        if ( std::fabs(1.0-factor) < 1e-3 || (scaledH == in->height && scaledW == in->width )) {
            mustScale=false;
        }
        if (! mustScale && in->buffer){
            return in;
        }
        if (! in->buffer && ! readerBuffer) return in;
        SymbolPtr rt=std::make_shared<SymbolData>(*in);
        rt->scale=in->scale*factor;
        rt->width=scaledW;
        rt->height=scaledH;
        rt->pivot_x=scalef(in->pivot_x,factor);
        rt->pivot_y=scalef(in->pivot_y,factor);
        rt->compute();
        rt->createBuffer();
        if (mustScale){
        stbir_resize_uint8(
                readerBuffer?(const unsigned char *)readerBuffer:(const unsigned char *)(in->getBufferPointer()),
                in->width,
                in->height,
                readerBuffer?readerWidth*sizeof(DrawingContext::ColorAndAlpha):in->getStride(),
                (unsigned char *)(rt->getBufferPointer()),
                rt->width,
                rt->height,
                rt->getStride(),
                4   
            );
        }
        else{
            for (int yt = 0; yt < in->height; yt++)
            {
                for (int xt = 0; xt < in->width; xt++)
                {
                    DrawingContext::ColorAndAlpha *src = readerBuffer + yt * readerWidth + xt;
                    rt->buffer->at(yt * rt->width + xt) = *src;
                }
            }    
        }
        return rt;
    }

    //HPGL scaling:
    //from s52lib (s52plib::SetPPMM): 810 units should become 32 pixel
    class HPGLRender{
        public:
        class Operation{
            public:
            String op;
            String param;
            Coord::PixelXy point;
            bool hasPoint=false;
            void setPoint(const Coord::PixelXy &p){
                point=p;
                hasPoint=true;
            }
            Operation(){}
            Operation(const String &o,const String &p):op(o),param(p){}
            Operation(const String &o,const Coord::PixelXy &p):op(o){
                setPoint(p);
            }
        };
        using Mode = enum{
            M_LS=0 //mode line style
        };
        Mode mode=M_LS;
        std::unique_ptr<DrawingContext> ctx;
        SymbolPtr symbol;
        SymbolPtr position;
        double scale=1;
        RotationParam rotp;
        std::vector<Operation> operations;
        Coord::PixelXy drawOffset=Coord::PixelXy(0,0);  //offset used to ensure we draw > 0
        float scalef(int in){
            return (float) in * 32.0 * scale /810.0;
        }
        float scalef(float in){
            return in * 32.0 * scale /810.0;
        }
        Coord::Point<float> scalef(const Coord::PixelXy &in){
            return Coord::Point<float>(scalef(in.x),scalef(in.y));
        }
        Coord::Point<float> scalef(const Coord::Point<float> &in){
            return Coord::Point<float>(scalef(in.x),scalef(in.y));
        }
        /* point computation seems to be different...
           for line styles:
           in o-charts_pi:
           r - point on the line
           origin - pivot from symbol def
           pivot - pivot from symbol def
           RotatePoint( lineStart, origin, rot_angle );
            lineStart -= pivot;
            lineStart.x /= scaleFactor;
            lineStart.y /= scaleFactor;
            lineStart += r;
           we assume r to be 0,0 (later we use drawOffset)
        */
        Coord::PixelXy computePoint(const Coord::PixelXy &orig){
            Coord::Point<float> rt=rotp.rotf<float>(orig);
            if (mode == M_LS){
                rt.x-=position->pivot_x;
                rt.y-=position->pivot_y;
            }
            rt=scalef(rt);
            return Coord::PixelXy(round(rt.x),round(rt.y));
        }
        HPGLRender(SymbolPtr pos,int rot,double sf,Mode m):scale(sf),position(pos),mode(m){
            if (mode == M_LS){
                rotp=RotationParam(rot,Coord::PixelXy(pos->pivot_x,pos->pivot_y)); //the vector origin is x,y
            }
        }
        bool parsePoint(const String &v, Coord::PixelXy &point, size_t &start){
            if (start >= v.size()) return false;
            size_t found=v.find(',',start);
            if (found == String::npos) return false;
            point.x=std::atol(v.substr(start,found-start).c_str());
            start=found+1;
            found=v.find(',',start);
            if (found == String::npos){
                point.y=std::atol(v.substr(start).c_str());
                start=v.size();
            }
            else{
                point.y=std::atol(v.substr(start,found-start).c_str());
                start=found+1;
            }
            point=computePoint(point);
            return true;
        }
        bool addAction(const String &action){
            String cmd=action.substr(0,2);
            String param;
            if (action.size() > 2) param=action.substr(2);
            if (cmd == "SP" || cmd == "SW" || cmd == "ST" || cmd == "CI" || cmd == "PM" || cmd == "FP"){
                operations.push_back(Operation(cmd,param));
                return true;
            }
            if (cmd == "PU"){
                Coord::PixelXy point;
                size_t start=0;
                if (parsePoint(param,point,start)){
                    operations.push_back(Operation(cmd,point));
                }
                return true;
            }
            if (cmd == "PD"){
                Coord::PixelXy point;
                bool hasOne=false;
                size_t start=0;
                //PD can have multiple points within a polygon
                //so we simply add multiple PDs
                while (parsePoint(param,point,start)){
                    hasOne=true;
                    operations.push_back(Operation(cmd,point));
                }
                if (! hasOne){
                    operations.push_back(Operation(cmd,param));
                }
                return true;
            }
            return false;
        }
        bool parse(const String &hpgl){
            size_t pos=0;
            size_t found=0;
            while ((found = hpgl.find(';', pos)) != String::npos)
            {
                addAction(hpgl.substr(pos,found-pos));
                pos=found+1;
            }
            if (pos < hpgl.size()){
                addAction(hpgl.substr(pos));
            }
            return true;
        }

        void computeBox(){
            Coord::PixelXy lastPoint;
            bool hasPoint=false;
            int width=1;
            Coord::PixelBox box;
            for (auto it=operations.begin();it != operations.end();it++){
                if (! it->hasPoint){
                    if (it->op == "PD" && hasPoint){
                        //for PD without point we use the last point
                        //with x incremented by one
                        it->setPoint(lastPoint.getShifted(1,0));
                    }
                }
                if (it->hasPoint){
                    lastPoint=it->point;
                    hasPoint=true;
                    box.extend(lastPoint.getShifted(-width,-width));
                    box.extend(lastPoint.getShifted(width,width));
                }
                if (hasPoint){
                    if (it->op == "CI"){
                        //circle
                        float radius=scalef(atoi(it->param.c_str()));
                        box.extend(lastPoint.getShifted(0-width-radius,0-width-radius));
                        box.extend(lastPoint.getShifted(width+radius,width+radius));
                        box.extend(lastPoint.getShifted(width+radius,0-width-radius));
                        box.extend(lastPoint.getShifted(0-width-radius,width+radius));
                    }
                }
                if (it->op == "SW"){
                    width=atoi(it->param.c_str());
                    if (width < 1) width=1;
                }
            }
            if (! symbol) symbol=std::make_shared<SymbolData>(*position);
            symbol->width =box.xmax-box.xmin;
            symbol->height=box.ymax-box.ymin;
            symbol->rotation=rotp.rotation;
            symbol->scale=scale;
            if (box.xmin != 0){
                drawOffset.x=-box.xmin;
            }
            if (box.ymin != 0){
                drawOffset.y=-box.ymin;
            }
            symbol->pivot_x=drawOffset.x;
            symbol->pivot_y=drawOffset.y;
            symbol->maxDist=scalef(position->maxDist);
            symbol->minDist=scalef(position->minDist);
            symbol->compute();
            ctx.reset(DrawingContext::create(symbol->width,symbol->height));
        }

        void draw(SymbolCache::GetColorFunction colorGet, const String &colorRef){
            if (operations.size() < 1){
                return;
            }
            Coord::PixelXy lastPoint;
            std::vector<Coord::PixelXy> polygon;
            bool hasPoint=false;
            int penWidth=1;
            DrawingContext::ColorAndAlpha color=DrawingContext::convertColor(255,255,255); //black
            bool inPolygon=false;
            for (auto it=operations.begin();it != operations.end();it++){
                if (it->op == "SP"){
                    //color code
                    String cName;
                    if (it->param.size() < 1){
                        LOG_DEBUG("invalid color ref %s",it->param);
                        continue;
                    }
                    for (size_t i=0;i< colorRef.size();i+=6){
                        if (colorRef[i]==it->param[0]){
                            cName=colorRef.substr(i+1,5);
                            break;
                        }
                    }
                    if (cName.empty()){
                        LOG_DEBUG("color ref %s not found in %s",it->param,colorRef);
                        continue;
                    }
                    color=colorGet(cName);
                    continue;
                }
                if (it->op == "SW"){
                    penWidth=atoi(it->param.c_str());
                    if (penWidth < 1) penWidth=1;
                    continue;
                }
                if (it->op == "ST"){
                    //transparency
                    int transIndex=atoi(it->param.c_str());
                    int transparency = (4 - transIndex) * 64;
                    transparency = std::min(transparency, 255);
                    transparency = std::max(0, transparency);
                    color=DrawingContext::setAlpha(color,transparency);
                    continue;
                }
                if (it->op == "PU"){
                    if (it->hasPoint){
                        lastPoint=it->point.getShifted(drawOffset);
                        hasPoint=true;
                    }
                    continue;
                }
                if (it->op == "PD"){
                    if (it->hasPoint){
                        if (inPolygon){
                            polygon.push_back(it->point.getShifted(drawOffset));
                        }
                        else
                        {
                            Coord::PixelXy next = it->point.getShifted(drawOffset);
                            if (hasPoint)
                            {
                                if (penWidth > 1)
                                {
                                    ctx->drawThickLine(lastPoint, next, color, true, nullptr, penWidth);
                                }
                                else
                                {
                                    ctx->drawLine(lastPoint, next, color, true, nullptr);
                                }
                            }
                            lastPoint = next;
                            hasPoint = true;
                        }
                    }
                    continue;
                }
                if (it->op == "CI"){
                    if (hasPoint){
                        int radius=scalef(atoi(it->param.c_str()));
                        if (radius >= 1){
                            ctx->drawArc(lastPoint,color,radius,inPolygon?0:-1);
                        }
                    }
                }
                if (it->op == "PM"){
                    if (it->param == "0"){
                        inPolygon=true;
                        polygon.clear();
                        if (hasPoint){
                            polygon.push_back(lastPoint);
                        }
                    }
                    else{
                        inPolygon=false;
                    }
                    continue;
                }
                if (it->op == "FP"){
                    //currently we simplify and assume convex polygons
                    if (polygon.size() == 2){
                        ctx->drawLine(polygon[0],polygon[1],color,true);
                    }
                    if (polygon.size() >= 3){
                        Coord::PixelXy start=polygon[0];
                        for (int i=2;i<polygon.size();i++){
                            ctx->drawTriangle(start,polygon[i-1],polygon[i],color);
                        }
                    }
                    polygon.clear();
                    continue;
                }
            }
            symbol->createBuffer();
            memcpy(symbol->getBufferPointer(),ctx->getBuffer(),symbol->buffer->size()*sizeof(DrawingContext::ColorAndAlpha));
        }

    };


    SymbolPtr SymbolCache::getSymbol(const String &name,SymbolCache::GetColorFunction colorGet, int rotation,double scale){
        SymbolBase::Ptr rt;
        {
            Synchronized locker(lock);
            BaseMap::const_iterator it;
            size_t p = name.find(",");
            if (p != String::npos)
            {
                it = baseMap.find(name.substr(0, p));
            }
            else
            {
                it = baseMap.find(name);
            }
            if (it == baseMap.end())
                return SymbolPtr();
            rt=it->second;    
        }
        SymbolBase::CreateParam param;
        param.scaleTolerance=scaleTolerance;
        param.rotationTolerance=rotationTolerance;
        uint64_t addedBytes=0;
        SymbolPtr prt=rt->getOrCreate(colorGet, addedBytes, param,rotation,scale);
        if (addedBytes) {
            memUsage+=addedBytes;
            symbolEntries+=1;
        }
        return prt;
    }
    bool checkTolerance(double expected,double found, double tolerance){
        if (expected < 0) return true;
        if (std::fabs(expected-found) < tolerance) return true;
        return false;
    }
    int getRotation(int wanted, int tolerance){
        wanted=wanted%360;
        if (wanted < 0) wanted=360+wanted;
        int r=wanted%tolerance;
        return wanted-r;
    }

    SymbolPtr SymbolCache::SymbolBase::create(SymbolCache::GetColorFunction colorGet, const CreateParam &param, int rotation,double scale){
        if (!baseSymbol->buffer) return SymbolPtr();
        SymbolPtr ns=baseSymbol;
        if (!checkTolerance(scale,ns->scale,param.scaleTolerance)){
            double cscale=scale/ns->scale;
            LOG_DEBUG("scale raster symbol %s to %f (cscale=%f)",baseSymbol->name,scale,cscale);
            if (cscale < 0.0001){
                LOG_ERROR("invalid scale request for %s: %f",baseSymbol->name,cscale);
                return SymbolPtr();
            }
            ns=resizeSymbol(baseSymbol,cscale);
        }
        if (ns->rotation != rotation){
            LOG_DEBUG("rotating raster symbol %s to %d",baseSymbol->name,rotation);
            ns=rotateSymbol(ns,rotation);
        }
        return ns;
    }

    SymbolPtr SymbolCache::SymbolBase::getOrCreate(SymbolCache::GetColorFunction colorGet, uint64_t &addedBytes, const SymbolCache::SymbolBase::CreateParam &param, int rotation,double scale){
        Synchronized locker(lock);
        addedBytes=0;
        //it seems that line symbols have a "native" rotation of 90°
        if (StringHelper::startsWith(baseSymbol->name,LS_PREFIX)){
            rotation-=90;
        }
        int requestedRot=getRotation(rotation,param.rotationTolerance);
        if (baseSymbol-> buffer && baseSymbol->rotation == requestedRot){
            if (checkTolerance(scale,baseSymbol->scale,param.scaleTolerance)){
                return baseSymbol;
            }
        }
        for (auto it=otherSymbols.begin();it != otherSymbols.end();it++){
            if ((*it)->rotation == requestedRot ){
                if (checkTolerance(scale,(*it)->scale,param.scaleTolerance)){
                    return *it;
                }
            }
        }
        if (scale < 0) scale=baseSymbol->scale; //use the default scale
        SymbolPtr ns=create(colorGet,param,requestedRot,scale);
        if (! ns) return ns;
        size_t oc=otherSymbols.capacity();
        otherSymbols.push_back(ns);
        addedBytes=ns->numBytes()+(otherSymbols.capacity()-oc)*sizeof(SymbolPtr);
        return ns;
    }

    SymbolPtr SymbolCache::SymbolBaseVector::create(SymbolCache::GetColorFunction colorGet,const SymbolCache::SymbolBase::CreateParam &param, int rotation,double scale){
        std::unique_ptr<HPGLRender> renderer;
        if (StringHelper::startsWith(baseSymbol->name,LS_PREFIX)|| StringHelper::startsWith(baseSymbol->name,PT_PREFIX)){
            renderer.reset(new HPGLRender(baseSymbol,rotation,scale,HPGLRender::M_LS));
        }
        if (renderer){
            LOG_DEBUG("create vector symbol %s, rot=%d, scale=%f",baseSymbol->name,rotation,scale);
            renderer->parse(hpgl);
            renderer->computeBox();
            renderer->draw(colorGet,colorRef);
            if (colorRef.size()>=6){
                renderer->symbol->defaultColor=colorGet(colorRef.substr(1,5));
            }
            return renderer->symbol;
        }
        LOG_DEBUG("unknown vector symbol %s, cannot render",baseSymbol->name);
        return SymbolPtr();
    }

    String SymbolCache::checkSymbol(const String &name){
        BaseMap::const_iterator it;
        {
            Synchronized locker(lock);
            size_t p = name.find(",");
            if (p != String::npos)
            {
                it = baseMap.find(name.substr(0, p));
            }
            else
            {
                it = baseMap.find(name);
            }
            if (it == baseMap.end())
                return String();
            return it->second->baseSymbol->name;
        }
    }
    template<class T>
    static void fillBaseParam(SymbolData *data,const T &position){
        data->height = position.bnbox_h;
        data->width = position.bnbox_w;
        data->minDist = position.minDist;
        data->maxDist = position.maxDist;
        data->pivot_x = position.pivot_x;
        data->pivot_y = position.pivot_y; 
        data->stagger = position.stagger;   
    }
    
    bool SymbolCache::fillRasterSymbol(const String &name,const SymbolPosition &position, PngReader *reader, double scale){
        if (scale < 0) return false;
        int xmin = position.glx;
        int xmax = position.glx + position.bnbox_w;
        int ymin = position.gly;
        int ymax = position.gly + position.bnbox_h;
        if (xmin < 0 || xmax > reader->width || ymin < 0 || ymax > reader->height)
        {
            return false;
        }
        SymbolBase::Ptr base=std::make_shared<SymbolBase>();
        int scaledW=scalef(position.bnbox_w,scale);
        int scaledH=scalef(position.bnbox_h,scale);
        SymbolData *data = base->baseSymbol.get();
        data->name = name;
        // no scale - the base Symbol has an initial scale of 1
        fillBaseParam(data,position);
        data->x = position.bnbox_x;
        data->y = position.bnbox_y;
        data->compute();
        DrawingContext::ColorAndAlpha *readerBuffer = reader->buffer + ymin * reader->width + xmin;
        base->baseSymbol = resizeSymbol(base->baseSymbol, scale, readerBuffer, reader->width);
        {
            Synchronized locker(lock);
            uint64_t removedBytes=0;
            BaseMap::const_iterator it;
            if ((it=baseMap.find(name)) != baseMap.end()){
                LOG_ERROR("raster symbol %s already in cache",name);
                removedBytes=it->second->numBytes();
            }
            baseMap[name]=base;
            //rough estimate - ignoring mem usage of base map
            memUsage-=removedBytes;
            memUsage+=base->numBytes();
        }
        return true;
    }
    bool SymbolCache::fillVectorSymbol(const String &name,const VectorSymbol &position,double scale){
        SymbolBaseVector::Ptr base=std::make_shared<SymbolBaseVector>();
        SymbolData *data = base->baseSymbol.get();
        data->name = name;
        fillBaseParam(data,position);
        data->scale=scale; //default scale
        data->x=position.glx;
        data->y=position.gly;
        base->hpgl=position.hpgl;
        base->colorRef=position.colorRef;
        {
            Synchronized locker(lock);
            uint64_t removedBytes=0;
            BaseMap::const_iterator it;
            if ((it=baseMap.find(name)) != baseMap.end()){
                LOG_ERROR("vector symbol %s already in cache",name);
                removedBytes=it->second->numBytes();
            }
            baseMap[name]=base;
            //rough estimate - ignoring mem usage of base map
            memUsage-=removedBytes;
            memUsage+=base->numBytes();
        }
        return true;

    }
    SymbolCache::SymbolCache(double scaleT, int rotationT):
        scaleTolerance(scaleT),
        rotationTolerance(rotationT){
    }
    void SymbolCache::ToJson(StatusStream &stream){
        Synchronized locker(lock);
        stream["baseSymbols"]=baseMap.size();
        stream["derivedSymbols"]=symbolEntries.load();
        stream["memkb"]=memUsage.load()/1000;
    };
}
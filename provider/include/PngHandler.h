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
#ifndef _PNGENCODER_H
#define _PNGENCODER_H
#include "Types.h"
#include "DrawingContext.h"
#include <memory>
class PngEncoder{
    protected:
        DrawingContext *context;
    public:
    PngEncoder(){
        this->context=NULL;
    }
    void setContext(DrawingContext *context){
        this->context=context;
    }
    virtual ~PngEncoder(){
       
    }
    virtual bool encode( DataPtr out)=0;
    static PngEncoder * createEncoder(String name);
};
class PngReader{
        public:
        DrawingContext::ColorAndAlpha *buffer=nullptr;
        uint32_t width=0;
        uint32_t height=0;
        bool readOk=false;
        virtual bool read(const String &fileName)=0;
        virtual ~PngReader(){}
        static PngReader* createReader(String name);
        protected:
        PngReader(){
        }
};
#endif
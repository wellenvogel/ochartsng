/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  S52 Text parsing
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
#ifndef _S52TEXTPARSER_H
#define _S52TEXTPARSER_H
#include "S52Data.h"
#include "StringHelper.h"
#include "Logger.h"
#include "Coordinates.h"
namespace s52
{
    class StringOptions{
        public:
        int fontSize=16;
        int hjust=0;
        int vjust=0;
        int space=0;
        uint8_t style=0;
        uint8_t weight=1;
        uint8_t width=1;
        int xoffs=0;
        int yoffs=0;
        RGBColor color;
        int grp=0; //show important only for 0...20
    };

    class S52TextParser{
        public:
        static StringOptions parseStringOptions(const S52Data* s52data,const StringVector &parts,int start=2);
        static DisplayString parseTE(ocalloc::PoolRef pool,const S52Data *s52data, const String &param, const StringOptions &, const Attribute::Map *attributes);
        static DisplayString parseTX(ocalloc::PoolRef pool,const S52Data *s52data, const String &param, const StringOptions &, const Attribute::Map *attributes);
    };
}
#endif
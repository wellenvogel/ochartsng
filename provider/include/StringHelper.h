/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  String Helper
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

#ifndef STRINGHELPER_H
#define STRINGHELPER_H

#include <vector>
#include <iostream>
#include <sstream>
#include <utility>
#include <locale>
#include <codecvt>
#include <string.h>
#include "Types.h"


#define FMT StringHelper::format

class StringHelper{
public:
    static void replaceInline(String &line, const String &search, const String &replace);
    static String safeJsonString(String in);
    static String SanitizeString(String input);
    template<typename ...Args>
    static String format(const char *format,Args&& ...args){
        thread_local std::stringstream stream;
        stream.clear();
        stream.str(String());
        StreamFormat(stream,format,std::forward<Args>(args)...);
        return stream.str();
    }
    //TODO: correctly handle html
    static String safeHtmlString(const String &in);
    static StringVector split(const String &in, const String &sep, int max=0);
    static String toUpper(const String&in, bool handleUtf8=false);
    static void toUpperI(String&in); //only for ASCII right now
    static String toLower(const String&in, bool handleUtf8=false);
    static void toLowerI(String&in); //only for ASCII right now
    static String unescapeUri(const String &in);
    static String rtrim(const String &in);
    static void rtrimI(String &in);
    static void rtrimI(String &in,unsigned char v);
    static void trimI(String &in, unsigned char v);
    static String trim(const String &in,unsigned char v);
    static String beforeFirst(const String &in, const String &search, bool fullNotFound=false);
    static String afterLast(const String &in, const String &search, bool fullNotFound=false);
    static bool endsWith(const String &in, const String &search);
    static bool endsWith(const String &in, const char *search);
    static bool startsWith(const String &in, const String &search);
    static bool startsWith(const String &in, const char *search);
    static String urlEncode(const String &data);
    static char * cloneData(const String &value);
    static String concat(const StringVector &data,const char *delim=" ");
    static String concat(const char **data,int len,const char *delim=" ");
    static String concat(char **data,int len,const char *delim=" "){
        return concat((const char **)data,len,delim );
    }
    template<typename StringType>
    static std::u32string toUnicode(const StringType &v){
        return std::wstring_convert<std::codecvt_utf8<char32_t>,char32_t>().from_bytes(v.data());
    }
    static void StreamFormat(std::ostream &stream,const char *fmt){
        stream << fmt;
    }
    template<typename T, typename ...Args>
    static void StreamFormat(std::ostream &stream,const char *fmt, T&& p1,Args&& ...args){
        const char *start=fmt;
        const char *p=fmt;
        while (*p!= 0){
            if (*p == '%'){
                if (*(p+1) == '%'){
                    p++; //write one % to stream
                    if (p != start ){
                        stream.write(start,p-start);
                    }
                    p++; //point after second %
                    start=p;
                    continue;
                }
                if (p != start ){
                    stream.write(start,p-start);
                }
                StreamSave old=HandleFormatValue(stream,&p);
                stream << std::forward<T>(p1);
                old.set(stream);
                StreamFormat(stream,p,std::forward<Args>(args)...);
                return;
            }
            else{
                p++;
            }
        }
        //we did not find any %
        stream.write(fmt,p-fmt);
    }
    typedef std::function<void(std::ostream &, const String &, const char &)> ArgCallback;
    static void DefaultArgCallback(std::ostream &stream,const String &v, const char type);
    static int StreamArrayFormat(std::ostream &stream,const String & fmt,const StringVector &args, ArgCallback callback=ArgCallback());

    private:
    class StreamSave{
        public:
        std::ios_base::fmtflags flags;
        char fill;
        int width;
        char formatType=0;
        std::streamsize precision;
        bool mustSet=false;  
        void set(std::ostream &stream){
            if (! mustSet) return;
            stream.flags(flags);
            stream.fill(fill);
            stream.precision(precision);
            stream.width(width);
        }
        void get(std::ostream &stream){
            flags = stream.flags();
            width = stream.width();
            fill = stream.fill();
            precision = stream.precision();
        }
    };
    static StreamSave HandleFormatValue(std::ostream &stream, const char **fmt);
};

#define PF_BOOL(val) (val?"true":"false")
#define JSONOK(obj) obj["status"]="OK"

#endif /* STRINGHELPER_H */


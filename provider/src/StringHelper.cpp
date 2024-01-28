/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  String Helper
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2020 by Andreas Vogel   *
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
#include "StringHelper.h"
#include <algorithm>
#include <cctype>
#include <locale>
#include <codecvt>
#include <regex>
#include <sstream>
#include <iostream>
#include <stdarg.h>
#include <libgen.h>
#include <curl/curl.h>

#define PATH_SEP "/"

void StringHelper::replaceInline(String &line, const String &search, const String &replace)
{
    const size_t oldSize = search.length();
    if (oldSize == 0)
        return;
    // do nothing if line is shorter than the string to find
    if (oldSize > line.length())
        return;

    const size_t newSize = replace.length();
    for (size_t pos = 0;; pos += newSize)
    {
        // Locate the substring to replace
        pos = line.find(search, pos);
        if (pos == String::npos)
            return;
        if (oldSize == newSize)
        {
            // if they're same size, use std::string::replace
            line.replace(pos, oldSize, replace);
        }
        else
        {
            // if not same size, replace by erasing and inserting
            line.erase(pos, oldSize);
            line.insert(pos, replace);
        }
    }
}
String StringHelper::safeJsonString(String in)
{
    String rt(in);
    replaceInline(rt, "\"", "\\\"");
    replaceInline(rt, "\n", " ");
    replaceInline(rt, "\r", " ");
    return rt;
}
String StringHelper::SanitizeString(String input)
{
    std::regex allowed("[^a-zA-Z0-9.,_\\-]");
    String rt = std::regex_replace(input, allowed, "_");
    return rt;
}

// TODO: correctly handle html
String StringHelper::safeHtmlString(const String &in)
{
    String out(in);
    replaceInline(out, "<", "&lt;");
    replaceInline(out, ">", "&gt;");
    replaceInline(out, "\"", "&quot;");
    replaceInline(out, "'", "&#39;");
    return out;
}

StringVector StringHelper::split(const String &in, const String &sep, int max)
{
    StringVector rt;
    size_t pos = 0;
    size_t slen = sep.length();
    if (!slen)
        return rt;
    size_t found;
    int num=0;
    while ((found = in.find(sep, pos)) != String::npos)
    {
        rt.push_back(in.substr(pos, found - pos));
        pos = found + slen;
        num++;
        if (max != 0 && num >= max){
            break;
        }
    }
    if (pos < in.length())
    {
        rt.push_back(in.substr(pos));
    }
    return rt;
}

void StringHelper::toUpperI(String&in){
    std::transform(in.begin(),in.end(),in.begin(),[](auto c){return std::toupper(c);});
}
String StringHelper::toUpper(const String&in, bool handleUtf8){
    while (handleUtf8){
        std::wstring_convert<std::codecvt_utf8<wchar_t>,wchar_t> convert;
        std::wstring uc=convert.from_bytes(in);
        try{
            std::locale loc=std::locale("en_US.UTF8");
            bool hasFacet=std::has_facet<std::ctype<wchar_t>>(loc);
            if (! hasFacet) break; //fallback
            auto& f = std::use_facet<std::ctype<wchar_t>>(loc);
            f.toupper(&uc[0],&uc[0]+uc.size());
            return convert.to_bytes(uc);
        }catch (std::exception &e){
            //fallback
            break;
        }
    }
    String out(in);
    StringHelper::toUpperI(out);
    return out;
}
void StringHelper::toLowerI(String& out){
    std::transform(out.begin(),out.end(),out.begin(),[](auto c){return std::tolower(c);});
}

String StringHelper::toLower(const String&in, bool handleUtf8){
    while (handleUtf8){
        std::wstring_convert<std::codecvt_utf8<wchar_t>,wchar_t> convert;
        std::wstring uc=convert.from_bytes(in);
        try{
            std::locale loc=std::locale("en_US.UTF8");
            bool hasFacet=std::has_facet<std::ctype<wchar_t>>(loc);
            if (! hasFacet) break; //fallback
            auto& f = std::use_facet<std::ctype<wchar_t>>(loc);
            f.tolower(&uc[0],&uc[0]+uc.size());
            return convert.to_bytes(uc);
        }catch (std::exception &e){
            //fallback
            break;
        }
    }

    String out(in);
    StringHelper::toLowerI(out);
    return out;

}
String StringHelper::unescapeUri(const String &value){
    //from https://github.com/d2school/da4qi4
    String result;
    result.reserve(value.size());
    
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        auto ch = value[i];
        
        if (ch == '%' && (i + 2) < value.size())
        {
            auto hex = value.substr(i + 1, 2);
            auto dec = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            result.push_back(dec);
            i += 2;
        }
        else if (ch == '+')
        {
            result.push_back(' ');
        }
        else
        {
            result.push_back(ch);
        }
    }   
    return result;
}
void StringHelper::rtrimI(String &out){
    out.erase(std::find_if(out.rbegin(), out.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), out.end());
}
String StringHelper::rtrim(const String &in){
    String out(in);
    StringHelper::rtrimI(out);
    return out;
}
void StringHelper::rtrimI(String &out, unsigned char v){
    auto rit=out.end()-1;
    while (*rit == v && rit >= out.begin()) rit--;
    out.erase(rit+1,out.end());  
}
void StringHelper::trimI(String &out, unsigned char v){
    auto rit=out.end()-1;
    while (*rit == v && rit >= out.begin()) rit--;
    out.erase(rit+1,out.end());
    auto lit=out.begin();
    while (*lit == v && lit != out.end()) lit++;
    out.erase(out.begin(),lit);     

}
String StringHelper::trim(const String &in,unsigned char v){
    String out(in);
    trimI(out,v);
    return out;
}
String StringHelper::beforeFirst(const String &in, const String &search, bool fullNotFound){
    size_t found;
    if ((found = in.find(search,0)) != String::npos){
        return in.substr(0,found);
    }
    return fullNotFound?in:String();
}
String StringHelper::afterLast(const String &in, const String &search, bool fullNotFound){
    size_t found;
    if ((found = in.find_last_of(search)) != String::npos){
        if (found < (in.size()-1)) return in.substr(found+1);
    }
    return fullNotFound?in:String();
}
bool StringHelper::endsWith(const String &in, const String &search){
    size_t len=search.length();
    if (len > in.length()) return false;
    return in.compare(in.length()-len,len,search) == 0;
}
bool StringHelper::endsWith(const String &in, const char *search){
    if (search == NULL) return false;
    size_t len=strlen(search);
    if (len > in.length()) return false;
    return in.compare(in.length()-len,len,search) == 0;
}
bool StringHelper::startsWith(const String &in, const String &search){
    size_t len=search.length();
    if (len > in.length()) return false;
    return in.compare(0,len,search) == 0;
}
bool StringHelper::startsWith(const String &in, const char *search){
    if (search == NULL) return false;
    size_t len=strlen(search);
    if (len > in.length()) return false;
    return in.compare(0,len,search) == 0;
}

String StringHelper::urlEncode(const String & data){
    char *encoded=curl_easy_escape(NULL,data.c_str(),0);
    String rt(encoded);
    curl_free(encoded);
    return rt;
}
char * StringHelper::cloneData(const String &value){
    char *rt=new char[value.size()+1];
    strcpy(rt,value.c_str());
    return rt;
}
String StringHelper::concat(const StringVector &data,const char *delim){
    std::stringstream out;
    for (auto it=data.begin();it != data.end();it++){
        out << *it;
        if (it < (data.end()-1)) out << delim;
    }
    return out.str();
}
String StringHelper::concat(const char **data,int len,const char *delim){
    std::stringstream out;
    for (int i=0;i<len;i++){
        if (data[i]){
            out << data[i];
            if (i < (len-1)){
                out  << delim;
            }
        }
    }
    return out.str();
}
/*
std::u32string StringHelper::toUnicode(const String &v){
    return std::wstring_convert<std::codecvt_utf8<char32_t>,char32_t>().from_bytes(v.data());
}
std::u32string StringHelper::toUnicode(const String &v){
    return std::wstring_convert<std::codecvt_utf8<char32_t>,char32_t>().from_bytes(v.data());
}
*/

int StringHelper::StreamArrayFormat(std::ostream &stream, const String & fmt,const StringVector &args, ArgCallback callback)
{
    if (fmt.size() == 0)
    {
        stream << fmt;
        return 0;
    }
    const char *start = fmt.c_str();
    const char *p = start;
    int argIdx = 0;
    while (*p != 0 && argIdx < args.size())
    {
        if (*p == '%')
        {
            if (*(p + 1) == '%')
            {
                p++; // write one % to stream
                if (p != start)
                {
                    stream.write(start, p - start);
                }
                p++; // point after second %
                start=p;
                continue;
            }
            if (p != start)
            {
                stream.write(start, p - start);
                start = p;
            }
            StreamSave old = HandleFormatValue(stream, &p);
            //p points after the format string
            if (callback)
            {
                callback(stream, args[argIdx], old.formatType);
            }
            else
            {
                stream << args[argIdx];
            }
            old.set(stream);
            argIdx++;
            start = p;
        }
        else{
            p++;
        }
    }
    while (*p != 0) p++;
    if (p > start){
        stream.write(start, p - start);
    }
    return argIdx;
}

StringHelper::StreamSave StringHelper::HandleFormatValue(std::ostream &stream, const char **fmt)
{
    StreamSave rt;
    if (**fmt != '%') return rt;
    /* convert  printf and iso format */
    int width = stream.width();;
    int precision = stream.precision();
    std::ios_base::fmtflags flags=stream.flags();
    flags &= ~(std::ios::adjustfield | std::ios::basefield | std::ios::floatfield);
    char fill = stream.fill();
    bool alternate = false;
    (*fmt)++;
    if (**fmt != 0)
    {

        /* these flags can run through multiple characters */
        bool more = true;

        while (more)
        {
            switch (**fmt)
            {
            case '+':
                flags |= std::ios::showpos;
                break;
            case '-':
                flags |= std::ios::left;
                break;
            case '0':
                flags |= std::ios::internal;
                fill = '0';
                break;
            case '#':
                alternate = true;
                break;
            default:
                more = false;
                break;
            }
            if (more)
                (*fmt)++;
        }

        /* width specifier */
        if (isdigit(**fmt))
        {
            width = atoi(*fmt);
            do
            {
                (*fmt)++;
            } while (isdigit(**fmt));
        }

        /* output precision */
        if (**fmt == '.')
        {
            (*fmt)++;
            precision = atoi(*fmt);
            do
            {
                (*fmt)++;
            } while (isdigit(**fmt));
        }

        /* type based settings */
        bool format_ok = true;
        //skip any ld, lld
        while (**fmt == 'l') (*fmt)++;
        rt.formatType=**fmt;
        switch (**fmt)
        {
        case 'p':
            break;
        case 's':
            break;
        case 'c':
        case 'C':
            break;
        case 'i':
        case 'u':
        case 'd':
            flags |= std::ios::dec;
            break;
        case 'x':
            flags |= std::ios::hex;
            if (alternate)
                flags |= std::ios::showbase;
            break;
        case 'X':
            flags |= std::ios::hex | std::ios::uppercase;
            if (alternate)
                flags |= std::ios::showbase;
            break;
        case 'o':
            flags |= std::ios::hex;
            if (alternate)
                flags |= std::ios::showbase;
            break;
        case 'f':
            flags |= std::ios::fixed;
            if (alternate)
                flags |= std::ios::showpoint;
            break;
        case 'e':
            flags |= std::ios::scientific;
            if (alternate)
                flags |= std::ios::showpoint;
            break;
        case 'E':
            flags |= std::ios::scientific | std::ios::uppercase;
            if (alternate)
                flags |= std::ios::showpoint;
            break;
        case 'g':
            if (alternate)
                flags |= std::ios::showpoint;
            break;
        case 'G':
            flags |= std::ios::uppercase;
            if (alternate)
                flags |= std::ios::showpoint;
            break;
        default:
            /* if we encountered an unknown type specifier do not set
               any options but simply "eat" the specifier  */
            format_ok = false;
            break;
        }
        /* if formatting string was recognized set stream options*/
        if (format_ok)
        {
            rt.get(stream);
            rt.mustSet=true;
            stream.flags(flags);
            stream.width(width);
            stream.precision(precision);
            stream.fill(fill);
        }

        /* skip type specifier and set formatting string position */
        (*fmt)++;
    }
    return rt;
}
void StringHelper::DefaultArgCallback(std::ostream &stream,const String &v, const char type){
    switch (type){
        case 'I':
        case 'i':
        case 'x':
        case 'X':
        case 'u':
        case 'd':
        case 'o':
            stream << ::atoll(v.c_str());
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
            stream << ::atof(v.c_str());
            break;
        default:
            stream << v;    
    };
}
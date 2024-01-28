/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  MD5
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

#ifndef MD5_H
#define MD5_H
#include "Types.h"
#include <openssl/evp.h>
#include <memory.h>
#include <string_view>

#define MD5_ADD_VALUE(md5,value) md5.AddBuffer((const unsigned char*)(&value),sizeof(value))
#define MD5_ADD_VALUEP(md5,value) md5->AddBuffer((const unsigned char*)(&value),sizeof(value))
#define MD5_LEN 16

class MD5Name{
private:
    unsigned char name[MD5_LEN];
public:
    const int len=MD5_LEN;
    void fromChar(const unsigned char *src =NULL){
        memset(name,0,MD5_LEN);
        if (src){
            size_t len=(int)strnlen((const char *)src,MD5_LEN);
            memcpy(name,src,len);
        }
    }
    MD5Name(const unsigned char *src =NULL){
        fromChar(src);    
    }
    MD5Name(const MD5Name &other){
        memcpy(name,other.name,MD5_LEN);
    }
    MD5Name& operator=(const MD5Name &other){
        if (this != &other){
            memcpy(name,other.name,MD5_LEN);
        }
        return *this;
    }
    bool operator==(const MD5Name &other) const{
        return memcmp(name,other.name,MD5_LEN) == 0;
    }
    bool operator!=(const MD5Name &other) const{
        return memcmp(name,other.name,MD5_LEN) != 0;
    }
    bool operator<(const MD5Name &other) const{
        return memcmp(name,other.name,MD5_LEN) < 0;
    }    
    bool operator>(const MD5Name &other) const{
        return memcmp(name,other.name,MD5_LEN) > 0;
    }
    const unsigned char *GetValue() const{
        return name;
    }
    std::string_view toStringView() const{
        return std::string_view((const char *)name,len);
    }
    size_t hash() const{
        return std::hash<std::string_view>{}(toStringView());
    }
    String ToString() const;
    
};

class MD5 {
public:
    MD5();
    MD5(const MD5&);
    MD5& operator=(const MD5&);
    virtual ~MD5();
    bool AddBuffer(const unsigned char *buffer, long len);
    bool AddBuffer(const char *buffer,long len){
        return AddBuffer((const unsigned char *)buffer,len);
    }
    template<typename T>
    bool AddValue(const T &v){
        return AddBuffer((const unsigned char *)(&v),sizeof(T));
    }
    bool AddValue(const String &s);
    bool AddValue(const char *s);
    bool AddFileInfo(const String &path,const String &base=String());
    bool IsOk() const;
    bool IsFinalized() const{ return finalized;}
    const unsigned char * GetValue();
    MD5Name GetValueCopy();
    String GetHex();
    static MD5Name Compute(String v);
private:
    EVP_MD_CTX *mdctx;
    unsigned char *resultBuffer;
    bool finalized;

};

#endif /* MD5_H */


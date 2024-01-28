/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  MD5
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


#include "MD5.h"
#include "Logger.h"
#include <memory.h>
#include "FileHelper.h"

MD5::MD5() {
   mdctx = EVP_MD_CTX_new();
   if(1 != EVP_DigestInit_ex(mdctx, EVP_md5(), NULL)){
        LOG_DEBUG("error init md context");
        EVP_MD_CTX_free(mdctx);
        mdctx=NULL;
    }
   resultBuffer=NULL;
   finalized=false;
}

MD5::MD5(const MD5& other){
    //make a copy of the current state
    finalized=other.finalized;
    mdctx=NULL;
    resultBuffer=NULL;
    if (finalized){
        resultBuffer=new unsigned char[MD5_LEN];
        memcpy(resultBuffer,other.resultBuffer,MD5_LEN);
        return;
    }
    if (!other.IsOk()) return;
    mdctx=EVP_MD_CTX_new();
    if (1 != EVP_MD_CTX_copy(mdctx,other.mdctx)){
        LOG_DEBUG("copying MD5 failed");
        EVP_MD_CTX_free(mdctx);
        mdctx=NULL;
    }
}
MD5& MD5::operator =(const MD5& other){
    finalized=other.finalized;
    mdctx=NULL;
    resultBuffer=NULL;
    if (finalized){
        resultBuffer=new unsigned char[MD5_LEN];
        memcpy(resultBuffer,other.resultBuffer,MD5_LEN);
        return *this;
    }
    if (!other.IsOk()) return *this;
    mdctx=EVP_MD_CTX_new();
    if (1 != EVP_MD_CTX_copy(mdctx,other.mdctx)){
        LOG_DEBUG("copying MD5 failed");
        EVP_MD_CTX_free(mdctx);
        mdctx=NULL;
    }
    return *this;
}

bool MD5::IsOk() const {
    return mdctx != NULL;
}
bool MD5::AddBuffer(const unsigned char* buffer, long len){
    if (! IsOk() || finalized) return false;
    if (EVP_DigestUpdate(mdctx,buffer,len) != 1){
        LOG_DEBUG("md5 add long failed");
        EVP_MD_CTX_free(mdctx);
        mdctx=NULL;
    }
    return IsOk();
}


bool MD5::AddValue(const String &data) {
    return AddBuffer(data.c_str(),data.size());
}
bool MD5::AddValue(const char *data) {
    return AddBuffer((const unsigned char*)data,strlen(data));
}

bool MD5::AddFileInfo(const String &path, const String &base){
    if (! IsOk() || finalized) return false;
    String fileName;
    if (!base.empty()){
        fileName=FileHelper::concatPath(base,path);
    }
    else{
        fileName=path;
    }
    if (! FileHelper::exists(fileName)){
        LOG_DEBUG("file not found for MD5::AddFileInfo: %s",fileName.c_str());
        return true;
    }
    AddValue(fileName);
    auto fileTime=FileHelper::fileTime(fileName);
    MD5_ADD_VALUEP(this,fileTime);
    auto fileSize=FileHelper::fileSize(fileName);
    MD5_ADD_VALUEP(this,fileSize);
    return IsOk();
}

const unsigned char * MD5::GetValue(){
    if (finalized) return resultBuffer;
    if (! IsOk()) return NULL;
    if (resultBuffer == NULL) resultBuffer=new unsigned char[MD5_LEN];
    finalized=true;
    unsigned int len=16;
    if(1 != EVP_DigestFinal_ex(mdctx, resultBuffer, &len)){
        LOG_DEBUG("error md result");
        delete [] resultBuffer;
        resultBuffer=NULL;
    }
    return resultBuffer;
}
MD5Name MD5::GetValueCopy(){
    return MD5Name(GetValue());
}

static unsigned char ToHex(unsigned char v){
    v=v&0xf;
    if (v <=9) return '0'+v;
    return 'a'+v-10;
}

String MD5::GetHex(){
    const unsigned char *result=GetValue();
    if (result == NULL) return String();
    char res[2 * MD5_LEN + 1];
    for (int i=0;i<MD5_LEN;i++){
        res[2*i]=ToHex(result[i]>>4);
        res[2*i+1]=ToHex(result[i]);
    }
    res[2 * MD5_LEN]=0;
    return String(res);
}

MD5::~MD5() {
    if (mdctx != NULL) EVP_MD_CTX_free(mdctx);
    if (resultBuffer != NULL) delete [] resultBuffer;
}

MD5Name MD5::Compute(String v){
    MD5 md5;
    md5.AddValue(v);
    return md5.GetValueCopy();
}

String MD5Name::ToString() const{
    char res[2 * MD5_LEN + 1];
    for (int i=0;i<MD5_LEN;i++){
        res[2*i]=ToHex(name[i]>>4);
        res[2*i+1]=ToHex(name[i]);
    }
    res[2 * MD5_LEN]=0;
    return String(res);
}


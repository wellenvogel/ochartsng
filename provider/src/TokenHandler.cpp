/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Token Handler
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

#include "TokenHandler.h"
#include "Logger.h"
#include "SimpleThread.h"
#include <deque>
#include "Timer.h"
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <vector>
#include "MD5.h"
#include "publicKey.h"

//timeout in seconds for a client
#define CLIENT_TIMEOUT 300*1000
//token interval
#define TOKEN_INTERVAL 120*1000
//how man tokens we keep (multiplied with token interval this is the validity period)
#define TOKEN_LIST_LEN 4


//https://gist.github.com/irbull/08339ddcd5686f509e9826964b17bb59
static RSA* createPublicRSA(const char * key) {
  RSA *rsa = NULL;
  BIO *keybio;
  keybio = BIO_new_mem_buf((void*)key, -1);
  if (keybio==NULL) {
      return NULL;
  }
  rsa = PEM_read_bio_RSA_PUBKEY(keybio, &rsa,NULL, NULL);
  return rsa;
}


/**
 * encrypt our 128 bit key using rsa
 * @param key
 * @return 
 */
static String encryptKey(const unsigned char *key){
    RSA* rsa=createPublicRSA(PUBLICKEY);
    EVP_PKEY_CTX *ctx=NULL;
    ENGINE *eng=NULL;
    unsigned char *out=NULL;
    size_t outlen;
    EVP_PKEY *pubKey=EVP_PKEY_new();;
    EVP_PKEY_assign_RSA(pubKey, rsa);

 
    ctx = EVP_PKEY_CTX_new(pubKey, eng);
    if (!ctx){
        EVP_PKEY_free(pubKey);
        LOG_DEBUG("error creating encrypt ctx");
        return String();
    }
        
     /* Error occurred */
    if (EVP_PKEY_encrypt_init(ctx) <= 0){
        EVP_PKEY_free(pubKey);
        EVP_PKEY_CTX_free(ctx); 
        LOG_DEBUG("error creating encrypt ctx");
        return String();
    }
        
     /* Error */
    if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0){
        EVP_PKEY_free(pubKey);
        EVP_PKEY_CTX_free(ctx); 
        LOG_DEBUG("error setting padding");
        return String();
    }
     /* Error */

    /* Determine buffer length */
    if (EVP_PKEY_encrypt(ctx, NULL, &outlen, key, 16) <= 0){
        EVP_PKEY_free(pubKey);
        EVP_PKEY_CTX_free(ctx); 
        LOG_DEBUG("error getting crypted len");
        return String();
    }
    out = new unsigned char[outlen];

    if (!out){
        EVP_PKEY_free(pubKey);
        EVP_PKEY_CTX_free(ctx); 
        LOG_DEBUG("new no mem");
        return String();
    }
    if (EVP_PKEY_encrypt(ctx, out, &outlen,key, 16) <= 0){
        EVP_PKEY_free(pubKey);
        EVP_PKEY_CTX_free(ctx); 
        delete []out;
        LOG_DEBUG("encrypt error");
        return String();
    }
    EVP_PKEY_free(pubKey);
    EVP_PKEY_CTX_free(ctx); 
    char *hexBuffer=OPENSSL_buf2hexstr(out,outlen);
    delete []out;
    String rt(hexBuffer);
    OPENSSL_free(hexBuffer);
    return rt;
}
/**
 * 
 * @param hexInput encrypted input in hex
 * @param key 128 bit AES key
 * @return the decrypted string
 */
static String decryptAes(String hexInput, String hexIv, const unsigned char *key){
   long inputLen=0; 
   unsigned char *input=OPENSSL_hexstr2buf(hexInput.c_str(),&inputLen);
   if (input == NULL){
       LOG_DEBUG("unable to decode aes %s",hexInput);
       return String();
   }
   long outputLen=inputLen+10;
   long ivLen=0;
   unsigned char *iv=OPENSSL_hexstr2buf(hexIv.c_str(),&ivLen);
   if (iv == NULL){
       LOG_DEBUG("invalid iv %s",hexIv);
       OPENSSL_free(input);
       return String();
   }
   if (ivLen != 16){
       LOG_DEBUG("invalid iv len for %s",hexIv);
       OPENSSL_free(input);
       OPENSSL_free(iv);
       return String();
   }
   EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
   if (ctx == NULL){
       LOG_DEBUG("unable to create cipher ctx");
       OPENSSL_free(input);
       OPENSSL_free(iv);
       return String();
   }
   if (EVP_DecryptInit(ctx,EVP_aes_128_ctr(),key,iv) != 1){
       LOG_DEBUG("unable to init decryption");
       EVP_CIPHER_CTX_free(ctx);
       OPENSSL_free(input);
       OPENSSL_free(iv);
       return String();
   }
   unsigned char output[outputLen];
   int filledOutput=0;
   if (EVP_DecryptUpdate(ctx,output,&filledOutput,input,inputLen) != 1){
       LOG_DEBUG("unable to decrypt %s",hexInput);
       EVP_CIPHER_CTX_free(ctx);
       OPENSSL_free(input);
       OPENSSL_free(iv);
       return String();
   }
   int addFilled=0;
   if (EVP_DecryptFinal(ctx,output+filledOutput,&addFilled) != 1){
       LOG_DEBUG("unable to finalize decrypt %s",hexInput);
       EVP_CIPHER_CTX_free(ctx);
       OPENSSL_free(input);
       OPENSSL_free(iv);
       return String();
   }
   filledOutput+=addFilled;
   EVP_CIPHER_CTX_free(ctx);
   OPENSSL_free(input);
   OPENSSL_free(iv);
   output[filledOutput]=0;
   String rt((char *)output,filledOutput);
   LOG_DEBUG("decoded url %s",hexInput);
   return rt;
}

void Token::Encrypt(){
        encryptedKey=encryptKey(key.GetValue());
}

//TokenList::
Token::ConstPtr TokenList::NextToken()
{
    Token::Ptr rt;
    {
        Synchronized locker(lock);
        sequence++;
        MD5 md5;
        md5.AddValue(sessionId);
        md5.AddValue(Timer::systemMillis());
        rt.reset(new Token(md5.GetValueCopy(), sequence));
    }
    rt->Encrypt();
    return rt;
}
TokenList::TokenList(String sId) : sessionId(sId)
{
    LOG_DEBUG("creating new token list for %s", sessionId);
    lastAccess = Timer::systemMillis();
    lastToken = lastAccess;
    sequence = 0;
    tokens.push_back(NextToken());
}
bool TokenList::TimerAction()
{
    auto now = Timer::systemMillis();
    unsigned long long plastToken;
    {
        Synchronized locker(lock);
        plastToken=lastToken;
    }
    if ((now - plastToken) >= TOKEN_INTERVAL)
    {
        LOG_DEBUG("new token for %s", sessionId);
        auto next = NextToken();
        Synchronized locker(lock);
        tokens.push_back(next);
        if (tokens.size() > TOKEN_LIST_LEN)
        {
            tokens.pop_front();
        }
        lastToken = now;
        return true;
    }
    return false;
}
TokenResult TokenList::NewestToken()
{
    Synchronized locker(lock);
    lastAccess = Timer::systemMillis();
    TokenResult rt;
    rt.state = TokenResult::RES_OK;
    rt.sessionId = sessionId;
    rt.key = tokens.back()->encryptedKey;
    rt.sequence = tokens.back()->sequence;
    return rt;
}
Token::ConstPtr TokenList::findTokenBySequence(int sequence){
        Synchronized locker(lock);
        for (const auto &token:tokens){
            if (token->sequence == sequence){
                return token;
            }
        }
        return Token::ConstPtr();
    }

TokenHandler::TokenHandler(String name): Thread() {
    this->name=name;
}
String TokenHandler::ComputeMD5(String data, unsigned long long ts){
    MD5 md5;
    md5.AddValue(data);
    md5.AddValue(ts);
    return md5.GetHex();
}
String TokenHandler::GetNextSessionId(){
    return ComputeMD5(name, Timer::systemMillis());
}


TokenResult TokenHandler::NewToken(String sessionId){
    TokenList::Ptr list;
    {
        Synchronized locker(lock);
        TokenMap::iterator it=map.find(sessionId);
        if (it != map.end()){
            list=it->second;
        }
    }
    if (!list){
        return NewToken();
    }
    TokenResult rt=list->NewestToken();
    LOG_DEBUG("TokenHandler::NewToken(sessionId) %s",rt.ToString());
    return rt;
}

TokenResult TokenHandler::NewToken(){
    TokenResult rt;
    {
        Synchronized locker(lock);
        if (map.size() >= MAX_CLIENTS){
            rt.state=TokenResult::RES_TOO_MANY;
            return rt;
        }
    }
    String sessionId=GetNextSessionId();
    TokenList::Ptr list;
    {
        Synchronized locker(lock);
        list.reset(new TokenList(sessionId));
        map[sessionId]=list;
    }
    rt=list->NewestToken();
    return rt;
}
DecryptResult TokenHandler::DecryptUrl(String url){
    StringVector parts=StringHelper::split(url,"/",3); //gives us 4 parts
    if (parts.size() < 4){
        LOG_ERROR("not enough parts in url to decrypt: %s",url);
        return DecryptResult("not enough parts in url to decrypt");
    }
    String sessionId=parts[0];
    if (sessionId.empty()){
        LOG_ERROR("no session Id in encrypted URL: %s",url);
        return DecryptResult("no session Id in encrypted URL");
    }
    TokenList::Ptr list;
    {
        Synchronized locker(lock);
        TokenMap::iterator it=map.find(sessionId);
        if (it != map.end()){
            list=it->second;
        }
    }
    if (! list){
        LOG_DEBUG("DecryptUrl: session not found: %s",sessionId);
        return DecryptResult("DecryptUrl: session not found");
    }
    String sequenceStr=parts[1];
    String hexIv=parts[2];
    String cryptedUrl=parts[3];
    int sequence=-1;
    if (sscanf(sequenceStr.c_str(),"%d",&sequence) != 1){
        LOG_DEBUG("DecryptUrl: no sequence in encrypted url %s",url);
        return DecryptResult("DecryptUrl: no sequence in encrypted url");
    }
    if (hexIv.empty() || cryptedUrl.empty()){
        LOG_DEBUG("DecrpytUrl: invalid crypted url %s",url);
        return DecryptResult("DecrpytUrl: invalid crypted url");
    }
    Token::ConstPtr token=list->findTokenBySequence(sequence);
    if (! token){
        LOG_DEBUG("DecryptUrl: unable to find sequence %d for session %s",sequence,sessionId);
        return DecryptResult("DecryptUrl: unable to find sequence");
    }
    DecryptResult rt;
    rt.url=decryptAes(cryptedUrl,hexIv,token->key.GetValue());
    rt.sessionId=sessionId;
    return rt;
}

bool TokenHandler::TimerAction(){
    //step1: find outdated sessions and remove them
    auto now=Timer::systemMillis();
    auto toErase=now-CLIENT_TIMEOUT;
    TokenMap::iterator it;
    {
        Synchronized locker(lock);
        avnav::erase_if(map,[toErase](std::pair<const String ,TokenList::Ptr> &item){
            return item.second->lastAccess < toErase;
        });
    }
    for (auto &[key,val]:map){
        val->TimerAction();
    }
    return true;
}

void TokenHandler::run(){
    LOG_INFO("token handler %s timer started",name);
    while (! shouldStop()){
        //TODO: wait/notify for fast stop
        Timer::microSleep(1000*1000);
        TimerAction();
    }
    LOG_INFO("token handler %s timer finished",name);
}
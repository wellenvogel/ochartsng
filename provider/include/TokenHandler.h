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

#ifndef TOKENHANDLER_H
#define TOKENHANDLER_H
#include "Types.h"
#include "SimpleThread.h"
#include "StringHelper.h"
#include <map>
#include <memory>
#include "MD5.h"

#define MAX_CLIENTS 5

class Token {
public: 
    String encryptedKey;
    MD5Name key;
    int sequence;   
    Token(const MD5Name &key,int sequence){
        this->key=key;
        this->sequence=sequence;
    }
    void Encrypt();
    using Ptr=std::shared_ptr<Token>;
    using ConstPtr=std::shared_ptr<const Token>;
};

class TokenResult{
public:
    typedef enum{
        RES_OK,         //session id and key are set  
        RES_TOO_MANY,   //too many clients  
        RES_EMPTY        
    } ResultState;
    ResultState     state;
    String        sessionId;
    String        key;
    int             sequence;
    TokenResult(){
        state=RES_EMPTY;
        sequence=-1;
    }
    String ToString(){
        String mode="EMPTY";
        switch(state){
            case RES_OK:
                mode="OK";
                break;
            case RES_TOO_MANY:
                mode="TOO_MANY";
                break;
            default:
                mode="EMPTY";
                break;
        }
        return FMT("TokenResult: state=%s,session=%s,key=%s,sequence=%d",
                mode,sessionId,key,sequence);
    }
    
};
class DecryptResult{
public:
    String url;
    String sessionId;
    String error;
    DecryptResult(){}
    DecryptResult(const String &u,const String &sId):url(u),sessionId(sId){
    }
    DecryptResult(const String &e):error(e){}
    
};

class TokenList{
private:
    unsigned long long lastToken;
    std::deque<Token::ConstPtr> tokens;
    String sessionId;
    std::mutex lock;
    int sequence;
    Token::ConstPtr NextToken();
public: 
    unsigned long long lastAccess;
    TokenList(String sessionId);
    bool TimerAction();
    TokenResult NewestToken();
    Token::ConstPtr findTokenBySequence(int sequence);
    using Ptr=std::shared_ptr<TokenList>;
};



typedef std::map<String,TokenList::Ptr> TokenMap;
class TokenHandler : public Thread{
public:
    using Ptr=std::shared_ptr<TokenHandler>;
    TokenHandler(String name);
    DecryptResult       DecryptUrl(String url);
    TokenResult         NewToken(String sessionId);
    TokenResult         NewToken();
    bool                TimerAction();
    virtual void        run();
    static String       ComputeMD5(String data,unsigned long long ts=0);
private:
    String    GetNextSessionId();
    String    name;
    TokenMap    map;
    std::mutex  lock;
};

#endif /* TOKENHANDLER_H */


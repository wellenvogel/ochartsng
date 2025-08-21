/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  oexserverd controller
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
#ifndef _EXCEPTION_H
#define _EXCEPTION_H
#include <exception>
#include <sstream>
#include <typeinfo>
#include "stdarg.h"
#include "Types.h"
#include "StringHelper.h"

typedef std::exception Exception;
#define DECL_EXC(Base,Name) class Name: public Base{ \
            using Base::Base; \
            public:\
                virtual const char *type()const noexcept {return #Name;}\
            };
class AvException : public std::exception{
    public:
        AvException(const String &reason,int error=0){
            this->reason=reason;
            this->error=error;
        }
        AvException(){}
        virtual const char * what() const noexcept override{
            return reason.c_str();
        }
        virtual const String msg() const noexcept{
            std::stringstream out;
            out << type();
            if (error != 0){
                out << "(" << error << "):";
            }
            else{
                out << ":";
            }
            out << reason;
            return out.str();
        }
        virtual int getError() const{
            return error;
        }
        virtual void setError(int error){
            this->error=error;
        }
        virtual ~AvException(){}
        virtual const char *type() const noexcept {return "AvException";}
    protected:
        String reason;
        String computed;
        int error=0;    
};

class FileException: public AvException{
    using AvException::AvException;
    protected:
    String fileName;
    public:
    String getFileName() const { return fileName;}
    FileException(const String &fn,const String &msg, int error=0): AvException(msg,error),fileName(fn){}
    virtual const String msg() const noexcept override{
        if (fileName.empty()) return AvException::msg();
        return "["+fileName+"]"+AvException::msg();
    }
};

class SystemException : public AvException{
    public:
        SystemException(const String &reason):AvException(reason,errno){}
        SystemException(const String &reason, int cerror):AvException(reason,cerror){}
        virtual const String msg() const noexcept override{
            if (error == 0) return AvException::msg();
            char buffer[200];
            return FMT("%s [%s(%d)]",reason,strerror_r(error,buffer,200),error);
        }
};
DECL_EXC(AvException,TimeoutException);
DECL_EXC(AvException,RecurringException);
class AssertException : public AvException{
    using AvException::AvException;
    protected:
    String file;
    int line;
    String func;
    public:
    AssertException(const String &fu, const String &fi, int l,const String &txt):file(fi),func(fu),line(l){
        reason=txt;
    }
    virtual const String msg() const noexcept override{
        return FMT("Assertion: %s in %s(%s:%d)",reason,func,file,line);
    }
};
#ifdef AVDEBUG
#define AVASSERT(cond,text) {if (! cond) throw AssertException(__func__,__FILE__,__LINE__,text);}
#else
#define AVASSERT(cond,text) /**/
#endif

#endif
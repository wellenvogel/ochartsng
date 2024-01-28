/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Object oriented Threading using std::thread
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

#ifndef SIMPLETHREAD_H
#define SIMPLETHREAD_H
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "ItemStatus.h"

//simple automatic unlocking mutex
typedef std::unique_lock<std::mutex> Synchronized;

//simple condition variable that encapsulates the monitor
//you need to ensure that the monitor life cycle fits
//to the condition life cycle
class Condition{
private:
    std::mutex *mutex;
    std::condition_variable cond;
    bool ownsMutex=false;
public:
    Condition(std::mutex &m){
        mutex=&m;
    }
    Condition(){
        mutex=new std::mutex();
        ownsMutex=true;
    }
    ~Condition(){
        if (ownsMutex) delete mutex;
    }
    void wait(){
        Synchronized l(*mutex);
        cond.wait(l);
    };
    void wait(long millis){
        Synchronized l(*mutex);
        cond.wait_for(l,std::chrono::milliseconds(millis));
    }
    void wait(Synchronized &s){
        cond.wait(s);
    }
    void wait(Synchronized &s,long millis){
        cond.wait_for(s,std::chrono::milliseconds(millis));
    }
    void notify(){
        Synchronized g(*mutex);
        cond.notify_one();
    }
    void notify(Synchronized &s){
        cond.notify_one();
    }
    void notifyAll(){
        Synchronized g(*mutex);
        cond.notify_all();
    }
    void notifyAll(Synchronized &s){
        cond.notify_all();
    }
    friend class CondSynchronized;
};

class CondSynchronized{
    Condition *cond;
    std::unique_ptr<Synchronized> s;
    public:
        CondSynchronized(Condition &c){
            cond=&c;
            s=std::make_unique<Synchronized>(*cond->mutex);
        }
        void wait(long millis){
            cond->cond.wait_for(*s,std::chrono::milliseconds(millis));
        }
        void notifyAll(){
            cond->cond.notify_all();
        }
};

//java like runnable interface
class Runnable : public ItemStatus{
protected:
    bool finish=false;
public:
    virtual void run()=0;
    virtual ~Runnable(){}
    virtual void stop(){finish=true;}
    virtual void ToJson(StatusStream &stream){}
};

/**
 * simple thread class
 * allows for 2 models:
 * either inheriting
 * or     using a separate runnable
 */

class Thread : public ItemStatus{
public:
    typedef std::function<void(void)> RunFunction;
private:
    Runnable *runnable;
    std::thread *mthread=NULL;
    RunFunction runFunction;
    std::mutex  lock;
    Condition   *waiter=NULL;
    bool finish;
protected:
    virtual void run(){
        if (runnable){
            runnable->run();
        }
        else{
            if (runFunction){
                runFunction();
            }
        }
    }
public:
    /**
     * ctor for overloading
     */
    Thread(){
        runnable=NULL;
        finish=false;
        waiter=new Condition(lock);
    }
    /**
     * ctor with run function
     */

    Thread(RunFunction function){
        runnable=NULL;
        finish=false;
        runFunction=function;
        waiter=new Condition(lock);
    }
    /**
     * ctor for using a runnable
     * @param runnable will not be owned, i.e. not destroyed
     */
    Thread(Runnable *runnable){
        this->runnable=runnable;
        finish=false;
    };
    ~Thread(){
        if (mthread) delete mthread;
        if (waiter) delete waiter;
    }
    void start(){
        if (mthread) return;
        if (this->runnable){
            Runnable *run=this->runnable;
            //with detach the this pointer could have gone until we really start
            mthread=new std::thread([run]{run->run();});
            return;
        }
        if (this->runFunction){
            mthread=new std::thread(runFunction);
            return;
        }
        mthread=new std::thread([this]{this->run();});
    }
    void wakeUp(){
        if (! waiter) return;
        waiter->notifyAll();
    }
    bool shouldStop() const{
        return finish;
    }
    void stop(){
        finish=true;
        if(waiter != NULL) waiter->notifyAll();
        if (runnable) runnable->stop();
    }
    void join(){
        if (! mthread) return;
        if (! mthread ->joinable()) return;
        mthread->join();
        delete mthread;
        mthread=NULL;
    };
    
    void detach(){
        if (!mthread) return;
        mthread->detach();
    }
    
    bool waitMillis(long millis){
        if (finish) return true;
        waiter->wait(millis);
        return finish;
    }
    
    Runnable * getRunnable(){
        return runnable;
    }
    virtual void ToJson(StatusStream &stream){
        if (runnable != NULL) {
            runnable->ToJson(stream);
        }
    }
};


#endif /* SIMPLETHREAD_H */


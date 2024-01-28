#ifndef _CACHEHANDLER_H
#define _CACHEHANDLER_H
#include "ItemStatus.h"
#include <memory>
#define CACHE_VERSION_IDENTIFIER 1
class CacheHandler : public ItemStatus{
    public:
    typedef std::shared_ptr<CacheHandler> Ptr;
    void ToJson(StatusStream &stream){}
    CacheHandler(String,long,long){}
    void Reset(){}
};
class CacheReaderWriter : public ItemStatus{
    public:
    typedef enum{
        STATE_WRITING,
        STATE_ERROR
    } State;
    CacheReaderWriter(String,String,CacheHandler *,long){}
    void ToJson(StatusStream &stream){}
    void start(){}
    void stop(){}
    void join(){}
    State GetState(){return STATE_ERROR; }
};
#endif
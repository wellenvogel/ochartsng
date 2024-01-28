/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Cache
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
#ifndef _CHARTCACHE_H
#define _CHARTCACHE_H
#include <memory>
#include <map>
#include <atomic>
#include "Chart.h"
#include "Timer.h"
#include "SimpleThread.h"
#include "IChartFactory.h"
#include "ChartSet.h"
#include "S52Data.h"
#include "Exception.h"

class ChartCache{
    static const long LOAD_WAIT_MILLIS=10000; //waittime for a chart being currently loaded
    public:
    DECL_EXC(AvException,ReloadException);
    typedef std::shared_ptr<ChartCache> Ptr;
    ChartCache(IChartFactory::Ptr factory, long loadTime=LOAD_WAIT_MILLIS);
    ~ChartCache();
    Chart::ConstPtr GetChart(s52::S52Data::ConstPtr s52data,ChartSet::Ptr chartSet, const String &fileName, bool doWait=true);
    /**
     * just load a chart and return it
     * will not put it into the cache
     */
    Chart::Ptr LoadChart(ChartSet::Ptr chartSet,const String &fileName,bool headOnly=false);
    int GetNumCharts();
    int CloseAllCharts();
    int CloseBySet(const String &setKey);
    int CloseByMD5(MD5Name cl);
    int CloseChart(const String &setKey, const String &chart);
    int HouseKeeping();
    void CheckMemoryLimit();
    void SetMemoryLimit(unsigned int limit){memKb=limit;}
    void OpenerRun(int sequence,long timeout=1000);
    void StopOpeners();
    void StartOpeners(int number);
    private:
    class CacheEntry{
        public:
            typedef enum
            {
                ST_INIT,
                ST_ERROR,
                ST_HEAD,
                ST_LOADED,
                ST_REQUESTED,
                ST_UNKNOWN
            } State;
            private:
            Chart::Ptr chart;
            s52::S52Data::ConstPtr s52data;
            ChartSet::Ptr chartSet;
            public:
            Timer::SteadyTimePoint lastAccess;
            State state=ST_INIT;
            MD5Name md5;
            int s52sequence=0;
            CacheEntry(s52::S52Data::ConstPtr s52data, Chart::Ptr chart=nullptr)
            {
                this->chart = chart;
                md5=s52data->getMD5();
                s52sequence=s52data->getSequence();
                //normally we do not keep a ref to the s52data
                lastAccess = Timer::steadyNow();
                if (chart) state=ST_LOADED;
            }
            CacheEntry(){}
            s52::S52Data::ConstPtr getS52Data(){return s52data;}
            Chart::Ptr getChart(){return chart;}
            void setLoaded(Chart::Ptr c){
                if (! c) return;
                state=ST_LOADED;
                chart=c;
                s52data.reset(); //if the chart is loaded we do not need the s52ref any more
                chartSet.reset(); //and we can also forget the CS ref
                
            }
            void setError(){
                state=ST_ERROR;
                chart.reset();
                s52data.reset();
                chartSet.reset();
            }
            void setRequested(s52::S52Data::ConstPtr s52data,ChartSet::Ptr cs){
                this->s52data=s52data;
                state=ST_REQUESTED;
                chartSet=cs;
            }
            ChartSet::Ptr getChartSet(){
                return chartSet;
            }


    };
    typedef std::map<String,CacheEntry> Charts;
    String ComputeKey(Chart::ConstPtr chart);
    String ComputeKey(const String &setKey, const String &fileName);
    Condition  waiter;
    /**
     * update the cache entry 
     * set to loaded if the chart is set - otherwise error
    */
    void UpdateChart(s52::S52Data::ConstPtr s52data, const String & setKey, const String &fileName, Chart::Ptr chart);
    Chart::ConstPtr LoadInternal(s52::S52Data::ConstPtr s52data,ChartSet::Ptr chartSet, const String &fileName);
    IChartFactory::Ptr factory;
    Charts charts;
    long loadWaitMillis;
    std::atomic<unsigned int> memKb={0}; //memory limit if != 0
    std::atomic<int> maxOpenCharts={-1};
    std::atomic<int> openerRunSequence={0};

};
#endif
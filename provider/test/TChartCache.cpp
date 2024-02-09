/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Test ChartCache
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
#include <gtest/gtest.h>
#include <functional>
#include "SimpleThread.h"
#include "TestHelper.h"
#include "ChartCache.h"
#include "IChartFactory.h"
#include "ChartFactory.h"
#include "SettingsManager.h"
#include "S52Data.h"

class TestFactory : public IChartFactory
{
    typedef std::function<Chart::Ptr(const String &, const Chart::ChartType, const String &)> CreateFunction;

public:
    String lastCreated;
    int numCreated = 0;
    int numFailed = 0;
    Chart::Ptr nextChart;
    CreateFunction creator;
    void reset()
    {
        numCreated = 0;
        numFailed = 0;
        lastCreated.clear();
        nextChart.reset();
        creator = nullptr;
    }

    virtual Chart::Ptr createChart(ChartSet::Ptr set,  const String &fileName)
    {
        if (nextChart)
        {
            Chart::Ptr rt = nextChart;
            nextChart.reset();
            return rt;
        }
        Chart::Ptr chart;
        if (creator)
        {
            chart = creator(set->GetKey(), GetChartType(fileName), fileName);
        }
        else
        {
            if (GetChartType(fileName) == Chart::UNKNOWN){
                chart= Chart::Ptr();
            }
            else{
                chart = Chart::Ptr(new Chart(set->GetKey(), GetChartType(fileName), fileName));
            }
        }
        if (chart)
        {
            lastCreated = fileName;
            numCreated++;
        }
        else
        {
            numFailed++;
        }
        return chart;
    }
    virtual InputStream::Ptr OpenChartStream(Chart::ConstPtr chart, 
            ChartSet::Ptr chartSet,
            const String fileName,
            bool headerOnly=false) override{
                return InputStream::Ptr();
            }
    virtual Chart::ChartType GetChartType(const String &fileName) const{
        std::unique_ptr<ChartFactory> f(new ChartFactory(OexControl::Ptr()));
        return f->GetChartType(fileName);
    }
};

static SettingsManager *settings=new SettingsManager(".",[](json::JSON &,const String &)->bool{return false;});

class TS52Data : public s52::S52Data{
    public:
        typedef std::shared_ptr<TS52Data> Ptr;
        MD5Name md5;
        using s52::S52Data::S52Data;
        TS52Data(SettingsManager *s,const char *d,int seq=0):
            s52::S52Data( s->GetRenderSettings(),FontFileHolder::Ptr(),seq),md5((const unsigned char*)d){}
        TESTVIRT MD5Name getMD5() const override{ return md5;}
};

static TS52Data::Ptr s52data = std::make_shared<TS52Data>(settings,"012345");

ChartSet::Ptr getSet(String name){
    ChartSetInfo::Ptr info= std::make_shared<ChartSetInfo>();
    info->name=name;
    return std::make_shared<ChartSet>(info,settings);
}
class OpenWaitChart: public Chart{
    long waitMillis;
    bool *destructed;
    public:
        OpenWaitChart(long waitMillis,const String &setKey,ChartType type,const String &fileName, bool *dp=NULL):Chart(setKey,type,fileName){
            this->waitMillis=waitMillis;
            this->destructed=dp;
        }
        virtual ~OpenWaitChart(){
            if (destructed) *destructed=true;
        }
        virtual bool ReadChartStream(InputStream::Ptr,s52::S52Data::ConstPtr s52data,bool headerOnly=false) override{
            if (s52data) md5=s52data->getMD5();
            Timer::microSleep(waitMillis*1000);
            return true;
        }
        virtual bool PrepareRender(s52::S52Data::ConstPtr s52data) override{
            md5=s52data->getMD5();
            return true;
        }
};
static std::shared_ptr<TestFactory> factory(new TestFactory());
TEST(ChartCache,create){
    ChartCache c(factory);
    EXPECT_EQ(c.GetNumCharts(),0);
};
TEST(ChartCache,getUnknownChart){
    factory->reset();
    ChartCache c(factory,100000L);
    EXPECT_THROW(
    Chart::ConstPtr chart=c.GetChart(s52data,getSet("dummy"),"unknown.xxx"),
    AvException);
    EXPECT_EQ(factory->numFailed,1);
}

static bool destructMarker=false;

TEST(ChartCache,openChartWithWait){
    factory->reset();
    destructMarker=false;
    String f("wait.oesu");
    factory->nextChart=std::make_shared<OpenWaitChart>(200,"dummy",Chart::OESU,f);
    ChartCache c(factory,100000L);
    Timer::SteadyTimePoint start=Timer::steadyNow();
    Chart::ConstPtr chart=c.GetChart(s52data, getSet("dummy"),f);
    long waitTime=1000-Timer::remainMillis(start,1000);
    EXPECT_GE(waitTime,200) << "we should have waited 200ms";
    EXPECT_TRUE(chart);
    EXPECT_EQ(chart->GetMD5(),s52data->getMD5());
    EXPECT_EQ(c.GetNumCharts(),1);
    start=Timer::steadyNow();
    chart.reset();
    chart=c.GetChart(s52data,getSet("dummy"),f);
    waitTime=1000-Timer::remainMillis(start,1000);
    EXPECT_EQ(factory->numCreated,0);
    EXPECT_EQ(factory->numFailed,0);
    EXPECT_TRUE(chart);
    EXPECT_EQ(c.GetNumCharts(),1);
    EXPECT_LE(waitTime,10) << "we should not have waited at all";
}

TEST(ChartCache,reopenMd5Change){
    factory->reset();
    destructMarker=false;
    String f("wait.oesu");
    factory->nextChart=std::make_shared<OpenWaitChart>(200,"dummy",Chart::OESU,f);
    ChartCache c(factory,100000L);
    Timer::SteadyTimePoint start=Timer::steadyNow();
    Chart::ConstPtr chart=c.GetChart(s52data, getSet("dummy"),f);
    long waitTime=1000-Timer::remainMillis(start,1000);
    EXPECT_GE(waitTime,200) << "we should have waited 200ms";
    EXPECT_TRUE(chart);
    EXPECT_EQ(chart->GetMD5(),s52data->getMD5());
    EXPECT_EQ(c.GetNumCharts(),1);
    start=Timer::steadyNow();
    chart.reset();
    TS52Data::Ptr changedS52=std::make_shared<TS52Data>(settings,"34567",1); //increment s52 seq
    factory->nextChart=std::make_shared<OpenWaitChart>(200,"dummy",Chart::OESU,f);
    chart=c.GetChart(changedS52,getSet("dummy"),f);
    waitTime=1000-Timer::remainMillis(start,1000);
    EXPECT_EQ(factory->numFailed,0);
    EXPECT_TRUE(chart);
    EXPECT_EQ(chart->GetMD5(),changedS52->getMD5());
    EXPECT_EQ(c.GetNumCharts(),1);
    EXPECT_LE(waitTime,200) << "we should not have waited 200ms";
}
TEST(ChartCache,reloadException){
    factory->reset();
    destructMarker=false;
    String f("wait.oesu");
    factory->nextChart=std::make_shared<OpenWaitChart>(200,"dummy",Chart::OESU,f);
    ChartCache c(factory,100000L);
    Timer::SteadyTimePoint start=Timer::steadyNow();
    Chart::ConstPtr chart=c.GetChart(s52data, getSet("dummy"),f);
    long waitTime=1000-Timer::remainMillis(start,1000);
    EXPECT_GE(waitTime,200) << "we should have waited 200ms";
    EXPECT_TRUE(chart);
    EXPECT_EQ(chart->GetMD5(),s52data->getMD5());
    EXPECT_EQ(c.GetNumCharts(),1);
    start=Timer::steadyNow();
    chart.reset();
    TS52Data::Ptr changedS52=std::make_shared<TS52Data>(settings,"34567");
    EXPECT_THROW(
        chart=c.GetChart(changedS52,getSet("dummy"),f),
        ChartCache::ReloadException);
}
class ChartReader : public Thread{
    ChartCache::Ptr cache;
    public:
        typedef std::shared_ptr<ChartReader> Ptr;
        long runTime=0;
        bool done=false;
        Chart::ConstPtr chart;
        String fileName;
        String setKey;
        ChartReader(ChartCache::Ptr cache, const String setKey, const String fileName){
            this->cache=cache;
            this->setKey=setKey;
            this->fileName=fileName;
        }
        virtual void run() override{
            try{
                Timer::SteadyTimePoint start=Timer::steadyNow();
                chart=cache->GetChart(s52data,getSet(setKey),fileName);
                runTime=1000-Timer::remainMillis(start,1000);
                done=true;
            }catch(Exception &e){
                std::cerr << "Exception in chart reader " << fileName <<": " << e.what() << std::endl;
                done=true;
            }
        }

};

TEST(ChartCache,multipleAccess){
    /**
     * run 11 threads in parallel
     * 10 with a chart that has been injected to be creatable in the factory
     * 1 with a chart with invalid name
     */
    factory->reset();
    destructMarker=false;
    String f("wait.xxx");
    factory->nextChart=std::make_shared<OpenWaitChart>(200,"dummy",Chart::OESU,f,&destructMarker);
    ChartCache::Ptr c=std::make_shared<ChartCache>(factory,1000L);
    std::vector<ChartReader::Ptr> readers;
    const int numThreads=10;
    for (int i=0;i<numThreads;i++){
        readers.push_back(std::make_shared<ChartReader>(c,"dummy",f));
    }
    readers.push_back(std::make_shared<ChartReader>(c,"invalidSet",f));
    Timer::SteadyTimePoint start=Timer::steadyNow();
    for (auto it=readers.begin();it != readers.end();it++){
        (*it)->start();
        (*it)->detach();
    }
    bool allDone=false;
    while (!Timer::steadyPassedMillis(start,1000)){
        bool hasRunning=false;
        for (auto it=readers.begin();it != readers.end();it++){
            if (!(*it)->done) {
                hasRunning=true;
            }
        }
        if (! hasRunning) {
            allDone=true;
            break;   
        }
    }
    long runTime=1000-Timer::remainMillis(start,1000);
    EXPECT_TRUE(allDone);
    EXPECT_LE(runTime,300);
    EXPECT_GE(runTime,190);
    const Chart *ptr=NULL;
    for (auto it=readers.begin();it != readers.end();it++){
        if ((*it)->setKey == String("invalidSet")){
            EXPECT_FALSE((*it)->chart);    
        }
        else{
            EXPECT_LE((*it)->runTime,250);
            EXPECT_TRUE((*it)->chart);
            if (ptr == NULL) {
                ptr=(*it)->chart.get();
            }
            else{
                EXPECT_EQ(ptr,(*it)->chart.get() ) << "all entries should point to the same chart";
            }
        }
    }
    EXPECT_EQ(factory->numCreated,0) << "only the injected chart should have been created";
    EXPECT_EQ(factory->numFailed,1);
    readers.clear(); //cleanup chart references in readers
    c.reset(); //forget the cache
    EXPECT_TRUE(destructMarker) << "chart has not been destructed";
};

TEST(ChartCache,GetChartDifferentSets){
    String s1("dummy");
    String s2("empty");
    String s3("third");
    String f("test.oesu");
    factory->reset();
    factory->creator=[s1,s2,f,s3](const String& set,const Chart::ChartType type,const String& fileName)-> Chart::Ptr{
        if (set == s1 || set == s3){
            return std::make_shared<OpenWaitChart>(1,set,type,fileName);
        }
        else{
            return Chart::Ptr();
        }
    };
    ChartCache::Ptr c=std::make_shared<ChartCache>(factory);
    Chart::ConstPtr c1=c->GetChart(s52data,getSet(s1),f);
    EXPECT_TRUE(c1);
    Chart::ConstPtr c2=c->GetChart(s52data,getSet(s1),f);
    EXPECT_TRUE(c1);
    EXPECT_EQ(c1.get(),c2.get());
    EXPECT_THROW(
    Chart::ConstPtr c3=c->GetChart(s52data,getSet(s2),f),
    AvException);
    Chart::ConstPtr c4=c->GetChart(s52data,getSet(s3),f);
    EXPECT_TRUE(c1);
    EXPECT_NE(c1.get(),c4.get());
    EXPECT_EQ(factory->numCreated,2);
    EXPECT_EQ(factory->numFailed,1);
}
class OpenRememberChart: public Chart{
    bool openCalled=false;
    public:
        OpenRememberChart(const String &setKey,ChartType type,const String &fileName):Chart(setKey,type,fileName){
            
        }
        virtual bool ReadChartStream(InputStream::Ptr,s52::S52Data::ConstPtr s52data,bool headerOnly=false) override{
            openCalled=true;
            this->headerOnly=headerOnly;
            return true;
        }
        virtual ~OpenRememberChart(){
        }
};
TEST(ChartCache,LoadChart){
    factory->reset();
    String s1("dummy");
    String f("test.oesu");
    String fi("test.xxx");
    factory->creator=[s1,f](const String& set,const Chart::ChartType type,const String& fileName)-> Chart::Ptr{
        if (type == Chart::UNKNOWN) throw AvException("unknown chart");
        return std::make_shared<OpenRememberChart>(set,type,fileName);
    };
    ChartCache::Ptr c=std::make_shared<ChartCache>(factory);
    Chart::Ptr c1=c->LoadChart(getSet(s1),f,false);
    EXPECT_TRUE(c1);
    EXPECT_FALSE(c1->HeaderOnly());
    Chart::Ptr c2=c->LoadChart(getSet(s1),f,false);
    EXPECT_TRUE(c2);
    EXPECT_NE(c1.get(),c2.get());
    EXPECT_EQ(c->GetNumCharts(),0);
    EXPECT_THROW(
    Chart::Ptr c3=c->LoadChart(getSet(s1),fi),
    AvException);
    Chart::Ptr c4=c->LoadChart(getSet(s1),f,true);
    EXPECT_TRUE(c4);
    EXPECT_TRUE(c4->HeaderOnly());
}

TEST(ChartCache,WaitTimeout){
    /**
     * loading of the chart takes longer then loadWaitTime of the cache
     * so the one thread that is really loading should be successfully
     * the other one that is waiting should fail
     * there should only be one creation call at the factory
     */
    factory->reset();
    String s1("dummy");
    String f("test.oesu");
    factory->creator=[s1,f](const String& set,const Chart::ChartType type,const String& fileName)-> Chart::Ptr{
        return std::make_shared<OpenWaitChart>(200,set,type,fileName);
    };
    ChartCache::Ptr c=std::make_shared<ChartCache>(factory,100);
    ChartReader::Ptr r1=std::make_shared<ChartReader>(c,s1,f);
    ChartReader::Ptr r2=std::make_shared<ChartReader>(c,s1,f);
    std::vector<ChartReader::Ptr> readers;
    readers.push_back(r1);
    readers.push_back(r2);
    for (auto it=readers.begin();it!=readers.end();it++){
        (*it)->start();
        (*it)->detach();
    }
    bool finished=false;
    bool hasErrors=false;
    while (! finished){
        finished=true;
        for (auto it=readers.begin();it!=readers.end();it++){
            if (!(*it)->done){
                finished=false;
                break;
            }
        }
    }
    EXPECT_EQ(factory->numCreated,1);
    EXPECT_FALSE(hasErrors);
    int numOk=0;
    int numErr=0;
    for (auto it=readers.begin();it!=readers.end();it++){
        if ((*it)->chart) numOk++;
        else numErr++;
    }
    EXPECT_EQ(numOk,1);
    EXPECT_EQ(numErr,1);   
}

TEST(ChartCache,GetChartError){
    factory->reset();
    String s1("dummy");
    String f2("test.xxx");
    String f("test.oesu");
    int count=0;
    factory->creator=[s1,f,&count](const String& set,const Chart::ChartType type,const String& fileName)-> Chart::Ptr{
        count++;
        if (fileName != f){
            throw AvException(FMT("cannot load chart %s",fileName));
        }
        return std::make_shared<OpenWaitChart>(10,set,type,fileName);
    };
    ChartCache::Ptr c=std::make_shared<ChartCache>(factory,100);
    EXPECT_THROW(c->GetChart(s52data,getSet(s1),f2),AvException);
    EXPECT_EQ(count,1);
    EXPECT_THROW(c->GetChart(s52data,getSet(s1),f2),AvException) << "should immediately give an error again";
    EXPECT_EQ(count,1) << "should have not asked the factory again";
    Timer::microSleep(150*1000);
    //should retry now
    EXPECT_THROW(c->GetChart(s52data,getSet(s1),f2),AvException);
    EXPECT_EQ(count,2) << "should have retried after waitTime";

}
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
#include "ChartCache.h"
#include "Logger.h"
#include "SystemHelper.h"
#include <algorithm>

ChartCache::ChartCache(IChartFactory::Ptr factory, long loadWaitMillis)
{
    this->factory = factory;
    this->loadWaitMillis = loadWaitMillis;
}
ChartCache::~ChartCache(){
    StopOpeners(); //TODO: make this safe...
}
void ChartCache::UpdateChart(s52::S52Data::ConstPtr s52data, const String &setKey, const String &fileName,  Chart::Ptr chart)
{
    String key = ComputeKey(setKey, fileName);
    CondSynchronized l(waiter);
    auto it = charts.find(key);
    if (it != charts.end())
    {
        if (it->second.s52sequence != s52data->getSequence()){
            LOG_DEBUG("settings changed during chart load for %s",fileName);
            return;
        }
        if (chart){
            LOG_INFO("updating chart state %s to LOADED", fileName);
            it->second.setLoaded(chart);
        }
        else{
            LOG_INFO("updating chart state %s to ERROR", fileName);
            it->second.setError();
        }
    }
    else
    {
        CacheEntry e(s52data, chart);
        if (chart){
            LOG_INFO("storing chart %s, state LOADED", fileName);
        }
        else{
            LOG_INFO("storing chart %s, state ERROR", fileName);
            e.state=CacheEntry::ST_ERROR;
        }
        charts[key] = e;
    }
    l.notifyAll();
}

Chart::ConstPtr ChartCache::GetChart(s52::S52Data::ConstPtr s52data,
    ChartSet::Ptr chartSet, const String &fileName, bool doWait)
{
    String chartSetKey=chartSet->GetKey();
    {
        String key = ComputeKey(chartSetKey, fileName);
        LOG_DEBUG("getChart %s", fileName.c_str());
        Timer::SteadyTimePoint start = Timer::steadyNow();
        CondSynchronized l(waiter);
        bool loadChart = false;
        while (!Timer::steadyPassedMillis(start, loadWaitMillis))
        {
            auto it = charts.find(key);
            if (it != charts.end())
            {
                CacheEntry::State state = it->second.state;
                if (state == CacheEntry::ST_LOADED && it->second.md5 != s52data->getMD5()){
                    if (s52data->getSequence() <= it->second.s52sequence){
                        throw ReloadException(FMT("settings have changed"));
                    }
                    LOG_DEBUG("MD5 changed for loaded chart %s",it->second.getChart()->GetFileName());
                    charts.erase(it);
                    loadChart=true;
                    break;
                }
                if ((state == CacheEntry::ST_INIT || 
                    state == CacheEntry::ST_ERROR ||
                    state == CacheEntry::ST_REQUESTED)
                     && Timer::remainMillis(it->second.lastAccess, loadWaitMillis) <= 0)
                {
                    // another load had timed out - or we retry after an error
                    // try again
                    // TODO: notify set when a load timed out
                    LOG_ERROR("old chart %s in state %d found, removing it", fileName, state);
                    charts.erase(it);
                    loadChart = true;
                    break;
                }
                else
                {
                    if (state == CacheEntry::ST_LOADED)
                    {
                        it->second.lastAccess = Timer::steadyNow();
                        LOG_DEBUG("found loaded chart %s", fileName);
                        return it->second.getChart();
                    }
                    if (state == CacheEntry::ST_ERROR)
                    {
                        throw RecurringException(FMT("skip chart %s due to previous error", fileName));
                    }
                    if (! doWait && (state == CacheEntry::ST_REQUESTED || state == CacheEntry::ST_INIT)){
                        return Chart::Ptr();
                    }
                    if (state == CacheEntry::ST_REQUESTED){
                        //if we would wait we also can just start loading
                        charts.erase(it);
                        loadChart=true;
                        break;
                    }
                    l.wait(Timer::remainMillis(start, loadWaitMillis));
                    continue;
                }
            }
            else
            {
                loadChart = true;
                break;
            }
        }
        if (loadChart)
        {
            CacheEntry e(s52data);
            if (!doWait)
            {
                e.setRequested(s52data, chartSet);
                LOG_INFO("trigger loading chart %s ", fileName);
            }
            charts[key] = e;
            l.notifyAll(); // TODO: really already notify?
        }
        else
        {
            throw TimeoutException(FMT("timeout waiting for chart %s", fileName));
        }
    }
    if (! doWait){
        return Chart::ConstPtr();
    }
    return LoadInternal(s52data,chartSet,fileName);
}
Chart::ConstPtr ChartCache::LoadInternal(s52::S52Data::ConstPtr s52data,ChartSet::Ptr chartSet, const String &fileName){
    Chart::Ptr chart;
    String chartSetKey=chartSet->GetKey();
    Timer::Measure measure;
    avnav::VoidGuard guard([this, s52data, &chartSetKey, &fileName]
                           { this->UpdateChart(s52data, chartSetKey, fileName, Chart::Ptr()); });
    int globalKb, ourKb, vszKb;

    SystemHelper::GetMemInfo(&globalKb, &ourKb, &vszKb);
    LOG_DEBUG("Memory before chart open global=%dkb,our=%dkb,vsz=%dkb", globalKb, ourKb, vszKb);
    chart = factory->createChart(chartSet,  fileName);
    if (!chart)
    {
        throw AvException(FMT("unable to create chart %s from factory", fileName.c_str()));
    }
    LOG_INFO("load chart for render %s", fileName.c_str());
    InputStream::Ptr chartStream = factory->OpenChartStream(chart, chartSet, fileName);
    bool readResult = chart->ReadChartStream(chartStream, s52data, false);
    chart->LogInfo("readChartStream");
    if (!readResult)
    {
        LOG_ERROR("reading chart %s returned false",fileName);
        throw AvException(FMT("unable to read chart from stream %s", fileName));
    }
    measure.add("read");
    LOG_DEBUG("chart %s prepare render", chart->GetFileName());
    chart->PrepareRender(s52data);
    chart->LogInfo("prepareRender");
    measure.add("prepare");
    UpdateChart(s52data, chartSetKey, fileName, chart);
    measure.add("update");
    SystemHelper::GetMemInfo(&globalKb,&ourKb,&vszKb);
    LOG_DEBUG("Memory after chart open global=%dkb,our=%dkb,vsz=%dkb",globalKb,ourKb,vszKb);
    CheckMemoryLimit();
    guard.disable();
    LOG_INFO("successfully loaded chart %s: %s", fileName, measure.toString());
    return chart;
}

String ChartCache::ComputeKey(Chart::ConstPtr chart)
{
    return ComputeKey(chart->GetSetKey(), chart->GetFileName());
}
String ChartCache::ComputeKey(const String &setKey, const String &fileName)
{
    return setKey + String("#") + fileName;
}
Chart::Ptr ChartCache::LoadChart(ChartSet::Ptr chartSet, const String &fileName, bool headOnly)
{
    LOG_INFO("start loading chart %s, headerOnly=%s", fileName, headOnly);
    Chart::Ptr chart = factory->createChart(chartSet, fileName);
    if (!chart)
    {
        throw AvException("unable to create chart from factory");
    }
    InputStream::Ptr chartStream = factory->OpenChartStream(chart, chartSet, fileName,headOnly);
    bool readResult = chart->ReadChartStream(chartStream, s52::S52Data::ConstPtr(), headOnly);
    if (!readResult)
    {
        throw AvException("unable to read chart from stream");
    }
    return chart;
}
int ChartCache::GetNumCharts()
{
    CondSynchronized l(waiter);
    return charts.size();
}
int ChartCache::CloseAllCharts()
{
    CondSynchronized l(waiter);
    charts.clear();
    l.notifyAll();
    return 0;
}
int ChartCache::CloseByMD5(MD5Name cl){
    LOG_INFO("deleting charts with md5 %s", cl.ToString());
    int rt = 0;
    {
        CondSynchronized l(waiter);
        int old = charts.size();
        avnav::erase_if(charts, [cl](std::pair<const String, CacheEntry> &item)
                        { return item.second.md5 == cl; });
        rt = charts.size() - old;
        if (rt > 0)
        {
            l.notifyAll();
        }
    }
    if (rt > 0){
        LOG_INFO("deleted %d charts with md5 %s", rt,cl.ToString());
    }
    return rt;
}
int ChartCache::CloseBySet(const String &setKey)
{
    LOG_INFO("deleting charts for set %s", setKey);
    int rt = 0;
    {
        CondSynchronized l(waiter);
        String search = setKey + "#";
        const char *sc = search.c_str();
        int old = charts.size();
        avnav::erase_if(charts, [setKey, sc](std::pair<const String, CacheEntry> &item)
                        { return StringHelper::startsWith(item.first, sc); });
        rt = charts.size() - old;
        if (rt > 0)
        {
            l.notifyAll();
        }
    }
    LOG_INFO("deleted %d charts from set %s", rt, setKey.c_str());
    return rt;
}
int ChartCache::CloseChart(const String &setKey, const String &chart){
    String key=ComputeKey(setKey,chart);
    {
    CondSynchronized l(waiter);
    return charts.erase(key);
    }
}
int ChartCache::HouseKeeping()
{
    int maxNum=maxOpenCharts;
    LOG_DEBUG("chartManager housekeeping max=%d", maxNum);
    if (maxNum <= 0) return 0;
    int rt = 0;
    {
        CondSynchronized l(waiter);
        int old = charts.size();
        if (old > maxNum)
        {
            typedef struct
            {
                String first;
                Timer::SteadyTimePoint second;
            } SE;
            std::vector<SE> sorted;
            for (auto it = charts.begin(); it != charts.end(); it++)
            {
                sorted.push_back({it->first, it->second.lastAccess});
            }
            // sort by last access, lowest(oldest) first
            std::sort(sorted.begin(), sorted.end(), [](const SE &item1, const SE &item2)
                      { return item1.second < item2.second; });
            // delete maxNum newest entries (i.e. from the back)
            if (sorted.size() <= maxNum){
                return 0;
            }
            // delete maxNum newest entries (i.e. from the back)
            sorted.erase(sorted.end() - maxNum, sorted.end());
            for (auto it = sorted.begin(); it != sorted.end(); it++)
            {
                auto ci = charts.find(it->first);
                if (ci != charts.end())
                {
                    LOG_INFO("removing chart with key %s", it->first.c_str());
                    charts.erase(ci);
                }
            }
            rt = old-charts.size();
        }
        if (rt > 0)
        {
            l.notifyAll();
        }
    }
    LOG_DEBUG("housekeeping removed %d charts", rt);
    return rt;
}

void ChartCache::CheckMemoryLimit(){
    if (maxOpenCharts > 0) return;
    int ourKb,vszKb;
    int currentOpen=GetNumCharts();
    SystemHelper::GetMemInfo(NULL, &ourKb,&vszKb);
    int vszLimit=SystemHelper::maxVsz()*0.75;
    unsigned int maxExpected=(unsigned int)ourKb;
    LOG_INFO("ChartManager::CheckMemoryLimit our=%dkb, expected=%dkb, vsz=%dkb limit=%dkb, vszLimit=%dkb",
        ourKb,maxExpected,vszKb,memKb,vszLimit);
    bool limitReached=(memKb > 0) && (maxExpected > memKb);
    if (limitReached){
        LOG_INFO("memory limit of %d kb reached, limiting open charts to %d",
                    memKb, currentOpen);
    }
    else{
        limitReached= vszKb > vszLimit;
        if (limitReached){
            LOG_INFO("vsz limit of %d kb reached, limiting open charts to %d",
                vszLimit,currentOpen);
        }
    }
    if (limitReached)
    {
        if (currentOpen < 10)
        {
            currentOpen = 10;
            LOG_INFO("allowing at least %d open charts", currentOpen);
        }
        maxOpenCharts = currentOpen;
    }
}

/**
 * run method for opener threads
 * check if there are charts in state REQUESTED
 * and open them
*/
void ChartCache::OpenerRun(int sequence,long timeout){
    while (sequence == openerRunSequence){
        String chartKey;
        s52::S52Data::ConstPtr s52data;
        ChartSet::Ptr chartSet;
        {
            CondSynchronized l(waiter);
            for (auto it=charts.begin();it != charts.end();it++){
                if (it->second.state == CacheEntry::ST_REQUESTED){
                    chartKey=it->first;
                    s52data=it->second.getS52Data();
                    chartSet=it->second.getChartSet();
                    it->second.state=CacheEntry::ST_INIT; //we start loading
                    break;
                }
            }
        }
        if (! chartKey.empty()){
            StringVector fileAndSet=StringHelper::split(chartKey,"#");
            if (! s52data || ! chartSet){
                LOG_ERROR("unable to run opener for %s - no s52data or chartSet",chartKey);
                {
                    CondSynchronized l(waiter);
                    auto it=charts.find(chartKey);
                    if (it != charts.end()){
                        if (it->second.state == CacheEntry::ST_INIT){
                            charts.erase(chartKey);
                        }
                    }
                }
            }
            else{
                LOG_INFO("opener: load chart %s",fileAndSet[1]);
                try{
                    LoadInternal(s52data,chartSet,fileAndSet[1]);
                } catch (AvException &e){
                    LOG_ERROR("unable to open %s: %s",fileAndSet[1],e.msg());
                    UpdateChart(s52data,fileAndSet[0],fileAndSet[1],Chart::Ptr());

                }
            }
            {
                CondSynchronized l(waiter);
                l.notifyAll();
            }
        }
        else{
            CondSynchronized l(waiter);
            l.wait(timeout);
        }
    }
}

void ChartCache::StopOpeners(){
    openerRunSequence++;
    CondSynchronized l(waiter);
    l.notifyAll();
}
void ChartCache::StartOpeners(int number){
    int seq=openerRunSequence;
    for (int i=0;i<number;i++){
        std::thread([seq,this](){this->OpenerRun(seq,5000);}).detach();
    } 
}


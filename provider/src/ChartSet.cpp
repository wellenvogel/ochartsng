/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Set
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

#include "ChartSet.h"
#include "ChartList.h"
#include "MD5.h"
#include "Logger.h"
#include "StringHelper.h"
#include "CacheHandler.h"
#include "FileHelper.h"
#include <algorithm>




ChartSet::ChartSet(ChartSetInfo::Ptr info, bool canDelete){
        this->info=info;
        this->charts=ChartList::Ptr(new ChartList());
        this->state=STATE_INIT;
        this->openErrors=0;
        this->reopenErrors=0;
        this->reopenOk=0;
        this->canDelete=canDelete;
        AddItem("info",this->info);
        AddItem("charts",charts);
        numValidCharts=0;
    }


const String ChartSet::GetKey(){
    ChartSetInfo::Ptr ip=info; //avoid lock        
    if (! ip) return String();
    return ip->name;
}

void ChartSet::SetReopenStatus(String fileName,bool ok){
    Synchronized l(lock);
    if (! ok){
        reopenOk=0;
        reopenErrors++;
    }
    else{
        reopenOk++;
        if (reopenOk > 5){
            reopenErrors=0;
        }
    }
}

bool ChartSet::SetEnabled(bool enabled,String disabledBy){
    LOG_INFO("ChartSet %s set enabled to %s",GetKey(),PF_BOOL(enabled));
    Synchronized l(lock);
    if (enabled){
        this->disabledBy.clear();
        state=STATE_READY;
    }
    else{
        this->disabledBy=disabledBy;
        state=STATE_DISABLED;
    }
    return false;
}


bool ChartSet::DisabledByErrors(){
    return openErrors >= MAX_ERRORS_RETRY;
}

bool ChartSet::LocalJson(StatusStream &stream){
    Synchronized l(lock);
    String status = "INIT";
    if (DisabledByErrors()) {
        status = "ERROR";
    } else {
        switch (state) {
            case STATE_DISABLED:
                status = "DISABLED";
                break;
            case STATE_READY:
                status = "READY";
                break;
            case STATE_INIT:
                status = "INIT";
                break;
            default:
                break;
        }
    }
    stream["status"]=status;
    stream["active"]=IsActive();
    stream["ready"]=state == STATE_READY;
    stream["errors"]=(int)openErrors;
    stream["globalErrors"]=(int)openErrorsGlob;
    stream["reopenErrors"]=(int)reopenErrors;
    stream["disabledBy"]=disabledBy;
    stream["canDelete"]=canDelete;
    stream["numValidCharts"]=(int)numValidCharts;
    stream["numIgnoredCharts"]=(int)numIgnoredCharts;
    return true;
}
void    ChartSet::SetReady(){
    Synchronized l(lock);
    if (state != STATE_DISABLED) state=STATE_READY;
    ChartList::ChartCounts counts=charts->NumValidCharts();
    numValidCharts=counts.valid;
    numIgnoredCharts=counts.ignored;
}

void ChartSet::AddChart(ChartInfo::Ptr info){
    Synchronized l(lock);
    if (info)
    {
        info->SetChartSetKey(GetKey());
        charts->AddChart(info);
        if (info->IsValid())
        {
            openErrors = 0;
            ChartList::ChartCounts counts=charts->NumValidCharts();
            numValidCharts=counts.valid;
            numIgnoredCharts=counts.ignored;
            return;
        }
    }
    openErrors++;
    openErrorsGlob++;
}
ChartSet::ExtentInfo ChartSet::GetExtent(){
    Synchronized l(lock);
    ExtentInfo rt;
    rt.extent=charts->GetBoundings();
    rt.maxScale=charts->GetMaxScale();
    rt.minScale=charts->GetMinScale();
    return rt;
}
size_t  ChartSet:: GetNumCharts(){
    Synchronized l(lock);
    return charts->NumCharts();
}
int  ChartSet::RemoveUnverified(){
    Synchronized l(lock);
    int rt=charts->RemoveUnverified();
    if (rt > 0){
        LOG_INFO("removed %d not longer existing charts from %s",rt,info->name);
    }
    ChartList::ChartCounts counts=charts->NumValidCharts();
    numValidCharts=counts.valid;
    numIgnoredCharts=counts.ignored;
    return rt;
}

WeightedChartList  ChartSet::FindChartForTile(const Coord::Extent &tileExtent){
    Synchronized l(lock);
    return charts->FindChartForTile(tileExtent);
}
String ChartSet::GetSetToken(){
    Synchronized l(lock);
    return charts->GetHash().ToString();
}

ChartInfo::Ptr   ChartSet::FindInfo(const String &fileName){
    Synchronized l(lock);
    return charts->FindInfo(fileName);
}
String ChartSet::GetChartKey(Chart::ChartType type, const String &fileName){
    Synchronized l(lock);
    String baseName=FileHelper::fileName(fileName,true);
    auto it=info->chartKeys.find(baseName);
    if (it != info->chartKeys.end()) return it->second;
    return info->userKey;
}
StringVector ChartSet::GetFailedChartNames(int maxErrors){
    Synchronized l(lock);
    return charts->GetFailedChartNames(); 
}
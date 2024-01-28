/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Info
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

#include "ChartList.h"
#include "StringHelper.h"



ChartList::ChartList() {
   
    
};
ChartList::~ChartList(){
}
bool ChartList::AddChart(ChartInfo::Ptr chart){
    if (!chart) return false;
    bool changed=false;
    bool existing=false;
    for (auto it=chartList.begin();it != chartList.end();it++){
        if ((*it)->GetFileName() == chart->GetFileName()){
            if (! chart->IsValid()){
                chart->AddErrors((*it)->GetNumErrors());
            }
            if ((*it)->GetFileSize() != chart->GetFileSize()||
                (*it)->GetFileTime() != chart->GetFileTime()){
                    changed=true;
                }
            *it=chart;
            existing=true;
            break; 
        }
    }
    if (!existing)
    {
        chartList.push_back(chart);
        if (chart->IsValid())
        {
            boundings.extend(chart->GetExtent());
            if (minScale < 0 || chart->GetNativeScale() < minScale)
            {
                minScale = chart->GetNativeScale();
            }
            if (maxScale < 0 || chart->GetNativeScale() > maxScale)
            {
                maxScale = chart->GetNativeScale();
            }
        }
        changed = true;
    }
    if (changed){
        computeHash();
    }
    return changed;
}

void ChartList::computeHash(){
    MD5 newHash;
    for (auto it=chartList.begin();it != chartList.end();it++){
        newHash.AddValue((*it)->GetFileName());
        int64_t v;
        v=(*it)->GetFileSize();
        MD5_ADD_VALUE(newHash,v);
        v=(*it)->GetFileTime();
        MD5_ADD_VALUE(newHash,v);
    }
    hash=newHash.GetValueCopy();
}
int ChartList::RemoveUnverified(){
    int rt=avnav::erase_if(chartList,[](const ChartInfo::Ptr &it){
        return it->GetState() == ChartInfo::NEEDS_VER;    
    });
    if (rt > 0){
        computeHash();
    }
    return rt;
}

WeightedChartList ChartList::FindChartForTile(const Coord::Extent &tileExtent){
    WeightedChartList rt;
    for (auto it=chartList.begin();it!= chartList.end();it++){
        ChartInfo::Ptr info=(*it);
        if (! info->IsValid() || info->IsIgnored()) {
            continue;
        }
        int scale=(info->HasTile(tileExtent));
        if (scale > 0){
            ChartInfoWithScale winfo(scale,info);
            rt.push_back(winfo);            
        }
    }
    return rt;
}

void ChartList::ToJson(StatusStream &stream){
    stream["numCharts"]=GetSize();
    stream["minScale"]=GetMinScale();
    stream["maxScale"]=GetMaxScale();
}
ChartList::ChartCounts ChartList::NumValidCharts(){
    ChartCounts rt;
    InfoList::iterator it;
    for (it=chartList.begin();it!=chartList.end();it++){
        ChartInfo::Ptr info=(*it);
        if (! info->IsValid()) {
            continue;
        }
        if ( info->IsIgnored()) rt.ignored++;
        else rt.valid++;
    }
    return rt;
}
ChartInfo::Ptr ChartList::FindInfo(const String &fileName){
    for (auto it=chartList.begin();it != chartList.end();it++){
        if ((*it)->GetFileName() == fileName){
            return *it;
        }
    }
    return ChartInfo::Ptr();
}
StringVector ChartList::GetFailedChartNames(int maxErrors){
    StringVector rt;
    for (auto it=chartList.begin();it != chartList.end();it++){
        if (!(*it)->IsValid() && (maxErrors < 0 || (*it)->GetNumErrors() <= maxErrors)){
            rt.push_back((*it)->GetFileName());
        }
    }
    return rt;
}


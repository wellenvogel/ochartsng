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
#include "MD5.h"
#include "Logger.h"
#include "StringHelper.h"
#include "CacheHandler.h"
#include "FileHelper.h"
#include <algorithm>

ChartSet::ChartSet(ChartSetInfo::Ptr info, bool canDelete)
{
    this->info = info;
    this->state = STATE_INIT;
    this->openErrors = 0;
    this->reopenErrors = 0;
    this->reopenOk = 0;
    this->canDelete = canDelete;
    AddItem("info", this->info);
    numValidCharts = 0;
}

const String ChartSet::GetKey()
{
    ChartSetInfo::Ptr ip = info; // avoid lock
    if (!ip)
        return String();
    return ip->name;
}

void ChartSet::SetReopenStatus(String fileName, bool ok)
{
    Synchronized l(lock);
    if (!ok)
    {
        reopenOk = 0;
        reopenErrors++;
    }
    else
    {
        reopenOk++;
        if (reopenOk > 5)
        {
            reopenErrors = 0;
        }
    }
}

bool ChartSet::SetEnabled(bool enabled, String disabledBy)
{
    LOG_INFO("ChartSet %s set enabled to %s", GetKey(), PF_BOOL(enabled));
    Synchronized l(lock);
    if (enabled)
    {
        this->disabledBy.clear();
        state = STATE_READY;
    }
    else
    {
        this->disabledBy = disabledBy;
        state = STATE_DISABLED;
    }
    return false;
}

bool ChartSet::DisabledByErrors()
{
    return openErrors >= MAX_ERRORS_RETRY;
}

bool ChartSet::LocalJson(StatusStream &stream)
{
    Synchronized l(lock);
    String status = "INIT";
    if (DisabledByErrors())
    {
        status = "ERROR";
    }
    else
    {
        switch (state)
        {
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
    stream["status"] = status;
    stream["active"] = IsActive();
    stream["ready"] = state == STATE_READY;
    stream["errors"] = (int)openErrors;
    stream["globalErrors"] = (int)openErrorsGlob;
    stream["reopenErrors"] = (int)reopenErrors;
    stream["disabledBy"] = disabledBy;
    stream["canDelete"] = canDelete;
    stream["numValidCharts"] = (int)numValidCharts;
    stream["numIgnoredCharts"] = (int)numIgnoredCharts;
    stream["numCharts"]=(int)numCharts;
    stream["minScale"]=(int)minScale;
    stream["maxScale"]=(int)maxScale;
    return true;
}
void ChartSet::SetReady()
{
    Synchronized l(lock);
    if (state != STATE_DISABLED)
        state = STATE_READY;
    computeHash();
}

void ChartSet::AddChart(ChartInfo::Ptr info)
{
    Synchronized l(lock);
    if (!info)
        return;
    info->SetChartSetKey(GetKey());
    bool changed = false;
    bool existing = false;
    for (auto &&it : chartList)
    {
        if (it->GetFileName() == info->GetFileName())
        {
            if (!info->IsValid())
            {
                info->AddErrors(it->GetNumErrors());
            }
            if (it->GetFileSize() != info->GetFileSize() ||
                it->GetFileTime() != info->GetFileTime())
            {
                changed = true;
            }
            it = info;
            existing = true;
            break;
        }
    }
    if (!existing)
    {
        chartList.push_back(info);
        if (info->IsValid())
        {
            boundings.extend(info->GetExtent());
            if (info->GetNativeScale() < minScale)
            {
                minScale = info->GetNativeScale();
            }
            if (info->GetNativeScale() > maxScale)
            {
                maxScale = info->GetNativeScale();
            }
        }
        changed = true;
    }
    if (changed)
    {
        computeHash();
    }

    if (info->IsValid())
    {
        openErrors = 0;
        return;
    }

    openErrors++;
    openErrorsGlob++;
}
ChartSet::ExtentInfo ChartSet::GetExtent()
{
    Synchronized l(lock);
    ExtentInfo rt;
    rt.extent = boundings;
    rt.maxScale = maxScale;
    rt.minScale = minScale;
    return rt;
}
void ChartSet::FillChartExtents(std::vector<Coord::Extent> &extents)
{
    Synchronized l(lock);
    for (const auto &info : chartList)
    {
        extents.push_back(info->GetExtent());
    }
}

size_t ChartSet::GetNumCharts()
{
    Synchronized l(lock);
    return chartList.size();
}
int ChartSet::RemoveUnverified()
{
    Synchronized l(lock);
    int rt = avnav::erase_if(chartList, [](const ChartInfo::Ptr &it)
                             { return it->GetState() == ChartInfo::NEEDS_VER; });
    if (rt > 0)
    {
        computeHash();
    }
    if (rt > 0)
    {
        LOG_INFO("removed %d not longer existing charts from %s", rt, info->name);
    }
    return rt;
}

WeightedChartList ChartSet::FindChartForTile(const Coord::Extent &tileExtent)
{
    Synchronized l(lock);
    WeightedChartList rt;
    for (auto &&info : chartList)
    {
        if (!info->IsValid() || info->IsIgnored())
        {
            continue;
        }
        int scale = (info->HasTile(tileExtent));
        if (scale > 0)
        {
            ChartInfoWithScale winfo(scale, info);
            rt.push_back(winfo);
        }
    }
    return rt;
}
String ChartSet::GetSetToken()
{
    Synchronized l(lock);
    return hash.ToString();
}

ChartInfo::Ptr ChartSet::FindInfo(const String &fileName)
{
    Synchronized l(lock);
    for (auto &&it : chartList)
    {
        if (it->GetFileName() == fileName)
        {
            return it;
        }
    }
    return ChartInfo::Ptr();
}
String ChartSet::GetChartKey(Chart::ChartType type, const String &fileName)
{
    Synchronized l(lock);
    String baseName = FileHelper::fileName(fileName, true);
    auto it = info->chartKeys.find(baseName);
    if (it != info->chartKeys.end())
        return it->second;
    return info->userKey;
}
StringVector ChartSet::GetFailedChartNames(int maxErrors)
{
    Synchronized l(lock);
    StringVector rt;
    for (auto &&it : chartList)
    {
        if (!it->IsValid() && (maxErrors < 0 || it->GetNumErrors() <= maxErrors))
        {
            rt.push_back(it->GetFileName());
        }
    }
    return rt;
}
/**
 * lock must already been held
 * alos computes valid charts, ignored charts
 */
void ChartSet::computeHash()
{
    MD5 newHash;
    int newValid = 0;
    int newIgored = 0;
    for (auto &&it : chartList)
    {
        newHash.AddValue(it->GetFileName());
        int64_t v;
        v = it->GetFileSize();
        newHash.AddValue(v);
        v = it->GetFileTime();
        newHash.AddValue(v);
        if (!it->IsValid())
        {
            continue;
        }
        if (it->IsIgnored())
            newIgored++;
        else
            newValid++;
    }
    hash = newHash.GetValueCopy();
    numIgnoredCharts = newIgored;
    numValidCharts = newValid;
    numCharts=chartList.size();
}
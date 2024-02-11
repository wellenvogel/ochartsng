/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Tile Cache
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2024 by Andreas Vogel   *
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

#ifndef _TILECACHE_H
#define _TILECACHE_H
#include "Types.h"
#include "ItemStatus.h"
#include "MD5.h"
#include <vector>
#include <atomic>
#include <map>
#include "Timer.h"
#include "SimpleThread.h"
#include "Tiles.h"

class TileCache : public ItemStatus{
    public:
    using Png=DataPtr;
    using Ptr=std::shared_ptr<TileCache>;
    class CacheDescription{
        public:
        int settingsSequence=0;
        String setHash;
        int setSequence=0;
        bool isNewer(const CacheDescription &other) const{
            if (settingsSequence == other.settingsSequence &&
                setHash == other.setHash
            ) return false;
            if (settingsSequence < other.settingsSequence
            || setSequence < other.setSequence) return false;
            return true;
        }
        bool equals(const CacheDescription &other) const{
            return (settingsSequence == other.settingsSequence
                && setHash == other.setHash);
        }
    };
    private:
    class CacheEntry{
        public:
        using Ptr=std::shared_ptr<CacheEntry>;
        CacheDescription description;
        Png data;
        Timer::SteadyTimePoint lastAccess;
        size_t size=2*sizeof(MD5Name)+sizeof(Timer::SteadyTimePoint);
        CacheEntry(Png d,  const CacheDescription &dsc, size_t keySize):
                data(d),description(dsc){
                    lastAccess=Timer::steadyNow();
                    size+=data->capacity()+keySize;
                    size=size/1024;
            }
        bool isNewer(const CacheEntry &other) const{
            return description.isNewer(other.description);
        }
    };
    Condition lock;
    using Data=std::map<String,CacheEntry::Ptr>;
    std::atomic<int> numEntries={0};
    std::atomic<int> numKb={0};
    size_t maxMem;
    Data cache;
    String getKey(const TileInfo &tile);
    bool stopAudit=false;
    void auditRun();
    public:
    TileCache(size_t max);
    virtual void ToJson(StatusStream &stream);
    void cleanup();
    void clean(String setKey="");
    void cleanBySettings(int remainingSeqeunce);
    bool addTile(Png d, const CacheDescription &description, const TileInfo &tile);
    Png getTile(const CacheDescription &description, const TileInfo &tile);
    void stop();

};
#endif
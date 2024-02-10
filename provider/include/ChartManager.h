/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Manager
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

#ifndef CHARTMANAGER_H
#define CHARTMANAGER_H

#include <map>
#include <vector>
#include <deque>
#include <memory>
#include <atomic>
#include "ChartInfo.h"
#include "ChartSetInfo.h"
#include "Logger.h"
#include "SimpleThread.h"
#include "CacheHandler.h"
#include "ChartSet.h"
#include "StatusCollector.h"
#include "SettingsManager.h"
#include "StringHelper.h"
#include "ChartCache.h"
#include "S52Data.h"

class CacheFiller;


typedef std::map<String,ChartSet::Ptr> ChartSetMap;
typedef std::shared_ptr<ChartSetMap> ChartSetMapPtr;
class ChartManager : public StatusCollector{
public:
    using Ptr=std::shared_ptr<ChartManager>;
    using ManagerState= enum{
            STATE_INIT,
            STATE_PREPARE,
            STATE_READING,
            STATE_READY        
    } ;
    using RunFunction=std::function<void (void)>;
    using SetChangeFunction=std::function<void(const String &setKey)>;
    using SettingsChangeFunction=std::function<void(s52::S52Data::ConstPtr newData)>;
    ChartManager(FontFileHolder::Ptr fontFile, IBaseSettings::ConstPtr bs, RenderSettings::ConstPtr rs, IChartFactory::Ptr chartFactory,String s57dataDir, unsigned int memLimitKb, int numOpeners);
    /**
     * really start reading the charts
     * beside initially opening them it will also monitor the mem usage
     * and define how many charts we can keep open
     * memSizeKb is the max allowed size
     * so we need to compute:
     * currentUsedMemKb-currentCacheKb+maxCacheKb<memSizeKb
     * @param dirsAndFiles
     * @param knownExtensions
     * @param memKb
     * @return 
     */   
    int                 ReadCharts(const StringVector &dirs, bool canDelete=true);    
    virtual             ~ChartManager();
    int                 GetNumCharts();
    bool                StartCaches(String dataDir,long maxCacheEntries,long maxFileEntries);
    bool                StartFiller(long maxPerSet,long maxPrefillZoom,bool waitReady=true);
    /**
     * @return 
     */
    bool                UpdateSettings(IBaseSettings::ConstPtr bs, RenderSettings::ConstPtr rs);
    /**
     * delete a chart set from all maps
     * @param key
     * @return 
     */
    bool                DeleteChartSet(const String &key);
    typedef struct{
        ChartSet::Ptr set;
        Chart::Ptr chart;
    } TryResult;
    /**
     * try to open a chart file (used after uploading),
     * the file is not assigned to any set and is closed immediately
     * @param chartFile
     * @return false in case of errors
     */
    TryResult           TryOpenChart(const String &chartFile);
    
    bool                Stop();
    ChartSet::Ptr       GetChartSet(String key);
    virtual bool        LocalJson(StatusStream &stream);
    IBaseSettings::ConstPtr GetBaseSettings();
    ManagerState        GetState();
    unsigned long       GetCurrentCacheSizeKb();
    unsigned long       GetMaxCacheSizeKb();
    String              GetCacheFileName(const String &fileName);
    void                PauseFiller(bool on);
    /**
     * write out extensions and native scale for all charts
     * @param config
     * @return true if written
     */
    bool                WriteChartInfoCache(const String &configFile);
    /**
     * read extension an native scale from the cache file
     * is used as a replacement for ReadCharts on fast start
     * it will set all needed sets to parsing state
     * @param config
     * @return true if at least one set needs parsing
     */
    bool                ReadChartInfoCache(const String &configFile);

    Chart::ConstPtr     OpenChart(s52::S52Data::ConstPtr s52data, ChartInfo::Ptr info,bool doWait=true);
    Chart::ConstPtr     OpenChart(const String &setName, const String &chartName, bool doWait=true);
    bool                CloseChart(const String &setName, const String &chartName);
    int                 HandleCharts(const StringVector &dirsAndFiles,bool setsOnly, bool canDelete=false);
    WeightedChartList   FindChartsForTile(RenderSettings::ConstPtr renderSettingsPtr,const TileInfo &tile, bool allLower=false);
    ChartSet::ExtentList  GetChartSetExtents(const String &chartSetKey,bool includeSet);
    /**
     * add mappings to shorten the chart set names for known directories
    */
    bool                AddKnownDirectory(String dirname,String shortname);
    ChartSetInfoList    ListChartSets();
    String              GetChartSetSequence(const String &chartSetKey);
    /**
     * remove all charts that have been read from the cache but are not 
     * available any more
     */ 
    int                 RemoveUnverified();
    bool                HasOpeners() const;
    s52::S52Data::ConstPtr GetS52Data() const;
    Chart::ChartType    GetChartType(const String &fileName) const;
    void                registerSetChagend(SetChangeFunction f);
    void                registerSettingsChanged(SettingsChangeFunction f);
private:
    class HouseKeeper : public Thread{
        ChartCache::Ptr cache;
        long intervallMs=0;
        public:
            typedef std::shared_ptr<HouseKeeper> Ptr;
            HouseKeeper(ChartCache::Ptr ck, long iv):cache(ck),intervallMs(iv){}
            virtual ~HouseKeeper(){
                stop();
            }
        protected:
        virtual void run() override;
    };

    /**
     * compute the active/inactive sets
     * based on the SettingsManager
     * must be called with lock already held
     * @return 
     */
    int                 computeActiveSets();
    void                runWithStoppedFiller(RunFunction f);
    bool                buildS52Data(RenderSettings::ConstPtr s);
    s52::S52Data::Ptr   s52data;
    IBaseSettings::ConstPtr baseSettings;
    ChartSetMap         chartSets;
    NameValueMap        dirMappings;
    std::mutex          lock;
    std::mutex          s52lock; //lock for building/updating the s52data
    String              s57Dir;
    String              KeyFromChartDir(String chartDir);
    ChartSet::Ptr       findOrCreateChartSet(String chartFile,bool mustExist=false,bool canDelete=false,bool addToList=true);
    bool                HandleChart(const String &chartFile,bool setsOnly,bool canDeleteSet);
    /**
     * cleanup currently open charts from disabled chart sets
     * main thread only
     */
    void                CloseDisabled();
    CacheFiller         *filler;
    ManagerState        state;
    std::atomic<int>    numRead;
    long                maxPrefillPerSet;
    long                maxPrefillZoom;
    ChartCache::Ptr     chartCache;
    IChartFactory::Ptr  chartFactory;
    HouseKeeper::Ptr    houseKeeper;
    int                 numOpeners;
    FontFileHolder::Ptr fontFile;
    SetChangeFunction   setChanged;
    SettingsChangeFunction settingsChanged;
};

#endif /* CHARTMANAGER_H */


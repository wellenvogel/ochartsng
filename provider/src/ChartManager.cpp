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



#include "ChartManager.h"
#include "SystemHelper.h"
#include <algorithm>
#include <unordered_set>
#include "StatusCollector.h"
#include "FileHelper.h"
#include "Types.h"
#include <set>
#include <numeric>

ChartManager::ChartManager( FontFileHolder::Ptr f, IBaseSettings::ConstPtr bs, RenderSettings::ConstPtr rs,IChartFactory::Ptr chartFactory, 
        String s57dataDir, unsigned int memLimitKb,int numOpeners) {
    this->fontFile=f;        
    this->s57Dir=s57dataDir;
    this->baseSettings=bs;
    this->numOpeners=numOpeners; 
    state=STATE_INIT;
    numRead=0;
    maxPrefillPerSet=0;
    maxPrefillZoom=0;
    buildS52Data(rs);
    this->chartFactory=chartFactory;
    chartCache=std::make_shared<ChartCache>(chartFactory);
    chartCache->SetMemoryLimit(memLimitKb);
    chartCache->StartOpeners(numOpeners);
    houseKeeper=std::make_shared<HouseKeeper>(chartCache,10000);
    houseKeeper->start();
}
bool ChartManager::HasOpeners()const {
    return numOpeners > 0;
}

ChartManager::~ChartManager() {
}

IBaseSettings::ConstPtr ChartManager::GetBaseSettings() {
    return baseSettings;
}


bool ChartManager::LocalJson(StatusStream &stream){
    int memkb;
    SystemHelper::GetMemInfo(NULL,&memkb);
    Synchronized locker(lock);
    String status="INIT";
    switch(state){
        case STATE_PREPARE:
            status="PREPARE";
            break;
        case STATE_READING:
            status="READING";
            break;
        case STATE_READY:
            status="READY";
            break;
        default:
            break;
    }
    stream["openCharts"]=chartCache->GetNumCharts();
    stream["state"]=status;
    stream["numRead"]=(int)numRead;
    stream["memoryKb"]=(int)memkb;
    return true;
}
String  ChartManager::KeyFromChartDir(String chartDir){
    chartDir=FileHelper::realpath(chartDir);
    Synchronized l(lock);
    for (auto it=dirMappings.begin();it!=dirMappings.end();it++){
        if (StringHelper::startsWith(chartDir,it->first)){
            String subDir=chartDir.substr(it->first.size());
            if (it->second.empty() && subDir.size() > 0 && subDir[0] == '/') subDir=subDir.substr(1);
            return String("CSI_")+it->second+StringHelper::SanitizeString(subDir);
        }
    }
    return StringHelper::SanitizeString(String("CS")+chartDir);
}   

bool ChartManager::AddKnownDirectory(String dirname,String shortname){
    Synchronized l(lock);
    auto it=dirMappings.find(dirname);
    if (it != dirMappings.end()) return false;
    dirMappings[dirname]=shortname;
    return true;
}

ChartSet::Ptr ChartManager::CreateChartSet(const String &dir,bool canDelete){
    String key=KeyFromChartDir(dir);
    ChartSetInfo::Ptr info=ChartSetInfo::ParseChartInfo(dir,key);
        if (! info->infoParsed){
            LOG_INFO("unable to retrieve chart set info for %s, trying anyway with defaults",dir);
        }
        else{
            LOG_INFO("created chart set with key %s for directory %s",key,dir);
        }
        if (info->title.empty()){
            //seems that we did not find a chart info
            //use our key without the prefixes
            String title=key;
            if (StringHelper::startsWith(title,"CSI_")) title=title.substr(4);
            if (StringHelper::startsWith(title,"CS")) title=title.substr(2);
            info->title=title;
        }
        ChartSet::Ptr newSet=std::make_shared<ChartSet>(info,canDelete);
        return newSet;
}

ChartSet::Ptr ChartManager::findOrCreateChartSet(String chartFile,bool mustExist, bool canDelete, bool addToList){
        String chartDir=FileHelper::dirname(chartFile);
        String key=KeyFromChartDir(chartDir);
        ChartSetMap::iterator it;
        {
            Synchronized locker(lock);
            it=chartSets.find(key);
            if (it != chartSets.end()){
                if (it->second->info->dirname != chartDir){
                    LOG_ERROR("found a chart dir %s that has the same short name like %s, cannot handle this",
                        chartDir.c_str(),it->second->info->dirname.c_str());
                    return NULL;
                }
                if (mustExist && ! it->second->IsEnabled()){
                    LOG_INFO("chart set %s is disabled, do not load charts",
                            it->second->GetKey().c_str());
                    return ChartSet::Ptr();
                }
                return it->second;
            }
        }
        if (mustExist){
            LOG_ERROR("no chart info created for file %s",chartFile.c_str());
            return ChartSet::Ptr();
        }
        ChartSetInfo::Ptr info=ChartSetInfo::ParseChartInfo(chartDir,key);
        if (! info->infoParsed){
            LOG_INFO("unable to retrieve chart set info for %s, trying anyway with defaults",chartDir.c_str());
        }
        else{
            LOG_INFO("created chart set with key %s for directory %s",key.c_str(),chartDir.c_str());
        }
        if (info->title.empty()){
            //seems that we did not find a chart info
            //use our key without the prefixes
            String title=key;
            if (StringHelper::startsWith(title,"CSI_")) title=title.substr(4);
            if (StringHelper::startsWith(title,"CS")) title=title.substr(2);
            info->title=title;
        }
        {
            Synchronized locker(lock);
            //check again - should normally not happen...
            ChartSetMap::iterator it=chartSets.find(key);
            if (it != chartSets.end()){
                return it->second;
            }
            ChartSet::Ptr newSet=std::make_shared<ChartSet>(info,canDelete);
            if (addToList){
                IBaseSettings::EnabledState state=baseSettings->IsChartSetEnabled(newSet->GetKey());
                if (state == IBaseSettings::EnabledState::DISABLED){
                    newSet->SetEnabled(state,false);
                }
                chartSets[key]=newSet;
                AddItem("chartSets",newSet,true);
            }
            return newSet;     
        }
    }

Chart::ChartType ChartManager::GetChartType(const String &fileName) const{
    return chartFactory->GetChartType(fileName);
}
ChartManager::TryResult ChartManager::TryOpenChart(const String &chartFile){
    LOG_INFO("ChartManager: TryOpenChart %s",chartFile);
    TryResult rt;
    Chart::ChartType type=GetChartType(chartFile);
    if (type == Chart::UNKNOWN){
        throw FileException(chartFile,"unknown extension for chart file");
    }
    //create a temp set
    rt.set=CreateChartSet(FileHelper::dirname(chartFile),true);
    rt.chart=chartCache->LoadChart(rt.set,chartFile,true);
    LOG_INFO("ChartManager: TryOpenChart %s OK",chartFile);
    return rt;
}

Chart::ConstPtr ChartManager::OpenChart(const String &setName, const String &chartName, bool doWait){
    ChartSet::Ptr chartSet;
    {
        Synchronized locker(lock);
        auto it=chartSets.find(setName);
        if (it == chartSets.end()) throw AvException(FMT("unknown chart set %s",setName));
        chartSet=it->second;
    }
    String fileName=FileHelper::concatPath(chartSet->info->dirname,chartName);
    Chart::ConstPtr rt=chartCache->GetChart(s52data,chartSet,fileName,doWait);
    return rt;
}
bool ChartManager::CloseChart(const String &setName, const String &chartName){
    ChartSet::Ptr chartSet;
    {
        Synchronized locker(lock);
        auto it=chartSets.find(setName);
        if (it == chartSets.end()) throw AvException(FMT("unknown chart set %s",setName));
        chartSet=it->second;
    }
    String fileName=FileHelper::concatPath(chartSet->info->dirname,chartName);
    return chartCache->CloseChart(setName,fileName) != 0;
}

bool ChartManager::HandleChart(const String &chartFile,ChartSet::Ptr set){
    LOG_INFO("ChartManager: HandleChart %s",chartFile);
    Chart::ChartType type=GetChartType(chartFile);
    if (type == Chart::UNKNOWN){
        LOG_DEBUG("unknown extension for chart file %s, skip",chartFile.c_str());
        return false;
    }
    ChartInfo::Ptr chartInfo=set->FindInfo(chartFile);
    if (chartInfo){
        bool verified=chartInfo->VerifyChartFileName(chartFile);
        if (verified){
            LOG_INFO("skip reading chart %s as already in set",chartFile);
            return false;
        }
        LOG_INFO("fileHash changed for %s, reread",chartFile);
    }
    if (!set->DisabledByErrors()) {
        Chart::Ptr chart;
        try{
            chart=chartCache->LoadChart(set,chartFile,true);
            if (chart) {
                ChartInfo::Ptr info=std::make_shared<ChartInfo>(chart->GetType(), chartFile,
                        chart->GetNativeScale(), chart->GetChartExtent(), chart->SoftUnder(), chart->IsIgnored());
                set->AddChart(info);
                LOG_INFO("adding chart %s",info->ToString());
                return true;
            }
            else{
                set->AddChart(ChartInfo::Ptr()); //count errors
                LOG_ERROR("loading chart failed with unknown error for %s",chartFile); 
            }
        }catch (AvException &e){
            LOG_ERROR("unable to load chart %s:%s",chartFile,e.what());
        }
    }
    else{
        LOG_ERROR("loading chart %s failed due to too many errors in set",chartFile);         
        set->AddChart(ChartInfo::Ptr()); //count errors
    }
    ChartInfo::Ptr info=std::make_shared<ChartInfo>(chartFactory->GetChartType(chartFile), chartFile);
    //store the info with an error state
    set->AddChart(info);
    return false;
    
}
ChartSet::Ptr ChartManager::ParseChartDir(const String &dir, bool canDelete)
{
    LOG_INFO("parsing chart dir %s", dir);
    if (!FileHelper::exists(dir, true))
    {
        LOG_INFO("chart dir %s not found", dir);
        return ChartSet::Ptr();
    }
    int rt = 0;
    int all=0;
    StringVector filesInDir = FileHelper::listDir(dir);
    if (filesInDir.empty())
    {
        LOG_INFO("no files found in %s", dir);
        return ChartSet::Ptr();
    }
    String key=KeyFromChartDir(dir);
    ChartSet::Ptr chartSet=CreateChartSet(dir,canDelete);
    for (auto && chartFile : FileHelper::listDir(dir)){
        all++;
        if(HandleChart(chartFile,chartSet)) rt++;
    }
    LOG_INFO("parsed %d/%d charts from %s",rt,all,dir);
    return chartSet;
}

int ChartManager::ReadChartDirs(const StringVector &dirsAndFiles,bool canDelete ){
    int numHandled=0;
    std::vector<ChartSet::Ptr> parsedSets;
    for (auto && chartDir : dirsAndFiles){
        if (FileHelper::exists(chartDir,true)) {
            //directory
            ChartSet::Ptr chartSet=ParseChartDir(chartDir,canDelete);
            int numInSet=chartSet->GetNumCharts();
            if (numInSet < 1){
                LOG_INFO("no charts found in %s, skipping",chartDir);
            }
            else{
                numHandled+=numInSet;
                parsedSets.push_back(chartSet);
            }
        } else {
            LOG_ERROR("can only handle chart directories, not files: %s",chartDir);
        }
    }
    if (numHandled < 1){
        LOG_INFO("ReadChartDirs: no charts found");
        return numHandled;
    }
    {
        Synchronized l(lock);
        int numCharts=numRead;
        for (auto && chartSet : parsedSets){
            chartSet->SetReady();
            auto existing=chartSets.find(chartSet->GetKey());
            if (existing != chartSets.end() ){
                numCharts-=existing->second->GetNumCharts();
                if (numCharts < 0) numCharts=0;
            }
            chartSets[chartSet->GetKey()]=chartSet;
        }
        numRead=numCharts+numHandled;
        computeActiveSets();
    }    
    return numHandled;
}


//we have: <system name>-<chart code>-<year>-<edition>
//new scheme <system name>-<chart code>-<year>/<edition>-<update>
class NameAndVersion{
public:
    String name;
    String base;
    String version;
    int      iyear;
    int      iversion;
    int      iupdate;
    bool valid=false;
    NameAndVersion(){}
    /**
     * 
     * @param name the chart set name
     * @param infoVersion parsed version from chartInfo if not empty 2022/10-25
     */
    
    NameAndVersion(String name,String chartId,String infoVersion){
        this->name=name;
        this->base=chartId.empty()?name:chartId;
        version=infoVersion;
        StringHelper::replaceInline(version,"/","-");
        StringVector versionParts=StringHelper::split(version,"-");
        if (versionParts.size() != 3){
            return;
        }
        if (chartId.empty()){
            //take everything before a -year- in the name as base
            base=StringHelper::beforeFirst(base,"-"+versionParts[0]+"-");
            if (base.empty()) base=name;
        }
        iyear=atoi(versionParts[0].c_str());
        iversion=atoi(versionParts[1].c_str());
        iupdate=atoi(versionParts[2].c_str());
        LOG_INFO("parsed %s to base=%s,year=%d,version=%d, update=%d",
            name,base,iyear,iversion,iupdate);
        valid=true;
    }
    bool IsBetter(const NameAndVersion &other){
        if (base != other.base) return false;
        if (valid && ! other.valid) return true;
        if (!valid) return false;
        if (iyear > other.iyear) return true;
        if (other.iyear> iyear) return false;
        if (iversion > other.iversion) return true;
        if (iversion < other.iversion) return false;
        return iupdate > other.iupdate;
    }
};


class EnabledState{
public:
    NameAndVersion                  set;
    IBaseSettings::EnabledState   state;
    EnabledState(){}
    EnabledState(String setName,String chartSetId,String infoVersion,IBaseSettings::EnabledState   state)
        :set(setName,chartSetId,infoVersion),state(state){
        
    }
    bool IsBetter(const EnabledState &other){
        if (state == IBaseSettings::ENABLED && other.state != IBaseSettings::ENABLED) return true;
        if (other.state == IBaseSettings::ENABLED && state != IBaseSettings::ENABLED) return false;
        return set.IsBetter(other.set);
    }
};



int ChartManager::computeActiveSets(){
    LOG_INFO("ChartManager::ComputeActiveSets");
    ChartSetMap::iterator it;
    int numEnabled=0;
    //hold the found best versions for charts
    std::map<String,EnabledState> bestVersions;
    std::map<String,EnabledState>::iterator vit;
    for (it=chartSets.begin();it != chartSets.end();it++){
        ChartSet::Ptr set=it->second;
        IBaseSettings::EnabledState isActive=baseSettings->IsChartSetEnabled(set->GetKey());
        if (isActive == IBaseSettings::EnabledState::DISABLED){
            continue; //ignore sets disabled by settings
        }
        if (set->CanDelete()){
            EnabledState enabled(set->GetKey(),set->info->chartSetId, set->info->edition,isActive);
            if (isActive == IBaseSettings::EnabledState::ENABLED){
                //if we have at least one explicitely enabled set - keep this in mind
                LOG_INFO("found set %s being explicitely enabled,base=%s",
                        enabled.set.name.c_str(),
                        enabled.set.base.c_str());
                bestVersions[enabled.set.base]=enabled;
            }
            if (isActive == IBaseSettings::UNCONFIGURED){                
                vit=bestVersions.find(enabled.set.base);
                if (vit == bestVersions.end() || enabled.IsBetter(vit->second)){
                    bestVersions[enabled.set.base]=enabled;
                    LOG_INFO("found better version %s for base %s",enabled.set.name.c_str(),enabled.set.base.c_str());
                }
            }
            
        }
    }
    for (it=chartSets.begin();it != chartSets.end();it++){
        ChartSet::Ptr set=it->second;
        IBaseSettings::EnabledState isActive=baseSettings->IsChartSetEnabled(set->GetKey());
        if (isActive == IBaseSettings::EnabledState::DISABLED){
            set->SetEnabled(isActive,false);
            continue; //ignore sets disabled by settings
        }
        bool enabled=false;
        String disabledBy;
        if (set->CanDelete() && isActive == IBaseSettings::UNCONFIGURED){
             EnabledState enabledState(set->GetKey(),set->info->chartSetId,set->info->edition,isActive);
             vit=bestVersions.find(enabledState.set.base);
             if (vit == bestVersions.end() || vit->second.set.name==enabledState.set.name){
                 enabled=true;
             }
             else {
                 LOG_INFO("set %s is better then %s, disabling",vit->second.set.name.c_str(),enabledState.set.name.c_str());
                 disabledBy=vit->second.set.name;
             }
        }
        else{
            enabled=(isActive == IBaseSettings::UNCONFIGURED || 
                 isActive == IBaseSettings::ENABLED);        
        }
        set->SetEnabled(isActive,enabled,disabledBy);
        if (enabled) numEnabled++;
        else{
            if (setChanged) setChanged(set->GetKey());
        }
    }
    return numEnabled;
}

bool ChartManager::DeleteChartSet(const String &key)
{
    LOG_INFO("ChartManager::DeleteChartSet %s ", key);
    Synchronized l(lock);
    auto it = chartSets.find(key);
    if (it == chartSets.end())
    {
        LOG_ERROR("chart set %s not found for closing", key);
        throw AvException(FMT("chart set %s not found", key));
    }
    RemoveItem("chartSets", it->second);
    chartSets.erase(it);
    computeActiveSets();
    chartCache->CloseBySet(key);
    if (setChanged)
        setChanged(key);
    return true;
}

int ChartManager::ReadChartsInitial(const StringVector &dirs,bool canDelete){
    state=STATE_READING;
    LOG_INFOC("ChartManager: ReadChartsInitial");
    ChartSetMap::iterator it;
    int rt=ReadChartDirs(dirs,canDelete);
    LOG_INFOC("ChartManager: ReadChartsInitial returned %d",rt);
    state=STATE_READY;
    return rt;
}


int ChartManager::GetNumCharts(){
    Synchronized locker(lock);
    int rt=0;
    for (auto it=chartSets.begin();it != chartSets.end();it++){
        rt+=it->second->GetNumValidCharts();
    }
    return rt;
}


ChartManager::ManagerState ChartManager::GetState() {
    return state;
}

unsigned long ChartManager::GetCurrentCacheSizeKb(){
    return 0;
}
//makes only sense after StartCaches
unsigned long ChartManager::GetMaxCacheSizeKb(){
    return 0;
}


ChartSet::Ptr ChartManager::GetChartSet(String key){
    Synchronized locker(lock);
    ChartSetMap::iterator it=chartSets.find(key);
    if (it == chartSets.end()) return ChartSet::Ptr();
    return it->second;
}

class Coverage{
    uint8_t *points=NULL;
    int width;
    int height;
    bool covered=false;
    public:
        Coverage(int width, int height){
            this->width=width;
            this->height=height;
            points=new uint8_t[this->width*this->height];
            memset(points,0,width*height);
        }
        ~Coverage(){
            delete [] points;
        }
        void setArea(const Coord::PixelBox &ext){
            //fast check for full coverage
            if (ext.xmin <=0 && ext.xmax >= (width-1) && ext.ymin <=0 && ext.ymax >= (height-1)){
                covered=true;
                return;
            }
            if (covered) return;
            for (int i=std::max(0,ext.ymin);i<=std::min(ext.ymax,height-1);i++){
                if (i < 0 || i >= height) continue;
                    uint8_t *line=points+i*width;
                    int start=ext.xmin;
                    if (start < 0) start=0;
                    if (start >= width) continue;
                    int end=ext.xmax;
                    if (end >= width) end=width-1;
                    if (end < start) continue;
                    memset(line+start,1,end-start+1);
            }
        }
        bool isCovered() {
            if (covered) return true;
            covered=memchr(points,0,width*height)== NULL;
            return covered;
        }
};

static void fillChartList(WeightedChartList &chartList,ChartSet::Ptr chartSet,const Coord::TileBox &tileBox)
{
    Coord::World expand=Coord::pixelToWorld(50, tileBox.zoom);
    //expand our search box by 50px
    //to render texts, symbols and lights that have their center
    //in a different tile
    Coord::Extent tileExt=tileBox.getExpanded(expand,expand);    
    WeightedChartList found=chartSet->FindChartForTile(tileExt);
    for (auto it=found.begin();it != found.end();it++){
        it->tile=tileBox;
        chartList.add(*it);
    }
}
ChartSet::ExtentList ChartManager::GetChartSetExtents(const String &chartSetKey, bool includeSet)
{
    ChartSet::ExtentList rt;
    Synchronized l(lock);
    auto it = chartSets.find(chartSetKey);
    if (it == chartSets.end())
    {
        throw AvException("unknown chart set " + chartSetKey);
    }
    if (includeSet){
        rt.push_back(it->second->GetExtent().extent);
    }
    it->second->FillChartExtents(rt);
    return rt;
}

WeightedChartList ChartManager::FindChartsForTile(RenderSettings::ConstPtr renderSettingsPtr,const TileInfo &tile, bool allLower){
    LOG_DEBUG("findChartsForTile %s",tile.ToString());
    //add some border to the extent to potentially pick up
    //charts that have lights/symbols that we should draw partially
    Coord::TileBox tileBox=Coord::tileToBox(tile);
    WeightedChartList rt;
    {
        Synchronized l(lock);
        auto it = chartSets.find(tile.chartSetKey);
        if (it == chartSets.end())
        {
            throw RenderException(tile,"unknown chart set");
        }
        if (!it->second->IsActive())
        {
            throw RenderException(tile,FMT("chart set not active, state=%d,num=%d",(int)(it->second->GetState()),it->second->GetNumValidCharts()));
        }
        fillChartList(rt,it->second, tileBox);
        //try a shifted tileBox
        //this will handle charts corssing +/- 180° (or being close to...)
        //see comments in Coordinates.h
        //to avoid 3 rounds we just shift lower number tiles upward and higher
        //number tiles downward
        //we simply decide by the xmin of the tilebox <0
        size_t direct=rt.size(); 
        if (tileBox.xmin < 0){
            //shift up
            fillChartList(rt,it->second, tileBox.getShifted(tileBox.limits.worldShift(),0));
        }
        else{
            fillChartList(rt,it->second, tileBox.getShifted(- tileBox.limits.worldShift(),0));
        }
        if (rt.size() != direct){
            LOG_DEBUG("added %d shifted charts for tile %s",(rt.size()-direct),tileBox.toString());
        }
    }
    if (rt.size() == 0){
        return rt;
    }
    const RenderSettings * renderSettings=renderSettingsPtr.get(); //fast access
    //sort list by scale
    std::sort(rt.begin(),rt.end(),[](ChartInfoWithScale first, ChartInfoWithScale second){
        if (first.info->IsOverlay() != second.info->IsOverlay() ){
            return second.info->IsOverlay(); //if the second one is an overlay it is "bigger"
        }
        return (first.scale > second.scale);
    });
    //find the wanted zoom levels
    //min zoom gives us the zoom we use to display bigger scale charts (i.e. charts belonging to lower zoom levels)
    int minZoom=allLower?-1:tile.zoom-renderSettings->overZoom;
    int requestedZoom=tile.zoom;
    if (requestedZoom > MAX_ZOOM) requestedZoom=MAX_ZOOM;
    //maxUnder gives the zoom we used to display better (higher zoom, smaller scale charts)
    //if we did not already fully cover
    int maxUnder=tile.zoom+renderSettings->underZoom;
    int maxSoftUnder=maxUnder+renderSettings->softUnderZoom;
    ZoomLevelScales scales(renderSettings->scale); //TODO: keep this
    //the scales for a zoom level are all scales with
    //scale <= scales[zoom] && scale > scales[zoom+1]
    //maxzscale is the maximum scale we allow at all - ignore all with a scale >= maxzscale
    //if minzoom is < 0 we allow all scales
    double maxzScale=(minZoom<0)?std::numeric_limits<double>().max():scales.GetScaleForZoom(minZoom);
    //start scale is the upper limit for all scales that we consider (<=)
    //being part of the requested zoom level or higher zoom level
    double startScaleUpper=scales.GetScaleForZoom(requestedZoom); //Scale that we start detecting coverage (including)
    double startScaleLower=scales.GetScaleForZoom(requestedZoom+1); //Scale that we start detecting coverage (excluding) 
    //minuScale is the minimum scale we accept at all
    double minuScale=(maxUnder >= MAX_ZOOM)?0:scales.GetScaleForZoom(maxUnder+1);
    double minSoftUScale=(maxSoftUnder >= MAX_ZOOM)?0:scales.GetScaleForZoom(maxSoftUnder+1);
    const int MAX_SOFT_UNDER_CHARTS=4;  //avoid too many charts to be opend for the tile
                                        //we just keep this number of the highest scale charts
    int numSoftUnder=0;
    //first remove everything outside minZoom, maxSoftUnder/maxUnder
    avnav::erase_if(rt,[&numSoftUnder,maxzScale,minSoftUScale,minuScale](const ChartInfoWithScale &it){ 
        if (it.scale < minuScale){
            if (! it.info->HasSoftUnder()) return true;
            numSoftUnder++;
            if (numSoftUnder > MAX_SOFT_UNDER_CHARTS) return true;
        }
        if (it.scale < minSoftUScale) return true;
        if (it.scale >= maxzScale) return true;
        return false;
    });
    if (allLower){
        //for feature info requests we do not check any coverages
        return rt;
    }
    for (auto && c:rt){
        if (c.scale < minuScale){
            c.softUnder=true;
        }
    }
    //now we look at the coverage
    //we start with our requested zoom level and the go down (i.e. larger scale charts)
    //if we have covered in this phase we are done.
    //we always go down to the min zoom as our coverage is currently rather heuristic
    //as we only use the chart extent. It could easily happen that a chart report a bigger 
    //extent then it really covers
    //for this phase we have to go backward through the list
    //Afterwards we start with the charts with hiher zoom level (i.e. lower scales)
    //this time we need to iterate forward
    //smallest scales (highest zoom levels) are at the end
    //but we need to ensure to still have all charts of a particular
    //zoom level (scale) otherwise we get strange artifacts
    //we then delete all charts with a zoom that is one above our cover zoom
    Coverage coverage(Coord::TILE_SIZE,Coord::TILE_SIZE);
    int coverZoom=-2; //not set, -1 could be the min zoom
    for (auto it=rt.rbegin();it != rt.rend();it++){
        if (it->scale > startScaleLower && ! it->info->IsOverlay()){
            //add to coverage (if the chart is no overlay)
            Coord::PixelBox pex=Coord::worldExtentToPixel(it->info->GetExtent(),it->tile);
            coverage.setArea(pex);
        }
        /*
        //see comment above - charts can report a larger extent then they have
        if (coverage.isCovered()){
            coverZoom=scales.FindZoomForScale(it->scale);
            break;
        }
        */
    }
    if (coverage.isCovered()){
        coverZoom=minZoom;
    }
    if (coverZoom >= -1){
        if (coverZoom > requestedZoom) coverZoom=requestedZoom; //should not happen...
        //we now keep all charts betweem cover zoom and requestedZoom (including)
        //meaning scale > scales[requestedZoom+1] && scale <= scales[coverZoom]
        double maxScale=scales.GetScaleForZoom(coverZoom); //including
        double minScale=scales.GetScaleForZoom(requestedZoom+1); //excluding
        avnav::erase_if(rt, [minScale,maxScale](const ChartInfoWithScale &it)
                        { 
                            if (it.info->IsOverlay()) return false;
                            if (it.scale < minScale) return true;
                            if (it.scale >= maxScale) return true;
                            return false; });
    }
    else{
        //second try - now go to higher zoom levels
        for (auto it=rt.begin();it != rt.end();it++){
            //we don't nee our own zoom again
            if (it->scale <= startScaleLower && ! it->info->IsOverlay()){
                //add to coverage (if the chart is no overlay)
                Coord::PixelBox pex=Coord::worldExtentToPixel(it->info->GetExtent(),it->tile);
                coverage.setArea(pex);
            }
            if (coverage.isCovered()){
                coverZoom=scales.FindZoomForScale(it->scale);
                break;
            }
        } 
        if (coverZoom >= -1){
            //if something went wron we at least go to requested zoom
            if (coverZoom < requestedZoom) coverZoom=requestedZoom;
            //we now keep all charts between minZoom ... requestedZoom ...coverZoom
            double minScale=scales.GetScaleForZoom(coverZoom+1); //excluding
            //maxScale would be maxzScale - but they have been removed any way
            avnav::erase_if(rt, [minScale](const ChartInfoWithScale &it)
                        { 
                            if (it.info->IsOverlay()) return false;
                            if (it.scale <= minScale) return true;
                            return false; });
        }
    }
    LOG_DEBUG("returning %d charts",rt.size());
    return rt;
}


bool ChartManager::UpdateSettings(IBaseSettings::ConstPtr bs, RenderSettings::ConstPtr rs){
    bool changed=buildS52Data(rs);
    {
        Synchronized l(lock);
        if (bs->GetMD5() != baseSettings->GetMD5()){
            changed=true;
            baseSettings=bs;
            LOG_INFO("changed base settings, recompute active chart sets");
            computeActiveSets();
        }
    }
    return changed;
    
}

bool ChartManager::Stop(){
    LOG_INFO("stopping chart manager");
    houseKeeper->stop();
    chartCache->CloseAllCharts();
    LOG_INFO("stopping chart manager done");
    return true;
}
typedef std::unordered_set<ChartInfo::Ptr> ChartInfoSet;
void ChartManager::CloseDisabled(){
    LOG_INFO("ChartManager::CloseDisabled");
    StringVector closed;
    int numClosed=0;
    {
    Synchronized l(lock);
        for (auto it=chartSets.begin();it!=chartSets.end();it++){
            ChartSet::Ptr set=it->second;
            if (set->IsEnabled()) continue;
            numClosed+=chartCache->CloseBySet(set->GetKey());
            closed.push_back(set->GetKey());
        }
    }
    LOG_INFO("ChartManager::CloseDisabled finished and closed %d charts",numClosed);
    if (setChanged){
        for (auto &&cs:closed){
            setChanged(cs);
        }
    }
}

Chart::ConstPtr ChartManager::OpenChart(s52::S52Data::ConstPtr s52data,ChartInfo::Ptr chartInfo,bool doWait){
    Chart::ConstPtr chart;
    if (!chartInfo) return chart;
    if (!chartInfo->IsValid()) return chart;
    ChartSet::Ptr set;
    {
        Synchronized l(lock);
        auto it=chartSets.find(chartInfo->GetChartSetKey());
        if (it == chartSets.end()){
            throw FileException(chartInfo->GetFileName(),"no chart set found");
        }
        set=it->second;
    }
    try
    {
        chart = chartCache->GetChart(s52data, set, chartInfo->GetFileName(),doWait);
        if (!chart)
        {
            if (! doWait){
                return chart;
            }
            throw FileException(chartInfo->GetFileName(), "chart loading failed");
        }
        set->SetReopenStatus(chartInfo->GetFileName(), true);
    }
    catch (Exception &e)
    {
        set->SetReopenStatus(chartInfo->GetFileName(), false);
        throw;
    }
    return chart;
}

String ChartManager::GetCacheFileName(const String &fileName){
    return StringHelper::SanitizeString(fileName);
}


bool ChartManager::WriteChartInfoCache(const String &configFile){
    LOG_INFO("writing chart info cache");   
    ChartSetMap::iterator it;
    int numWritten=0;
#if 0
    for (it=chartSets.begin();it != chartSets.end();it++){
        ChartSet *set=it->second;
        if (!set->IsEnabled()) continue;
        if (set->DisabledByErrors()) continue; //if the set has errors we always will reparse
        LOG_DEBUG("writing chart info cache entry for set %s",set->GetKey());
        config->SetPath(wxT("/")+set->GetKey());
        config->Write("token",set->GetSetToken());
        ChartList::InfoList::iterator cit;
        ChartList::InfoList charts=set->GetAllCharts();
        for (cit=charts.begin();cit!=charts.end();cit++){
            ChartInfo *info=(*cit);
            LOG_DEBUG("writing chart info cache entry for %s",info->GetFileName());
            config->SetPath(wxT("/")+set->GetKey());
            config->SetPath(GetCacheFileName(info->GetFileName()));
            config->Write("valid",info->IsValid());
            if (info->IsValid()) {
                config->Write("scale", info->GetNativeScale());
                ExtentPI extent = info->GetExtent();
                config->Write("slat", extent.SLAT);
                config->Write("wlon", extent.WLON);
                config->Write("nlat", extent.NLAT);
                config->Write("elon", extent.ELON);
            }
            numWritten++;
        }
    }
    config->Flush();
#endif
    LOG_INFO("written %d chart info cache entries",numWritten);
    return true;
}
bool ChartManager::ReadChartInfoCache(const String &configFile){
    LOG_INFO("reading chart info cache");
    bool rt=false;
#if 0
    if (config == NULL){
        LOG_ERROR(wxT("chart cache file not open in read chart info cache"));
        return true;
    }
    ChartSetMap::iterator it;
    //we read in 2 rounds
    //first we only check if we can find all, in the second we really set the 
    //infos
    //this way we can safely read the charts completely if we fail in the first round
    for (int round = 0; round <= 1; round++) {
        for (it = chartSets.begin(); it != chartSets.end(); it++) {
            ChartSet *set = it->second;
            if (!set->IsEnabled()) continue;
            if (round >= 1  && set->IsParsing()) continue; //no need to try this again
            ChartSet::CandidateList::iterator cit;
            ChartSet::CandidateList charts = set->GetCandidates();
            config->SetPath(wxT("/")+set->GetKey());
            if (! config->HasEntry("token")){
                LOG_ERROR("missing entry token for chart set %s in chart info cache",set->GetKey());
                set->StartParsing();
                rt=true;
                continue;
            }
            String cacheToken;
            cacheToken=config->Read("token","");
            if (cacheToken != set->GetSetToken()){
                LOG_ERROR("set token has changed for set %s (cache=%s,current=%s)",
                        set->GetKey(),
                        cacheToken,set->GetSetToken()
                        );
                set->StartParsing();
                rt=true;
                continue;
            }
            for (cit = charts.begin(); cit != charts.end(); cit++) {
                ChartSet::ChartCandidate candidate = (*cit);
                ExtensionList::iterator it=extensions->find(candidate.extension);
                if (it == extensions->end()){
                    LOG_ERROR(wxT("unknown extension for chart file %s when reading chart info cache, skip"),candidate.fileName);
                    continue;
                }
                LOG_DEBUG("reading chart info cache entry for %s round %d", candidate.fileName,round);
                config->SetPath(wxT("/") + set->GetKey());
                config->SetPath(GetCacheFileName(candidate.fileName));
                if (! config->HasEntry("valid")){
                    LOG_ERROR("missing valid entry for %s in chart info cache", candidate.fileName);
                    set->StartParsing();
                    rt=true;
                    break;
                }
                bool valid=false;
                config->Read("valid",&valid);
                if (valid) {
                    if (!config->HasEntry("scale")) {
                        LOG_ERROR("missing scale entry for %s in chart info cache", candidate.fileName);
                        set->StartParsing();
                        rt=true;
                        break;                     
                    }
                    if (!config->HasEntry("slat")) {
                        LOG_ERROR("missing slat entry for %s in chart info cache", candidate.fileName);
                        set->StartParsing();
                        rt=true;
                        break;                        
                    }
                    if (!config->HasEntry("wlon")) {
                        LOG_ERROR("missing wlon entry for %s in chart info cache", candidate.fileName);
                        set->StartParsing();
                        rt=true;
                        break;                    
                    }
                    if (!config->HasEntry("nlat")) {
                        LOG_ERROR("missing nlat entry for %s in chart info cache", candidate.fileName);
                        set->StartParsing();
                        rt=true;
                        break; 
                    }
                    if (!config->HasEntry("elon")) {
                        LOG_ERROR("missing elon entry for %s in chart info cache", candidate.fileName);
                        set->StartParsing();
                        rt=true;
                        break;
                    }
                }
                if (round != 1) continue;
                ChartInfo *info=new ChartInfo(it->second.classname,candidate.fileName);
                if (valid){
                    long nativeScale = -1;
                    config->Read("scale", &nativeScale);
                    ExtentPI extent;
                    config->Read("slat", &extent.SLAT);
                    config->Read("wlon", &extent.WLON);
                    config->Read("nlat", &extent.NLAT);
                    config->Read("elon", &extent.ELON);
                    info->FromCache(nativeScale,extent);
                    numRead++;
                }
                else {
                    LOG_ERROR(wxT("adding invalid chart entry %s to set %s"),candidate.fileName,set->GetKey());
                    set->AddError(candidate.fileName);
                }
                set->AddChart(info);
            }
        }
    }
    int numParsing=0;
    for (it=chartSets.begin();it != chartSets.end();it++){
        if (it->second->IsParsing()){
            numParsing++;
            continue;
        }
        it->second->SetZoomLevels();
        if (it->second->IsEnabled()) it->second->SetReady();
    }
    if (! rt) state=STATE_READY;
    LOG_INFO(String::Format("read %d chart info cache entries, %d sets still need parsing",numRead,numParsing));
#endif    
    return rt;
}

ChartSetInfoList ChartManager::ListChartSets(){
    ChartSetInfoList rt;
    Synchronized l(lock);
    for (auto it=chartSets.begin();it != chartSets.end();it++){
        if (!it->second->IsActive() || it->second->DisabledByErrors()) continue;
        rt.push_back(it->second->info);
    }
    return rt;
}

String ChartManager::GetChartSetSequence(const String &chartSetKey){
    Synchronized l(lock);
    auto it=chartSets.find(chartSetKey);
    if (it == chartSets.end()) return "";
    s52::S52Data::ConstPtr s52data=GetS52Data();
    if (!s52data) return it->second->GetSetToken();
    return s52data->getMD5().ToString()+"-"+it->second->GetSetToken();
}

int ChartManager::RemoveUnverified()
{
    int rt = 0;
    StringVector setsToRemove;
    {
        Synchronized l(lock);
        for (auto it = chartSets.begin(); it != chartSets.end(); it++)
        {
            int sr = it->second->RemoveUnverified();
            if (sr > 0 && it->second->GetNumCharts() == 0)
            {
                LOG_INFO("remove chart set %s that has no charts any more", it->second->info->name);
                setsToRemove.push_back(it->second->GetKey());
            }
        }
        for (auto it = setsToRemove.begin(); it != setsToRemove.end(); it++)
        {
            chartSets.erase(*it);
        }
    }
    // outside lock
    for (auto it = setsToRemove.begin(); it != setsToRemove.end(); it++)
    {
        chartCache->CloseBySet(*it);
    }
    return setsToRemove.size();
}

void ChartManager::HouseKeeper::run(){
    while(true){
        if(waitMillis(intervallMs)) break;
        int systemKb, ourKb;
        SystemHelper::GetMemInfo(&systemKb,&ourKb);
        int res=cache->HouseKeeping();
        if (res > 0){
            int ourKbN;
            SystemHelper::GetMemInfo(&systemKb,&ourKbN);
            LOG_INFO("Housekeeping closed %d charts, memory system %d kb, our %d kb, diff %dkb",res,systemKb,ourKbN,ourKb-ourKbN);
        }
    }
}
s52::S52Data::ConstPtr ChartManager::GetS52Data() const{
    return s52data;
}
/**
 * rebuild s52 data if needed
*/
bool ChartManager::buildS52Data(RenderSettings::ConstPtr s){
    bool hasOld=false;
    MD5Name oldMd5;
    {
        Synchronized locker(s52lock);
        int sequence=0;
        if (s52data){
            if (s52data->getMD5() == s->GetMD5()){
                LOG_INFO("no need to rebuild s52 data: render settings unchanged");
                return false;
            }
            hasOld=true;
            oldMd5=s52data->getMD5();
            sequence=s52data->getSequence()+1;
            RemoveItem("s52data");
        }
        s52::S52Data::Ptr newS52Data=std::make_shared<s52::S52Data>(s,fontFile,sequence); 
        LOG_INFO("building s52 data with dir %s",s57Dir);
        newS52Data->init(s57Dir);
        s52data=newS52Data;
        AddItem("s52data",s52data);
    }
    if (hasOld && chartCache) chartCache->CloseByMD5(oldMd5);
    if (settingsChanged){
        settingsChanged(s52data);
    } 
    return true;  
}

void ChartManager::registerSetChagend(ChartManager::SetChangeFunction f){
    setChanged=f;
}
void ChartManager::registerSettingsChanged(ChartManager::SettingsChangeFunction f){
    settingsChanged=f;
}
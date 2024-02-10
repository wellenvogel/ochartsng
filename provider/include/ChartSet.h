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

#ifndef CHARTSET_H
#define CHARTSET_H
#include "ChartSetInfo.h"
#include "SimpleThread.h"
#include "ChartInfo.h"
#include "StatusCollector.h"
#include "MD5.h"
#include <vector>
#include <map>
#include <memory>
#include <CacheHandler.h>
#include <atomic>
class CacheReaderWriter;
class ChartList;
class ChartSet : public StatusCollector{
public:
    using InfoList=std::vector<ChartInfo::Ptr>;
    using Ptr=std::shared_ptr<ChartSet>;
    using ConstPtr=std::shared_ptr<const ChartSet>;
    class ExtentList: public std::vector<Coord::Extent>{
        public:
        using std::vector<Coord::Extent>::vector;
        String setHash;
        int setSequence=0;
    };
    const int MAX_ERRORS_RETRY=10; //stop retrying after that many errors
    typedef enum{
        STATE_INIT,
        STATE_DISABLED,
        STATE_READY,
    } SetState;

    class ExtentInfo{
        public:
            Coord::Extent extent;
            int minScale=0;
            int maxScale=0;
    };
    class ChartCounts{
        public:
        int valid=0;
        int ignored=0;
    } ;

    ChartSetInfo::Ptr   info;
    ChartSet(ChartSetInfo::Ptr info, bool canDelete=false);
    virtual             ~ChartSet(){}
    const String        GetKey() const;
    bool                IsActive(){return (state == STATE_READY && (numValidCharts>0) && ! DisabledByErrors());}
    /**
     * change the enabled/disabled state
     * @param enabled
     * @return true if state has changed
     */
    bool                SetEnabled(bool enabled=true,String disabledBy=String());
    bool                IsEnabled(){return state != STATE_DISABLED;}
    void                SetReopenStatus(String fileName,bool ok);
    void                SetReady();
    
    virtual bool        LocalJson(StatusStream &stream) override;
    bool                DisabledByErrors();
    void                AddChart(ChartInfo::Ptr info);
    WeightedChartList   FindChartForTile(const Coord::Extent &tileExtent);
    int                 GetNumValidCharts(){return numValidCharts;}
    size_t              GetNumCharts();
    String              GetSetToken();
    bool                ShouldRetryReopen(){return reopenErrors < 2;}
    String              GetChartKey(Chart::ChartType type, const String &fileName);
    ChartInfo::Ptr      FindInfo(const String &fileName);
    bool                CanDelete() const {return canDelete;}
    SetState            GetState() const{return state;}
    StringVector        GetFailedChartNames(int maxErrors=-1);   
    ExtentInfo          GetExtent();
    int                 RemoveUnverified();
    void                FillChartExtents(ExtentList &extents);

    
    
private:
    std::atomic<SetState> state={STATE_INIT};
    String              dataDir;
    std::mutex          lock;
    bool                canDelete;
    String              disabledBy;
    std::atomic<int>    openErrors={0}; //consecutive openErrors, disable set when limit reached
    std::atomic<int>    openErrorsGlob={0}; //openErrors global
    std::atomic<int>    reopenErrors={0}; //errors during reopen
    std::atomic<int>    reopenOk={0}; //consecutive reopen ok
    std::atomic<int>    numValidCharts={0}; 
    std::atomic<int>    numIgnoredCharts={0};
    std::atomic<int>    numCharts={0};
    /**
     * compute hash, numbers
    */
    void                computeHash();
    InfoList            chartList;
    std::atomic<int>    minScale={std::numeric_limits<int>().max()};
    std::atomic<int>    maxScale={std::numeric_limits<int>().min()}; 
    MD5Name             hash;
    Coord::Extent       boundings;
};


#endif /* CHARTSET_H */


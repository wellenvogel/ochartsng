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
#include "ChartList.h"

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
    typedef std::shared_ptr<ChartSet> Ptr;
    typedef std::shared_ptr<const ChartSet> ConstPtr;
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

    ChartSetInfo::Ptr   info;
    ChartSet(ChartSetInfo::Ptr info, bool canDelete=false);
    virtual             ~ChartSet(){}
    const String        GetKey();
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

    
    
private:
    std::atomic<SetState> state={STATE_INIT};
    ChartList::Ptr      charts;
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
};


#endif /* CHARTSET_H */


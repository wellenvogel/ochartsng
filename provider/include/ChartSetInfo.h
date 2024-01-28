/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Set Info 
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

#ifndef CHARTSETINFO_H
#define CHARTSETINFO_H


#include <vector>
#include "ItemStatus.h"
#include "Types.h"

typedef enum {
    SHOW_NEVER,
    SHOW_ONCE,
    SHOW_SESSION,
    UNKNOWN    
} ShowMode;
class ChartSetInfo: public ItemStatus{
public:
    static constexpr const char *LEGACY_CHARTINFO="Chartinfo.txt"; 
    static constexpr const char *NEW_CHARTINFO="ChartList.XML";
    typedef std::shared_ptr<ChartSetInfo> Ptr;
    typedef std::shared_ptr<const ChartSetInfo> ConstPtr;
    ChartSetInfo(){
        eulaMode=SHOW_NEVER;
        chartInfoMode=SHOW_NEVER;
        infoParsed=false;
    }
    String                name;
    ShowMode              eulaMode=UNKNOWN;
    ShowMode              chartInfoMode=UNKNOWN;
    String                validTo;
    String                edition; //2022/10-25 (new), 2020-21 - old
    StringVector          eulaFiles;
    String                dirname;
    String                title;
    String                userKey;
    NameValueMap          chartKeys;
    String                chartSetId; //ID used for shop (chartId)
    bool                  infoParsed=false;
    long                  mtime=0;
    
    String                ToString();
    virtual               void ToJson(StatusStream &stream) override;
    
    static ChartSetInfo::Ptr   ParseChartInfo(String chartDir,String key);
    private:
    void SetChartInfoMode(const String &, bool overWrite=true);
    void SetEulaMode(const String &, bool overWrite=true);

};

typedef std::vector<ChartSetInfo::ConstPtr> ChartSetInfoList;

#endif /* CHARTSETINFO_H */


/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Factory
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2010 by Andreas Vogel   *
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
#include "ChartFactory.h"
#include "OESUChart.h"
#include "Logger.h"

static std::map<String,Chart::ChartType> chartTypes={
    {"oesu",Chart::OESU},
    {"oesenc",Chart::OESENC},
    {"oernc",Chart::ORNC},
    {"s57",Chart::S57},
    {"senc",Chart::SENC},
    {"osenc",Chart::SENC}
};


Chart::ChartType ChartFactory::GetChartType(const String& fileName) const{
    auto it=chartTypes.find(StringHelper::toLower(FileHelper::ext(fileName)));
    if (it != chartTypes.end()) return it->second;
    return Chart::UNKNOWN;
}

Chart::Ptr ChartFactory::createChart(ChartSet::Ptr chartSet,const String &fileName)
{
    Chart::ChartType type=GetChartType(fileName);
    switch (type){
        case Chart::OESU:
        case Chart::OESENC:
            return Chart::Ptr(new OESUChart(chartSet->GetKey(),type, fileName));
        case Chart::S57:
            return Chart::Ptr(new S57Chart(chartSet->GetKey(),type, fileName));
        case Chart::SENC:
            return Chart::Ptr(new SENCChart(chartSet->GetKey(),type, fileName));
        case Chart::UNKNOWN:
            throw AvException("unable to determine chart type for "+fileName);
    }    
    return Chart::Ptr(new Chart(chartSet->GetKey(),type, fileName));
}


ChartFactory::ChartFactory(OexControl::Ptr oexcontrol){
    this->oexcontrol=oexcontrol;
    const char *tc=getenv("AVNAV_TEST_KEY");
    if (tc != NULL) testKey=tc;
}
InputStream::Ptr ChartFactory::OpenChartStream(Chart::ConstPtr chart, 
            ChartSet::Ptr chartSet,
            const String fileName,
            bool headerOnly){
                if (! FileHelper::exists(fileName)){
                    throw OpenException(FMT("chart file %s not found", fileName));
                }
                OexControl::OexCommands command=headerOnly?chart->OpenHeaderCmd():chart->OpenFullCmd();
                if (command == OexControl::CMD_UNKNOWN){
                    return FileHelper::openFileStream(fileName,4096);    
                }
                String key=chartSet->GetChartKey(chart->GetType(), fileName);
                if (! testKey.empty() && key == testKey ){
                    return FileHelper::openFileStream(fileName,4096);
                }
                LOG_DEBUG("opening chart stream for %s with key %s",fileName.c_str(),key.c_str());
                return oexcontrol->SendOexCommand(command,fileName,key);
            }

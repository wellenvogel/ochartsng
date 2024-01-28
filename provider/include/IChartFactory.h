/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Factory
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
#ifndef _ICHARTFACTORY_H
#define _ICHARTFACTORY_H
#include <vector>
#include <memory>
#include "Chart.h"
#include "Types.h"
#include "Testing.h"
#include "FileHelper.h"
#include "ChartSet.h"
class IChartFactory{
    public:
    typedef std::shared_ptr<IChartFactory> Ptr;
    DECL_EXC(AvException,OpenException);
    virtual ~IChartFactory(){}
    virtual Chart::ChartType GetChartType(const String &fileName) const =0;
    virtual Chart::Ptr createChart(ChartSet::Ptr chartSet,const String &fileName)=0;
    virtual InputStream::Ptr OpenChartStream(Chart::ConstPtr chart, 
            ChartSet::Ptr chartSet,
            const String fileName,
            bool headerOnly=false)=0;
};
#endif
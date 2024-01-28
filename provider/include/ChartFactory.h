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
#ifndef _CHARTFACTORY_H
#define _CHARTFACTORY_H
#include "IChartFactory.h"
#include "OexControl.h"
class TChartFactory;

class ChartFactory : public IChartFactory{
    public:
    ChartFactory(OexControl::Ptr oexcontrol);
    virtual ~ChartFactory(){}
    virtual Chart::Ptr createChart(ChartSet::Ptr chartSet,const String &fileName) override;
    virtual InputStream::Ptr OpenChartStream(Chart::ConstPtr chart, 
            ChartSet::Ptr chartSet,
            const String fileName,
            bool headerOnly=false) override;
    virtual Chart::ChartType GetChartType(const String &fileName) const override;
    protected:
    OexControl::Ptr oexcontrol;
    String testKey;

    friend class TChartFactory;

};
#endif
/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Info
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
#ifndef _CHART_H
#define _CHART_H
#include <map>
#include "Types.h"
#include "Testing.h"
#include "Tiles.h"
#include "DrawingContext.h"
#include <memory>
#include "FileHelper.h"
#include "Coordinates.h"
#include "RenderContext.h"
#include "S52Data.h"
#include <numeric>
#include "ObjectDescription.h"
#include "OexControl.h"


class Chart
{
public:
    DECL_EXC(FileException, InvalidChartException); 
    DECL_EXC(FileException, ValueException); // invalid value in input
    DECL_EXC(FileException, VersionException);
    DECL_EXC(FileException, ExpiredException);
    typedef std::shared_ptr<Chart> Ptr;
    typedef std::shared_ptr<const Chart> ConstPtr;
    typedef enum
    {
        OESU,
        OESENC,
        SENC,
        S57,
        ORNC,
        RNC,
        UNKNOWN
    } ChartType;
    typedef enum
    {
        OK,
        DECRYPT_ERROR,
        MISSING_KEY,
        OTHER_ERROR
    } InitResult;
    typedef enum
    {
        RFAIL, // not redendered
        ROK,   // normal render
        RFULL  // rendered complete tile- no need to render lower tiles (only for raster charts)
    } RenderResult;

    Chart(const String &setKey, ChartType type, const String &fileName)
    {
        this->type = type;
        this->fileName = fileName;
        this->setKey = setKey;
    }
    TESTVIRT ~Chart() {}
    ChartType GetType() const { return type; }
    String GetFileName() const { return fileName; }
    String GetSetKey() const { return setKey; }
    int GetNativeScale() const;
    Coord::Extent GetChartExtent() const;
    bool HeaderOnly() const { return headerOnly; }
    virtual bool ReadChartStream(InputStream::Ptr input, s52::S52Data::ConstPtr s52data, bool headerOnly = false) { return false; }
    /**
     * prepareRender can be called from a different thread
     * while others still are rendering
     * so internally atomic operations only!
    */
    virtual bool PrepareRender(s52::S52Data::ConstPtr s52data){
        if (s52data) md5=s52data->getMD5();
        return true;
    }
    virtual int getRenderPasses() const {return 1;}
    virtual RenderResult Render(int pass,RenderContext & context,DrawingContext &out, const Coord::TileBox &box) const;
    //we use a (small) tile box that contains our tolerance
    //and the click point being the mid
    virtual ObjectList FeatureInfo(RenderContext & context,DrawingContext &drawing,const Coord::TileBox &box, bool overview) const;
    virtual MD5Name GetMD5() const { return md5;}
    virtual bool IsIgnored() const { return false;}
    virtual bool SoftUnder() const { return false;}
    virtual void LogInfo(const String &prefix) const {}
    virtual OexControl::OexCommands OpenHeaderCmd() const{ return OexControl::OexCommands::CMD_UNKNOWN;}
    virtual OexControl::OexCommands OpenFullCmd() const{ return OexControl::OexCommands::CMD_UNKNOWN;}

protected:
    String fileName;
    ChartType type;
    String setKey;
    bool headerOnly = false;
    Coord::Extent extent;
    int nativeScale = -1;
    MD5Name md5;
};



#endif
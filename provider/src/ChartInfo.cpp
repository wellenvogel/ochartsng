/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Chart Info
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
#include <wait.h>
#include "ChartInfo.h"
#include "Tiles.h"
#include "Logger.h"
#include "StringHelper.h"
#include "FileHelper.h"
#include "Chart.h"
#include "OexControl.h"
#include "Coordinates.h"
#include <algorithm>
#include <map>
#include <cmath>


static const double WGS84_semimajor_axis_meters       = 6378137.0;           // WGS84 semimajor axis
static const double mercator_k0                       = 0.9996;
static const double WGSinvf                           = 298.257223563;       /* WGS84 1/f */


ZoomLevelScales::ZoomLevelScales(double scaleLevel) {
    double resolution=BASE_MPP * scaleLevel;
    //OpenCPN uses some correction for the major axis
    //this is no big problem anyway but we like to be consistent
    zoomMpp[0] = 2*M_PI*WGS84_semimajor_axis_meters*mercator_k0/Coord::TILE_SIZE;
    for (int i = 1; i <= MAX_ZOOM; i++) {
        zoomMpp[i] = zoomMpp[i - 1] / 2;
    }
    for (int i = 0; i <= MAX_ZOOM; i++) {
        zoomScales[i] = zoomMpp[i] / resolution;
    }
}
double ZoomLevelScales::GetScaleForZoom(int zoom) const{
    if (zoom < 0) zoom=0;
    if (zoom > MAX_ZOOM ) zoom=MAX_ZOOM;
    return zoomScales[zoom];
}
double ZoomLevelScales::GetMppForZoom(int zoom) const{
    if (zoom < 0) zoom=0;
    if (zoom > MAX_ZOOM ) zoom=MAX_ZOOM;
    return zoomMpp[zoom];
}
int ZoomLevelScales::FindZoomForScale(double scale) const{
    //currently we simply take the next lower zoom
    //can be improved
    for (int i=0;i<=MAX_ZOOM;i++){
        if (zoomScales[i]<scale) {
            if (i > 0) return i-1;
            return i;
        }
    }
    return MAX_ZOOM;
}
void ChartInfo::CheckOverlay(){
    //      Get the "Usage" character
    String cname = FileHelper::fileName(filename);
    if (cname.size() >= 3){
        isOverlay=((cname[2] == 'L') || (cname[2] == 'A'));
    }
}

ChartInfo::ChartInfo(const Chart::ChartType &type,const String &fileName,int nativeScale, Coord::Extent extent,bool ignore){
    this->filename=fileName;
    this->type=type;
    this->nativeScale=nativeScale;
    this->extent=extent;
    checkOrSetFileData();
    this->state=READY;
    this->ignore=ignore;
    CheckOverlay();
}
ChartInfo::ChartInfo(const Chart::ChartType &type,const String &fileName,int nativeScale, Coord::Extent extent, int64_t fileSize, int64_t fileTime,bool ignore){
    this->filename=fileName;
    this->type=type;
    this->nativeScale=nativeScale;
    this->extent=extent;
    this->fileSize=fileSize;
    this->fileTime=fileTime;
    this->state=NEEDS_VER;
    this->ignore=ignore;
    CheckOverlay();
}
ChartInfo::ChartInfo(const Chart::ChartType &type,const String &fileName){
    this->filename=fileName;
    this->type=type;
    this->nativeScale=-1;
    this->state=UNKNOWN;
    CheckOverlay();
    numErrors=1;
}

void ChartInfo::AddErrors(int num){
    numErrors+=num;
}

bool ChartInfo::VerifyChartFileName(const String &fileName){
    bool fnChanged=fileName != this->filename;
    if (fnChanged) this->filename=fileName;
    bool rt=checkOrSetFileData() && ! fnChanged;
    if (rt) state=READY;
    return rt;
}
bool ChartInfo::checkOrSetFileData(){
    FileHelper::FileInfo info=FileHelper::getFileInfo(filename);
    if (! info.existing){
        fileTime=-1;
        fileSize=-1;
        return false;
    }
    if (fileTime != info.time || fileSize != info.size){
        fileTime=info.time;
        fileSize=info.size;
        return false;
    }
    return true;
}
ChartInfo::~ChartInfo() {
}


bool ChartInfo::IsRaster() const{
    return type == Chart::RNC || type == Chart::ORNC;
}
bool ChartInfo::IsOverlay() const {
  return isOverlay && ! IsRaster();
}




String ChartInfo::ToString() const{
    return StringHelper::format("Chart set=%s,file=%s,valid=%s,scale=%d,%s, %s",
            chartSetKey,
            filename,PF_BOOL(state == READY),nativeScale,
                Coord::LLBox::fromWorld(extent).toString(),
                extent.toString()
                );
}

int ChartInfo::HasTile(const Coord::Extent &tileExt) const{
    if (state != READY) return 0;
    return tileExt.intersects(extent)?nativeScale:0;
}




#! /usr/bin/env python3
'''******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  python senc support
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2024 by Andreas Vogel   *
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
'''
import struct
import sys
import getopt
import os
import csv
import sqlite3
import senc.senc as senc
from senc.s57 import S57Mappings






def warn(txt,*args):
    print(("WARNING: "+txt)%args)

def readGeometryPoint(endian,gv):
    lon,lat=struct.unpack("%sdd"%endian,gv)
    return senc.Point(lon,lat)
def parseGeometry(gv,txt) -> senc.GeometryBase:
    '''
    see https://github.com/locationtech/jts/blob/master/modules/core/src/main/java/org/locationtech/jts/io/WKBReader.java
    :param gv:
    :param txt:
    :return:
    '''
    if len(gv) < 5:
        raise Exception("invalid geometry %s"%txt)
    type=gv[0]
    endian="<" if type == 1 else ">"
    mode,=struct.unpack("%sxI%dx"%(endian,len(gv)-5),gv)
    hasZ=(mode & 0x80000000) != 0
    mode=mode & 0xffff
    if mode == 1:
        if hasZ:
            lon,lat,d=struct.unpack("%s5xddd"%endian,gv)
            return senc.SoundingGeometry(senc.SoundingPoint(lon,lat,d))
        else:
            lon,lat=struct.unpack("%s5xdd"%endian,gv)
            return senc.PointGeometry(senc.Point(lon,lat))
    if mode == 2:
        if hasZ:
            raise Exception("3rd coordinate not supported for line strings in %s"%txt)
        num,=struct.unpack("%sI"%endian,gv[5:9])
        points=[]
        start=9
        for i in range(0,num):
            points.append(readGeometryPoint(endian,gv[start:start+16]))
            start+=16
        return senc.LineGeometry(points)
    if mode == 3:
        #polygons
        idx=5
        numRings,=struct.unpack("%sI"%endian,gv[idx:idx+4])
        idx+=4
        if numRings <= 0:
            return senc.PolygonGeometry([])
        if numRings > 1:
            raise Exception("cannot currently handle polygons with holes in %s",txt)
        numCoord,=struct.unpack("%sI"%endian,gv[idx:idx+4])
        idx+=4
        ring=[]
        for i in range(0,numCoord):
            lon,lat=struct.unpack("%sdd"%endian,gv[idx:idx+16])
            idx+=16
            ring.append(senc.Point(lon,lat))
        return senc.PolygonGeometry(ring)
    raise Exception("unknown geometry %d in %s"%(mode,txt))

def objectToSenc(wh: senc.SencFile,name,dbrow,gcol):
    attrs=[]
    geometry=None
    for k,v in dict(dbrow).items():
        if v is None:
            continue
        if k == gcol:
            geometry=parseGeometry(v,name)
        else:
            attrs.append(senc.FAttr(k,v))
    if geometry is None:
        warn("no geometry found for %s",name)
        return False
    wh.addFeature(name,attrs,geometry)
    return True


def writeTableToSenc(wh:senc.SencFile,cur,name,gcol):
    res=cur.execute("select * from %s"%name)
    for dbrow in res:
        objectToSenc(wh,name,dbrow,gcol)

def writeSoundings(wh:senc.SencFile,cur,name,gcol):
    res=cur.execute("select * from %s"%name)
    soundings=[]
    for dbrow in res:
        row=dict(dbrow)
        geometry=parseGeometry(row[gcol],"soundings")
        if type(geometry) is not senc.SoundingGeometry:
            raise Exception("invalid geometry %s for sounding"%type(geometry))
        soundings.append(geometry.point)
        if len(soundings) >= 100:
            wh.addSoundings(soundings)
            soundings=[]
    if len(soundings) > 0:
        wh.addSoundings(soundings)


if __name__ == '__main__':
    s57dir=os.path.join(os.path.dirname(__file__),"..","provider","s57static")
    optlist,args=getopt.getopt(sys.argv[1:],"s:")
    #TODO options
    s57mappings=S57Mappings(s57dir)
    s57mappings.parse()
    if len(args) < 0:
        print("usage: %s outname [sqlitedb]"%sys.argv[0])
        sys.exit(1)
    if len(args) < 2:
        header=senc.SencHeader(
            nw=senc.Point(12.5,54.7),
            se=senc.Point(14.5,53.7),
            edition=99,
            scale=30000,
            name="test"
        )
        wh=senc.SencFile(s57mappings,args[0],header)
        atts=[
            senc.FAttr("BOYSHP",4),
            senc.FAttr("COLOUR","3,1"),
            senc.FAttr("COLPAT",2),
            senc.FAttr("OBJNAM","Greifswald")
        ]
        #Ansteuerung GW
        wh.addFeature("BOYSAW",atts,
                      senc.PointGeometry(senc.Point(13.5015,54.1631667)))
        wh.close()
        print("created %s"%args[0])
        sys.exit(0)
    print("opening %s"%args[1])
    db=sqlite3.connect(args[1])
    db.row_factory = sqlite3.Row
    print("parsing geometries...")
    curtables = db.cursor()
    cur = db.cursor()
    descriptions=curtables.execute("select * from geometry_columns")
    extent=senc.Extent()
    for drow in descriptions:
        row=dict(drow)
        table=row["f_table_name"]
        col=row['f_geometry_column']
        gtype=row['geometry_type']
        dim=row['coord_dimension']
        KNOWN_G=[1,2,3]
        if int(gtype) not in KNOWN_G:
            print("ignoring table %s with unknown geometry type %d"%(table,gtype))
            continue
        if int(dim) != 2 and int(dim) != 3:
            raise Exception("invalid geometry dimension %d for %s"%(dim,table))
        if row['geometry_format'] != 'WKB':
            raise Exception("invalid geometry format %s in %s, expected WKB"%(row['geometry_format'],table))
        print("parsing %s..."%table)
        vrows=cur.execute("select %s from %s"%(col,table))
        for vrow in vrows:
            if vrow[0] is None:
                continue
            geometry=parseGeometry(vrow[0],table)
            geometry.extend(extent)
    print("parsed extent: %s"%str(extent))
    header=senc.SencHeader(
        nw=extent.nw,
        se=extent.se,
        name="sqltest",
        edition=100,
        scale=30000)
    wh=senc.SencFile(s57mappings,args[0],header)
    cur = db.cursor()
    tables=cur.execute("SELECT f_table_name,f_geometry_column FROM geometry_columns")
    for tablerow in tables.fetchall():
        table=tablerow[0]
        print("reading table %s"%table)
        if table == "soundg":
            writeSoundings(wh,cur,table,tablerow[1])
            continue
        objd=s57mappings.objectClasses.get(table.upper())
        if objd is None:
            print("ignoring unknown table %s"%table)
            continue
        writeTableToSenc(wh,cur,table,tablerow[1])
    print("wrote %d features to %s"%(wh.featureId,args[0]))

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
from senc.s57 import S57Att, S57Mappings, S57OCL






def warn(txt,*args):
    print(("WARNING: "+txt)%args)

def parsePointGeometry(gv,txt):
    if len(gv) < 5:
        raise Exception("invalid point geometry %s"%txt)
    type=gv[0]
    endian="<" if type == 1 else ">"
    mode,=struct.unpack("%sxI%dx"%(endian,len(gv)-5),gv)
    if mode != 1:
        raise Exception("expected point geometry (1) got %d"%mode)
    lon,lat=struct.unpack("%s5xdd"%endian,gv)
    return senc.Point(lon,lat)
def pointObjectToSenc(wh: senc.SencFile,name,dbrow):
    attrs=[]
    point=None
    for k,v in dict(dbrow).items():
        if v is None:
            continue
        if k == 'GEOMETRY':
            point=parsePointGeometry(v,name)
        else:
            attrs.append(senc.FAttr(k,v))
    if point is None:
        warn("no geometry found for %s",name)
        return False
    wh.addFeature(name,attrs,senc.PointGeometry(point))
    return True


def writeTableToSenc(wh:senc.SencFile,cur,name):
    res=cur.execute("select * from %s"%name)
    for dbrow in res:
        pointObjectToSenc(wh,name,dbrow)



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
    db=sqlite3.connect(args[1])
    db.row_factory = sqlite3.Row
    header=senc.SencHeader(
        nw=senc.Point(0,60),
        se=senc.Point(20,20),
        name="sqltest",
        edition=100,
        scale=30000)
    wh=senc.SencFile(s57mappings,args[0],header)
    cur = db.cursor()
    tables=cur.execute("SELECT name FROM sqlite_master WHERE type='table';")
    for tablerow in tables.fetchall():
        table=tablerow[0]
        objd=s57mappings.objectClasses.get(table.upper())
        if objd is None:
            print("ignoring unknown table %s"%table)
            continue
        writeTableToSenc(wh,cur,table)
    print("wrote %d features to %s"%(wh.featureId,args[0]))

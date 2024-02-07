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
import json
import struct
import os
import sqlite3
from . import senc
from  .s57 import  S57Mappings






def warn(txt,*args):
    print(("WARNING: "+txt)%args)
def err(txt,*args):
    print(("ERROR: "+txt)%args)
    raise Exception(txt%args)
def log(txt,*args):
    print(("LOG: "+txt)%args)

class GVReader:
    def __init__(self,gv,txt,start=0):
        self.gv=gv
        self.txt=txt
        self.start=start
        if len(gv) < 5:
            raise Exception("invalid geometry %s"%txt)
        self.type=gv[0]
        self.start+=1
        if self.type == 1:
            self.guint="<I"
        else:
            self.guint=">I"
        self.mode=self.uint()
        self.hasZ=(self.mode & 0x80000000) != 0
        self.mode=self.mode & 0xffff
        if self.type == 1:
            self.gpoint="<ddd" if self.hasZ else "<dd"
        else:
            self.gpoint=">ddd" if self.hasZ else ">dd"
    def uint(self):
        v,=struct.unpack(self.guint,self.gv[self.start:self.start+4])
        self.start+=4
        return v
    def readGeometryPoint(self):
        if not self.hasZ:
            lon,lat=struct.unpack(self.gpoint,self.gv[self.start:self.start+16])
            self.start+=16
            return senc.Point(lon,lat)
        lon,lat,z=struct.unpack(self.gpoint,self.gv[self.start:self.start+24])
        self.start+=24
        return senc.Point(lon,lat,z)

    def readPolygon(self):
        if self.hasZ:
            raise Exception("3rd coordinate not supported for polygons in %s"%self.txt)
        #polygons
        numRings=self.uint()
        rt=senc.PolygonGeometry()
        if numRings <= 0:
            return rt
        if numRings > 1:
            debug=1
        for nr in range(0,numRings):
            numCoord=self.uint()
            ring=[]
            for i in range(0,numCoord):
                point=self.readGeometryPoint()
                ring.append(point)
            rt.rings.append(ring)
        return rt

    def readLineGeometry(self):
        if self.hasZ:
            raise Exception("3rd coordinate not supported for line strings in %s"%self.txt)
        num=self.uint()
        points=[]
        for i in range(0,num):
            points.append(self.readGeometryPoint())
        return senc.LineGeometry(points)

    def readMultiPoint(self):
        numPoints=self.uint()
        rt=senc.MultiPointGeometry([])
        for i in range(0,numPoints):
            nestedReader=GVReader(self.gv,self.txt,self.start)
            if nestedReader.mode != 1:
                raise Exception("invalid multi point geometry in %s",self.txt)
            rt.points.append(nestedReader.readGeometryPoint())
            self.start=nestedReader.start
        return rt

    def readMultiPolygon(self):
        numPoly=self.uint()
        rt=senc.MultiPolygonGeometry()
        for i in range(0,numPoly):
            nestedReader=GVReader(self.gv,self.txt,self.start)
            if nestedReader.mode != 3:
                raise Exception("invalid multi polygon geometry in %s"%self.txt)
            rt.append(nestedReader.readPolygon())
            self.start=nestedReader.start
        return rt

    def readGeometry(self):
        if self.mode == 1:
            rt=self.readGeometryPoint()
            return senc.PointGeometry(rt)
        if self.mode == 2:
            return self.readLineGeometry()
        if self.mode == 3:
            return self.readPolygon()
        if self.mode == 4:
            #multipoint
            return self.readMultiPoint()
        if self.mode == 6:
            #multi-polygon
            return self.readMultiPolygon()
        raise Exception("unknown geometry %d in %s"%(self.mode,self.txt))


def parseGeometry(gv,txt) -> senc.GeometryBase:
    '''
    see https://github.com/locationtech/jts/blob/master/modules/core/src/main/java/org/locationtech/jts/io/WKBReader.java
    :param gv:
    :param txt:
    :return:
    '''
    reader=GVReader(gv,txt)
    return reader.readGeometry()

def objectToSenc(wh: senc.SencFile,name,dbrow,gcol,jsonColumns:list,basedir=None):
    attrs=[]
    geometry=None
    txtdsc=None
    for k,v in dict(dbrow).items():
        if v is None:
            continue
        if k == gcol:
            geometry=parseGeometry(v,name)
        else:
            if k in jsonColumns:
                jv=json.loads(v)
                if type(jv) is list:
                    v=",".join(map(lambda v: str(v),jv))
                else:
                    v=str(jv)
            attrs.append(senc.FAttr(k,v))
            if k.upper()=='TXTDSC':
                txtdsc=v
    wh.addFeature(name,attrs,geometry)
    if txtdsc is not None and basedir is not None:
        fn=os.path.join(basedir,txtdsc)
        if os.path.exists(fn):
            with open(fn,"r",encoding='utf-8',errors='ignore') as h:
                txt=h.read()
                wh.addTxt(txtdsc,txt)
                log("added txt %s",txtdsc)
        else:
            warn("txt file %s not found",txtdsc)
    return True

def getJsonColumns(table:str,cur):
    res=cur.execute("SELECT name, type FROM pragma_table_info('%s')"%table)
    rt=[]
    for dbrow in res:
        if dbrow[1].upper().startswith("JSON"):
            rt.append(dbrow[0])
    return rt

def writeTableToSenc(wh:senc.SencFile,cur,name,gcol,basedir=None):
    jsonColumns=getJsonColumns(name,cur)
    res=cur.execute("select * from %s"%name)
    for dbrow in res:
        objectToSenc(wh,name,dbrow,gcol,jsonColumns,basedir)

#we only handle some sounding attrs to be able to group them nicely
SOUNGD_ATTRS=['SCAMIN','SCAMAX']
def writeSoundings(wh:senc.SencFile,cur,name,gcol):
    def a2l(lastAttrs):
        return list(map(lambda v: senc.FAttr(v,lastAttrs[v]),lastAttrs))
    #find our attr combinations
    res=cur.execute("SELECT name, type FROM pragma_table_info('%s')"%name)
    anames=[]
    for dbrow in res:
        if dbrow[0].upper() in SOUNGD_ATTRS:
            anames.append(dbrow[0])
    chunks=[]
    if len(anames) > 0:
        res=cur.execute("select distinct %s from %s"%(",".join(anames),name))
        for dbrow in res:
            chunks.append(dict(dbrow))
    if len(chunks) <1:
        chunks.append({})
    chunk={}
    for chunk in chunks:
        query=None
        for k,v in chunk.items():
            if query is not None:
                query+=" and "
            else:
                query=""
            if v is None:
                query+="%s is null"%k
            else:
                query+="%s='%s'"%(k,v)
        if query is not None:
            query=" where "+query
        else:
            query=""
        res=cur.execute("select * from %s %s"%(name,query))
        soundings=[]
        for dbrow in res:
            row=dict(dbrow)
            geometry=parseGeometry(row[gcol],"soundings")
            if not geometry.hasZ():
                raise Exception("no 3rd dimension in sounding geometry")
            attrs={}
            if type(geometry) is senc.MultiPointGeometry:
                #take this "as is" and write to the senc
                attrs=[]
                for k,v in row.items():
                    if k == gcol:
                        continue
                    if v is None:
                        continue
                    attrs.append(senc.FAttr(k,v))
                wh.addSoundings(geometry.points,attrs)
                continue
            elif type(geometry) is senc.PointGeometry:
                pass
            else:
                raise Exception("unknown geometry %s for sounding",type(geometry))
            soundings.append(geometry.point)
            if len(soundings) >= 100:
                wh.addSoundings(soundings,a2l(chunk))
                soundings=[]
        if len(soundings) > 0:
            wh.addSoundings(soundings,a2l(chunk))



class Options:
    def __init__(self):
        self.basedir=None
        self.s57dir=os.path.join(os.path.dirname(__file__),"..","..","provider","s57static")
        self.em=senc.SencFile.EM_ALL
        self.scale=None

def main(iname:str, oname:str,options:Options=None):
    if options is None:
        options=Options()
    s57dir=options.s57dir
    basedir=options.basedir
    scale=options.scale
    if not os.path.isdir(s57dir):
        err("s57dir %s not found",s57dir)
    s57mappings=S57Mappings(s57dir)
    s57mappings.parse()
    ibase,dummy=os.path.splitext(os.path.basename(iname))
    if basedir is None:
        basedir=os.path.dirname(iname)
    header=senc.SencHeader(
        nw=senc.Point(-179,80),
        se=senc.Point(179,-80),
        name=ibase,
        edition=1,
        scale=1000 if scale is None else scale) #similar to OpenCPN default
    log("opening %s",iname)
    db=sqlite3.connect(iname)
    db.row_factory = sqlite3.Row
    db.text_factory = lambda b: b.decode(errors = 'ignore')
    log("parsing geometries...")
    curtables = db.cursor()
    cur = db.cursor()
    descriptions=curtables.execute("select * from geometry_columns")
    extent=senc.Extent()
    for drow in descriptions:
        row=dict(drow)
        table=row["f_table_name"]
        col=row['f_geometry_column']
        if row['geometry_format'] != 'WKB':
            raise Exception("invalid geometry format %s in %s, expected WKB"%(row['geometry_format'],table))
        log("parsing %s...",table)
        vrows=cur.execute("select %s from %s"%(col,table))
        id=1
        for vrow in vrows:
            if vrow[0] is None:
                continue
            geometry=parseGeometry(vrow[0],table+":"+str(id))
            geometry.extend(extent)
            id+=1
    log("parsed extent: %s",str(extent))
    header.nw=extent.nw
    header.se=extent.se
    hasDsid=False
    try:
        dsidtable=cur.execute("select * from dsid")
        for dbdsid in dsidtable:
            hasDsid=True
            dsid=dict(dbdsid)
            scale=dsid.get('dspm_cscl')
            if scale is not None:
                log("scale %d found in dsid",scale)
                header.scale=scale
            else:
                warn("no scale found in dsid, using default %d",header.scale)
            edition=dsid.get('dsid_edtn')
            if edition is not None:
                log("found edition %s in dsid",str(edition))
                edition=int(edition)
                if edition < 0:
                    warn("cell edition %d < 0, setting to 0",edition)
                    edition=0
                if edition > 65535:
                    warn("cell edition %d > 65535, setting to65535",edition)
                    edition=65535
                header.edition=edition
            else:
                warn("no edition found in dsid, using default %d",header.edition)
            update=dsid.get('dsid_isdt')
            if update is not None:
                log("found update %s in dsid",update)
                header.update=update
            else:
                warn("no update date found in dsid")
            name=dsid.get('dsid_dsnm')
            if name is not None:
                cn,dummy=os.path.splitext(name)
                log("found chart name %s in dsid",cn)
                header.name=cn
            else:
                warn("no chart name in dsid, using default %s",header.name)
    except Exception as e:
        warn("Exception parsing dsid: %s",str(e))
        pass
    if not hasDsid:
        warn("no dsid record found, using defaults")
    wh=senc.SencFile(s57mappings,oname,header,em=options.em)
    cur = db.cursor()
    tables=cur.execute("SELECT f_table_name,f_geometry_column FROM geometry_columns")
    for tablerow in tables.fetchall():
        table=tablerow[0]
        log("reading table %s",table)
        if table == "soundg":
            writeSoundings(wh,cur,table,tablerow[1])
            continue
        objd=s57mappings.objectClasses.get(table.upper())
        if objd is None:
            warn("ignoring unknown table %s",table)
            continue
        writeTableToSenc(wh,cur,table,tablerow[1],basedir)
    log("wrote %d features to %s",wh.featureId,oname)

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
class RecordTypes:
    HEADER_SENC_VERSION=             1
    HEADER_CELL_NAME=                2
    HEADER_CELL_PUBLISHDATE=         3
    HEADER_CELL_EDITION=             4
    HEADER_CELL_UPDATEDATE=          5
    HEADER_CELL_UPDATE=              6
    HEADER_CELL_NATIVESCALE=         7
    HEADER_CELL_SENCCREATEDATE=      8
    HEADER_CELL_SOUNDINGDATUM=       9

    FEATURE_ID_RECORD=               64
    FEATURE_ATTRIBUTE_RECORD=        65

    FEATURE_GEOMETRY_RECORD_POINT=           80
    FEATURE_GEOMETRY_RECORD_LINE=            81
    FEATURE_GEOMETRY_RECORD_AREA=            82
    FEATURE_GEOMETRY_RECORD_MULTIPOINT=      83
    FEATURE_GEOMETRY_RECORD_AREA_EXT=        84

    VECTOR_EDGE_NODE_TABLE_EXT_RECORD=        85
    VECTOR_CONNECTED_NODE_TABLE_EXT_RECORD=   86

    VECTOR_EDGE_NODE_TABLE_RECORD=             96
    VECTOR_CONNECTED_NODE_TABLE_RECORD=        97

    CELL_COVR_RECORD=                        98
    CELL_NOCOVR_RECORD=                      99
    CELL_EXTENT_RECORD=                      100
    CELL_TXTDSC_INFO_FILE_RECORD=            101

    SERVER_STATUS_RECORD=                    200


class Point:
    def __init__(self,lon:float,lat:float) -> None:
        self.lon=lon
        self.lat=lat

class RecordBase:
    def __init__(self,rtype: int) -> None:
        self.buffer=bytearray(struct.pack("<HI",rtype,0))
        self.rtype=rtype
    def updateLength(self):
        l=struct.pack("<I",len(self.buffer))
        self.buffer[2:6]=l
    def append(self,bytes):
        self.buffer+=bytes
        self.updateLength()
    def appenduint8(self,val:int):
        self.append(struct.pack("<B",val))
    def appenduint16(self,val:int):
        self.append(struct.pack("<H",val))
    def appenduint32(self,val:int):
        self.append(struct.pack("<I",val))
    def appendstr(self,val):
        if type(val) is int:
            val=str(val)
        if type(val) is float:
            val=str(val)
        if type(val) is str:
            val=val.encode(errors='ignore')
        l=len(val)
        self.append(struct.pack("@%ds"%l,val))
    def appenddouble(self,val:float):
        self.append(struct.pack("<d",val))
    def write(self,stream):
        stream.write(self.buffer)

class VersionRecord(RecordBase):
    def __init__(self,version=201) -> None:
        super().__init__(RecordTypes.HEADER_SENC_VERSION)
        self.appenduint16(version)
class CellEditionRecord(RecordBase):
    def __init__(self, val: int=1) -> None:
        super().__init__(RecordTypes.HEADER_CELL_EDITION)
        self.appenduint16(val)
class NativeScaleRecord(RecordBase):
    def __init__(self, scale: int) -> None:
        super().__init__(RecordTypes.HEADER_CELL_NATIVESCALE)
        self.appenduint32(scale)
class StringRecord(RecordBase):
    def __init__(self, rtype: int, val: bytes) -> None:
        super().__init__(rtype)
        self.appendstr(val)
class CellNameRecord(StringRecord):
    def __init__(self, val: bytes) -> None:
        super().__init__(RecordTypes.HEADER_CELL_NAME, val)
class ExtendRecord(RecordBase):
    def __init__(self, nw:Point,se:Point,ne:Point=None,sw:Point=None) -> None:
        super().__init__(RecordTypes.CELL_EXTENT_RECORD)
        if ne is None:
            ne=nw #ignored any way
        if sw is None:
            sw=se
        self.appenddouble(sw.lat)
        self.appenddouble(sw.lon)
        self.appenddouble(nw.lat)
        self.appenddouble(nw.lon)
        self.appenddouble(ne.lat)
        self.appenddouble(ne.lon)
        self.appenddouble(se.lat)
        self.appenddouble(se.lon)

class FeatureIdRecord(RecordBase):
    def __init__(self,typeCode:int, id:int,primitive:int=0)->None:
        super().__init__(RecordTypes.FEATURE_ID_RECORD)
        self.appenduint16(typeCode)
        self.appenduint16(id)
        self.appenduint8(primitive)

#always ensure correct order: 1. FeatureIdRecord, 2.Attr/Geom
class PointGeometryRecord(RecordBase):
    def __init__(self,point: Point)->None:
        super().__init__(RecordTypes.FEATURE_GEOMETRY_RECORD_POINT)
        self.appenddouble(point.lat)
        self.appenddouble(point.lon)
class FeatureAttributeRecord(RecordBase):
    T_INT=0
    T_DOUBLE=1
    T_STR=4
    def __init__(self,typeCode: int,valueType:int,value)->None:
        super().__init__(RecordTypes.FEATURE_ATTRIBUTE_RECORD)
        self.appenduint16(typeCode)
        self.appenduint8(valueType)
        if valueType == self.T_INT:
            self.appenduint32(int(value))
        elif valueType == self.T_DOUBLE:
            self.appenddouble(float(value))
        elif valueType == self.T_STR:
            self.appendstr(value)
        else:
            raise Exception("unknown value type %d for feature %d"%(valueType,typeCode))

class FAttr:
    def __init__(self,name,vtype,val):
        self.name=name
        self.vtype=vtype
        self.val=val

def writeAttributes(s57mappings,wh,atts):
    if type(atts) is not list:
        atts=[atts]
    for att in atts:
        FeatureAttributeRecord(s57mappings.attributes[att.name].id(),att.vtype,att.val).write(wh)




class S57OCL():
    def __init__(self,row)->None:
        self.Code=row["Code"]
        self.Acronym=row["Acronym"]
    def id(self):
        return int(self.Code)
class S57Att():
    def __init__(self,row)->None:
        self.Code=row["Code"]
        self.Acronym=row["Acronym"]
        self.type=row["Attributetype"]
        self.clazz=row["Class"]
    def id(self):
        return int(self.Code)

class S57Mappings:
    OCL="s57objectclasses.csv"
    ATT="s57attributes.csv"
    def __init__(self,s57dir):
        self.s57dir=s57dir
        self.objectClassesId={}
        self.attributesId={}
        self.objectClasses={}
        self.attributes={}
    def parseObjectClasses(self):
        ocl=os.path.join(self.s57dir,self.OCL)
        with open(ocl,"r") as ih:
            reader=csv.DictReader(ih)
            for row in reader:
                oclz=S57OCL(row)
                self.objectClassesId[oclz.id()]=oclz
                self.objectClasses[oclz.Acronym]=oclz
    def parseAttributes(self):
        attrf=os.path.join(self.s57dir,self.ATT)
        with open(attrf,"r") as ih:
            reader=csv.DictReader(ih)
            for row in reader:
                attr=S57Att(row)
                self.attributesId[attr.id()]=attr
                self.attributes[attr.Acronym]=attr

    def parse(self):
        self.parseObjectClasses()
        self.parseAttributes()

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
    return Point(lon,lat)
def pointObjectToSenc(s57mappings:S57Mappings,wh,name,dbrow,fid):
    ftype=s57mappings.objectClasses.get(name.upper())
    if ftype is None:
        warn("Object %s not found",name)
        return False
    FeatureIdRecord(ftype.id(),fid).write(wh)
    attrs=[]
    for k,v in dict(dbrow).items():
        if v is None:
            continue
        if k == 'GEOMETRY':
            point=parsePointGeometry(v,name)
            PointGeometryRecord(point).write(wh)
        else:
            attrd=s57mappings.attributes.get(k.upper()) # type: S57Att
            if attrd is None:
                continue
            at=FeatureAttributeRecord.T_STR
            if attrd.type == 'I':
                at=FeatureAttributeRecord.T_INT
            elif attrd.type == 'F':
                at=FeatureAttributeRecord.T_DOUBLE
            attrs.append(FAttr(attrd.Acronym,at,v))
    writeAttributes(s57mappings,wh,attrs)
    return True


def writeTableToSenc(s57mappings,wh,cur,name,featureId):
    res=cur.execute("select * from %s"%name)
    for dbrow in res:
        if pointObjectToSenc(s57mappings,wh,name,dbrow,featureId):
            featureId+=1
    return featureId



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
        with open(args[0],'wb') as wh:
            nw=Point(12.5,54.7)
            se=Point(14.5,53.7)
            VersionRecord().write(wh)
            CellNameRecord("test").write(wh)
            CellEditionRecord(99).write(wh)
            NativeScaleRecord(30000).write(wh)
            ExtendRecord(nw,se).write(wh)
            featureId=1
            #Ansteuerung GW
            FeatureIdRecord(s57mappings.objectClasses["BOYSAW"].id(),featureId).write(wh)
            atts=[
                FAttr("BOYSHP",FeatureAttributeRecord.T_INT,4),
                FAttr("COLOUR",FeatureAttributeRecord.T_STR,"3,1"),
                FAttr("COLPAT",FeatureAttributeRecord.T_INT,2),
                FAttr("OBJNAM",FeatureAttributeRecord.T_STR,"Greifswald")
            ]
            writeAttributes(s57mappings,wh,atts)
            PointGeometryRecord(Point(13.5015,54.1631667)).write(wh)
        print("created %s"%args[0])
        sys.exit(0)
    db=sqlite3.connect(args[1])
    db.row_factory = sqlite3.Row

    with open(args[0],'wb') as wh:
        nw=Point(0,60)
        se=Point(20,20)
        VersionRecord().write(wh)
        CellNameRecord("test").write(wh)
        CellEditionRecord(99).write(wh)
        NativeScaleRecord(30000).write(wh)
        ExtendRecord(nw,se).write(wh)
        featureId=1
        cur = db.cursor()
        tables=cur.execute("SELECT name FROM sqlite_master WHERE type='table';")
        for tablerow in tables.fetchall():
            table=tablerow[0]
            objd=s57mappings.objectClasses.get(table.upper())
            if objd is None:
                print("ignoring unknown table %s"%table)
                continue
            featureId=writeTableToSenc(s57mappings,wh,cur,table,featureId)
        print("wrote %d features to %s"%(featureId,args[0]))

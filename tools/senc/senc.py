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
import os
from . import s57
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
  def __init__(self, val: str) -> None:
    super().__init__(RecordTypes.HEADER_CELL_NAME, val)
class ExtendRecord(RecordBase):
  def __init__(self, nw:Point,se:Point,ne:Point=None,sw:Point=None) -> None:
    super().__init__(RecordTypes.CELL_EXTENT_RECORD)
    if ne is None:
      ne=Point(se.lon,nw.lat) #ignored any way
    if sw is None:
      sw=Point(nw.lon,se.lat)
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
  #some attrs that are expected to be int's in the code
  #even if they are "E" in the attribute type
  INT_ATTR=["QUALTY","CATOBS","WATLEV",
            "CATWRK","QUAPOS","LITCHR",
            "CONRAD","CONDTN","CATSLC",
            "TOPSHP"]
  '''
  all found double attrs are of type f - noo need for extra handling
  DOUBLE_ATTR=["VALDCO","ORIENT","DRVAL1",
               "DRVAL2","VALSOU","SECTR1",
               "SIGPER","HEIGHT","VALNMR"
               ]
  '''
  @classmethod
  def s57TypeToType(cls,name,s57t):
    if name in cls.INT_ATTR:
      return cls.T_INT
    if type == 'I':
      return cls.T_INT
    elif type == 'F':
      return cls.T_DOUBLE
    return cls.T_STR

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

class SencHeader:
  def __init__(self,nw:Point,se:Point,scale:int,name:str,edition:int,version:int=201):
    self.nw=nw
    self.se=se
    self.scale=scale
    self.name=name
    self.edition=edition
    self.version=version
class FAttr:
    def __init__(self,name,val):
        self.name=name
        self.val=val

class GeometryBase:
  T_POINT=1
  def __init__(self,gtype:int):
    self.gtype=gtype

class PointGeometry(GeometryBase):
  def __init__(self,point:Point):
    super().__init__(self.T_POINT)
    self.point=point

class SencFile():
  def __init__(self,s57mappings:s57.S57Mappings, name:str,header:SencHeader)->None:
    self.name=name
    self.header=header
    self.s57mappings=s57mappings
    self.refPoint=Point(
      (self.header.nw.lon+self.header.se.lon)/2,
      (self.header.nw.lat+self.header.se.lat)/2,
    )
    self.wh=open(name,"wb")
    self.featureId=1
    #should we throw an exception if feature/attribute not found
    self.errNotFound=False
    VersionRecord(self.header.version).write(self.wh)
    CellNameRecord(self.header.name).write(self.wh)
    CellEditionRecord(self.header.edition).write(self.wh)
    ExtendRecord(self.header.nw,self.header.se).write(self.wh)
    NativeScaleRecord(self.header.scale).write(self.wh)

  def _isOpen(self):
    if self.wh is None:
      raise Exception("senc file %s not open"%self.name)
  def addFeature(self,name:str,attributes:list,geometry:GeometryBase):
    self._isOpen()
    od=self.s57mappings.objByName(name)
    if od is None:
      if self.errNotFound:
        raise Exception("unknown feature %s"%name)
      return False
    geometryRecord=None
    if geometry.gtype == GeometryBase.T_POINT:
      geometryRecord=PointGeometryRecord(geometry.point)
    else:
      raise Exception("unknown geometry %d for %s"%(geometry.gtype,name))
    fid=self.featureId
    self.featureId+=1
    FeatureIdRecord(od.id(),fid,0).write(self.wh)
    geometryRecord.write(self.wh)
    for att in attributes:
      ad=self.s57mappings.attrByName(att.name)
      if ad is None:
        if self.errNotFound:
          raise Exception("unkown attribute %s for feature %s id=%d"%(att.name,name,fid))
        continue
      ft=FeatureAttributeRecord.s57TypeToType(att.name,ad.type)
      FeatureAttributeRecord(ad.id(),ft,att.val).write(self.wh)
    return True

  def close(self):
    if self.wh is None:
      return
    self.wh.close()
    self.wh=None

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
import copy
import math
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

class Extent:
  def __init__(self,nw:Point=None, se:Point=None) -> None:
    self.nw=nw
    self.se=se

  def add(self,point:Point):
    if self.nw is None or self.se is None:
      self.nw=copy.copy(point)
      self.se=copy.copy(point)
    else:
      if self.nw.lat < point.lat:
        self.nw.lat=point.lat
      if self.nw.lon > point.lon:
        self.nw.lon=point.lon
      if self.se.lon < point.lon:
        self.se.lon=point.lon
      if self.se.lat > point.lat:
        self.se.lat=point.lat

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
  def appendint32(self,val:int):
    self.append(struct.pack("<i",val))
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
  def appendfloat(self,val:float):
    self.append(struct.pack("<f",val))
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

class LineGeometryRecord(RecordBase):
  def __init__(self,extent:Extent,nvid:int)->None:
    super().__init__(RecordTypes.FEATURE_GEOMETRY_RECORD_LINE)
    self.appenddouble(extent.se.lat)
    self.appenddouble(extent.nw.lat)
    self.appenddouble(extent.nw.lon)
    self.appenddouble(extent.se.lon)
    self.appenduint32(1) #for now only one edge vector
    self.appendint32(-1) #first connected node
    self.appendint32(nvid) #edge vector index
    self.appendint32(-1) #end node
    self.appendint32(0) #always forward

class EdgeVectorRecord(RecordBase):
  def __init__(self,idx:int,enlist:list):
    super().__init__(RecordTypes.VECTOR_EDGE_NODE_TABLE_RECORD)
    self.appenduint32(1) #only one entry included
    self.appenduint32(idx) #table index
    self.appenduint32(len(enlist)) #num entries
    for en in enlist:
      self.appendfloat(en.east)
      self.appendfloat(en.north)


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
  T_LINE=2
  def __init__(self,gtype:int):
    self.gtype=gtype

class PointGeometry(GeometryBase):
  def __init__(self,point:Point):
    super().__init__(self.T_POINT)
    self.point=point

class LineGeometry(GeometryBase):
  def __init__(self,points:list):
    super().__init__(self.T_LINE)
    self.points=points

class EastNorth:
  def __init__(self,e:float,n:float):
    self.east=e
    self.north=n


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
    self.nodeVectorIndex=1
    VersionRecord(self.header.version).write(self.wh)
    CellNameRecord(self.header.name).write(self.wh)
    CellEditionRecord(self.header.edition).write(self.wh)
    ExtendRecord(self.header.nw,self.header.se).write(self.wh)
    NativeScaleRecord(self.header.scale).write(self.wh)

  def _isOpen(self):
    if self.wh is None:
      raise Exception("senc file %s not open"%self.name)

  WGS84_semimajor_axis_meters       = 6378137.0 # WGS84 semimajor axis
  mercator_k0                       = 0.9996
  DEGREE=math.pi/180.0

  def  _toSM(self, point:Point, ref:Point=None) -> EastNorth:
    '''
    compute easting and northing
    see georef.cpp
    :param ref: the chart ref point
    :param point: the point to convert
    :return: 
    '''
    if ref is None:
      ref=self.refPoint

    xlon = point.lon

    #  Make sure lon and lon0 are same phase

    if (point.lon * ref.lon < 0.) and (abs(point.lon - ref.lon) > 180.):
      xlon+= 360.0 if point.lon < 0 else -360.0

    z = self.WGS84_semimajor_axis_meters * self.mercator_k0
    east = (xlon - ref.lon) * self.DEGREE * z

    #// y =.5 ln( (1 + sin t) / (1 - sin t) )
    s = math.sin(point.lat * self.DEGREE)
    y3 = (.5 * math.log((1 + s) / (1 - s))) * z
    s0 = math.sin(ref.lat * self.DEGREE)
    y30 = (.5 * math.log((1 + s0) / (1 - s0))) * z
    north = y3 - y30
    return EastNorth(east,north)

  def addFeature(self,name:str,attributes:list,geometry:GeometryBase):
    self._isOpen()
    od=self.s57mappings.objByName(name)
    if od is None:
      if self.errNotFound:
        raise Exception("unknown feature %s"%name)
      return False
    geometryRecords=[]
    if geometry.gtype == GeometryBase.T_POINT:
      geometryRecords.append(PointGeometryRecord(geometry.point))
    elif geometry.gtype == GeometryBase.T_LINE:
      #1 compute extent and store points
      idx=self.nodeVectorIndex
      self.nodeVectorIndex+=1
      enPoints=[]
      extent=Extent()
      for point in geometry.points:
        enPoints.append(self._toSM(point))
        extent.add(point)
      geometryRecords.append(LineGeometryRecord(extent,idx))
      geometryRecords.append(EdgeVectorRecord(idx,enPoints))
    else:
      raise Exception("unknown geometry %d for %s"%(geometry.gtype,name))
    fid=self.featureId
    self.featureId+=1
    FeatureIdRecord(od.id(),fid,0).write(self.wh)
    for geometryRecord in geometryRecords:
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

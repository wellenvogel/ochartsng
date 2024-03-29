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
from . import s57
import mapbox_earcut as earcut
import numpy
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
  def __init__(self,lon:float,lat:float,val:float=None) -> None:
    self.lon=lon
    self.lat=lat
    if val is not None:
      self.val=val
  def __getitem__(self, item):
    if item == 0:
      return self.lon
    if item == 1:
      return self.lat
    if item == 2 and hasattr(self,'val'):
      return self.val
  def __len__(self):
    if hasattr(self,'val'):
      return 3
    return 2
  def hasZ(self):
    return hasattr(self,'val')


class Extent:
  def __init__(self,nw:Point=None, se:Point=None) -> None:
    self.nw=nw
    self.se=se

  def __str__(self):
    return "Extent: nwlon=%f,nwlat=%f,selon=%f,selat=%f"%\
           (self.nw.lon,self.nw.lat,self.se.lon,self.se.lat)

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
    l=len(val)+1
    self.append(struct.pack("@%ds"%l,val))
  def appenddouble(self,val:float):
    val=float(val)
    self.append(struct.pack("<d",val))
  def appendfloat(self,val:float):
    val=float(val)
    self.append(struct.pack("<f",val))
  def appendext(self,extent:Extent):
    self.appenddouble(extent.se.lat)
    self.appenddouble(extent.nw.lat)
    self.appenddouble(extent.nw.lon)
    self.appenddouble(extent.se.lon)
  def write(self,stream):
    self.updateLength()
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
  def __init__(self,extent:Extent,evid:int)->None:
    super().__init__(RecordTypes.FEATURE_GEOMETRY_RECORD_LINE)
    self.appendext(extent)
    self.appenduint32(1) #for now only one edge vector
    self.appendint32(-1) #first connected node
    self.appendint32(evid) #edge vector index
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

class SoundingRecord(RecordBase):
  def __init__(self,extent:Extent,soundings:list):
    super().__init__(RecordTypes.FEATURE_GEOMETRY_RECORD_MULTIPOINT)
    self.appendext(extent)
    self.appenduint32(len(soundings))
    for sndg in soundings:
      self.appendfloat(sndg.east)
      self.appendfloat(sndg.north)
      self.appendfloat(sndg.val)

class AreaGeometryRecord(RecordBase):
  def __init__(self,extent: Extent,vertices:list,evids:list):
    if len(vertices)%3 != 0:
      raise Exception("invalid vertex list, len %d ",len(vertices))
    super().__init__(RecordTypes.FEATURE_GEOMETRY_RECORD_AREA)
    self.appendext(extent)
    self.appenduint32(0) #contour count that is skipped any way
    self.appenduint32(1) #number of vertex lists
    self.appenduint32(len(evids)) #number of edge vectors
    self.appenduint8(4) #vertex type PTG - see S57Object::RenderObject::RenderSingleRule
    self.appendint32(len(vertices))
    self.appendext(extent) #is not read any way...
    for vert in vertices:
      self.appendfloat(vert.east)
      self.appendfloat(vert.north)
    for evid in evids:
      self.appendint32(-1) #first connected node
      self.appendint32(evid) #edge vector index
      self.appendint32(-1) #end node
      self.appendint32(0) #always forward

class FeatureAttributeRecord(RecordBase):
  T_INT=0
  T_DOUBLE=2
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
    if name.upper() in cls.INT_ATTR:
      return cls.T_INT
    if s57t == 'I':
      return cls.T_INT
    elif s57t == 'F':
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


class TXTRecord(RecordBase):
  def __init__(self,name:str,txt:str):
    super().__init__(RecordTypes.CELL_TXTDSC_INFO_FILE_RECORD)
    nlen=len(name)+1
    tlen=len(txt)+1
    self.appenduint32(nlen)
    self.appenduint32(tlen)
    self.appendstr(name)
    self.appendstr(txt)

class SencHeader:
  def __init__(self,nw:Point,se:Point,scale:int,name:str,edition:int,version:int=201):
    self.nw=nw
    self.se=se
    self.scale=scale
    self.name=name
    self.edition=edition
    self.version=version
    self.update=None
class FAttr:
    def __init__(self,name,val):
        self.name=name
        self.val=val

class GeometryBase:
  def __init__(self):
    pass
  def extend(self,ext:Extent):
    pass
  def hasZ(self):
    return False

class PointGeometry(GeometryBase):
  def __init__(self,point:Point):
    super().__init__()
    self.point=point
  def extend(self, ext: Extent):
    ext.add(self.point)
  def hasZ(self):
    return self.point.hasZ()


class MultiPointGeometry(GeometryBase):
  def __init__(self,points: list):
    super().__init__()
    self.points=points

  def extend(self, ext: Extent):
    for p in self.points:
      ext.add(p)
  def hasZ(self):
    if self.points is None or len(self.points) < 1:
      return False
    return self.points[0].hasZ()


class PolygonGeometry(GeometryBase):
  def __init__(self,rings:list=None):
    '''
    the first ring is the outer ring
    others are the holes
    :param rings:
    '''
    super().__init__()
    if rings is None:
      self.rings=[]
    else:
      self.rings=rings

  def extend(self, ext: Extent):
    for ring in self.rings:
      for p in ring:
        ext.add(p)

class MultiPolygonGeometry(GeometryBase):
  def __init__(self):
    super().__init__()
    self.polygons=[]
  def append(self,polygon):
    if type(polygon) is PolygonGeometry:
      self.polygons.append(polygon.rings)
    else:
      self.polygons.append(polygon)

  def extend(self, ext: Extent):
    for polygon in self.polygons:
      for ring in polygon:
        for p in ring:
          ext.add(p)


class LineGeometry(GeometryBase):
  def __init__(self,points:list):
    super().__init__()
    self.points=points

  def extend(self, ext: Extent):
    for p in self.points:
      ext.add(p)


class EastNorth:
  def __init__(self,e:float,n:float):
    self.east=e
    self.north=n
  def __getitem__(self, item):
    if item == 0:
      return self.east
    if item == 1:
      return self.north
  def __len__(self):
    return 2

  def __array__(self):
    return (self.east,self.north)

class Sounding(EastNorth):
  def __init__(self,e:float,n:float,val:float):
    super().__init__(e,n)
    self.val=val

class SencFile():
  EM_NONE=0
  EM_EXT=1
  EM_ALL=2
  def __init__(self,s57mappings:s57.S57Mappings, name:str,header:SencHeader, em=EM_EXT)->None:
    self.name=name
    self.header=header
    self.s57mappings=s57mappings
    self.em=em
    self.refPoint=Point(
      (self.header.nw.lon+self.header.se.lon)/2,
      (self.header.nw.lat+self.header.se.lat)/2,
    )
    self.wh=open(name,"wb")
    self.featureId=1
    #should we throw an exception if feature/attribute not found
    self.errNotFound=False
    self.edgeVectorIndex=1
    self.addedTxt=[]
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
    y3 = (.5 * math.log((1 + s) / (1 - s)))
    #y3alt=math.asinh(math.tan(point.lat*self.DEGREE))
    s0 = math.sin(ref.lat * self.DEGREE)
    y30 = (.5 * math.log((1 + s0) / (1 - s0)))
    north = (y3 - y30)*z
    return EastNorth(east,north)

  def _polyRingsToRecords(self,rings,txt):
    records=[]
    enpoints=[]
    vertinput=[]
    extent=Extent()
    hidx=[]
    idx=0
    evids=[]
    hasPoints=False
    for ring in rings:
      if ring[0].lon != ring[-1].lon or ring[0].lat != ring[-1].lat:
        raise Exception("ring %d in %s not closed"%(idx,txt))
      ringpoints=[]
      for p in ring:
        en=self._toSM(p,self.refPoint)
        vertinput.append((en.east,en.north))
        enpoints.append(en)
        ringpoints.append(en)
        extent.add(p)
        hasPoints=True
      hidx.append(len(enpoints))
      if (idx == 0 and self.em == self.EM_EXT) or self.em == self.EM_ALL:
        evid=self.edgeVectorIndex
        self.edgeVectorIndex+=1
        records.append(EdgeVectorRecord(evid,ringpoints))
        evids.append(evid)
      idx+=1
    #earcut triangulation
    triangles=[]
    ecverts = numpy.array(vertinput).reshape(-1,2)
    ecrings=numpy.array(hidx)
    triangles=earcut.triangulate_float64(ecverts,ecrings)
    if len(triangles)%3 != 0:
      raise Exception("invalid triangulation result, len must be multiple of 3, is %d"%len(triangles))
    vertexlist=[]
    for i in range(0,len(triangles)):
      vertexlist.append(enpoints[triangles[i]])
    if hasPoints:
      records.append(AreaGeometryRecord(extent,vertexlist,evids))
    return records

  def _buildGeometryRecords(self,name:str,geometries):
    if geometries is None:
      return []
    if type(geometries) is not list:
      geometries=[geometries]
    geometryRecords=[]
    for geometry in geometries:
      if type(geometry) is PointGeometry:
        geometryRecords.append(PointGeometryRecord(geometry.point))
      elif type(geometry) is LineGeometry:
        #1 compute extent and store points
        idx=self.edgeVectorIndex
        self.edgeVectorIndex+=1
        enPoints=[]
        extent=Extent()
        geometry.extend(extent)
        for point in geometry.points:
          enPoints.append(self._toSM(point))
        geometryRecords.append(LineGeometryRecord(extent,idx))
        geometryRecords.append(EdgeVectorRecord(idx,enPoints))
      elif type(geometry) is PolygonGeometry:
        geometryRecords+=self._polyRingsToRecords(geometry.rings,name)
      elif type(geometry) is MultiPolygonGeometry:
        for poly in geometry.polygons:
          geometryRecords+=self._polyRingsToRecords(poly,name)
      else:
        print("unknown geometry %s for %s, ignoring"%(type(geometry),name))
        continue
    return geometryRecords



  def addFeature(self,name:str,attributes:list,geometries:GeometryBase=None):
    self._isOpen()
    od=self.s57mappings.objByName(name)
    if od is None:
      if self.errNotFound:
        raise Exception("unknown feature %s"%name)
      return False
    geometryRecords=self._buildGeometryRecords(name,geometries)
        #raise Exception("unknown geometry %d for %s"%(geometry.gtype,name))
    fid=self.featureId
    self.featureId+=1
    FeatureIdRecord(od.id(),fid,0).write(self.wh)
    for geometryRecord in geometryRecords:
      geometryRecord.write(self.wh)
    for att in attributes:
      if att.val is None:
        continue
      ad=self.s57mappings.attrByName(att.name)
      if ad is None:
        if self.errNotFound:
          raise Exception("unkown attribute %s for feature %s id=%d"%(att.name,name,fid))
        continue
      ft=FeatureAttributeRecord.s57TypeToType(att.name,ad.type)
      FeatureAttributeRecord(ad.id(),ft,att.val).write(self.wh)
    return True

  def addSoundings(self,soundings:list,attrs:list=None):
    self.addFeature("SOUNDG",attrs)
    enlist=[]
    extent=Extent()
    for sndg in soundings:
      ensndg=self._toSM(sndg)
      extent.add(sndg)
      ensndg.val=sndg.val
      enlist.append(ensndg)
    SoundingRecord(extent,enlist).write(self.wh)

  def addTxt(self,name:str, txt:str):
    if name in self.addedTxt:
      return False
    self.addedTxt.append(name)
    TXTRecord(name,txt).write(self.wh)
    return True

  def close(self):
    if self.wh is None:
      return
    self.wh.close()
    self.wh=None

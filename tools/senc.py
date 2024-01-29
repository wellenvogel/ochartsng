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
    def appenduint16(self,val:int):
        self.append(struct.pack("<H",val))
    def appenduint32(self,val:int):
        self.append(struct.pack("<I",val))
    def appendstr(self,val:bytes):
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
        if type(val) is str:
            val=val.encode(errors='ignore')
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





if __name__ == '__main__':
    with open(sys.argv[1],'wb') as wh:
        nw=Point(12.5,54.7)
        se=Point(14.5,53.7)
        VersionRecord().write(wh)
        CellNameRecord("test").write(wh)
        CellEditionRecord(99).write(wh)
        NativeScaleRecord(30000).write(wh)
        ExtendRecord(nw,se).write(wh)
        
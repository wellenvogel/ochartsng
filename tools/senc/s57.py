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
import csv
import os


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
  DEFAULT_DIR=os.path.join(os.path.dirname(__file__),"..","..","provider","s57static")
  def __init__(self,s57dir=None):
    self.s57dir=s57dir
    if self.s57dir is None:
      self.s57dir=self.DEFAULT_DIR
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

  def objByName(self,name:str) -> S57OCL:
    o=self.objectClasses.get(name.upper())
    return o
  def objById(self,id) -> S57OCL:
    id=int(id)
    o=self.objectClassesId.get(id)
    return o
  def attrByName(self,name:str)->S57Att:
    return self.attributes.get(name.upper())
  def attrById(self,id)->S57Att:
    id=int(id)
    return self.attributesId.get(id)
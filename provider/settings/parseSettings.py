#! /usr/bin/env python3
'''
******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Settings parser
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2023 by Andreas Vogel   *
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
'''

import csv
import datetime
import json
import sys
import os

CATCOL=0
NAMECOL=1
METHODCOL=2
VALCOL=3
TITLECOL=4
DEFAULTCOL=5
MINCOL=6 #also enum values
MAXCOL=7 #also enum names
GROUPCOL=8
REQUESTCOL=9
MINLEN= REQUESTCOL + 1

def entryFromRow(row):
  rt={}
  rt['category']=row[CATCOL]
  rt['name']=row[NAMECOL]
  type=row[METHODCOL]
  rt['type']=type
  rt['title']=row[TITLECOL]
  rt['group']=row[GROUPCOL]
  if type == 'enum':
    rt['values']=row[MINCOL]
    rt['choices']=row[MAXCOL]
  else:
    if type != 'bool':
      rt['min']=row[MINCOL]
      rt['max']=row[MAXCOL]
  default=row[DEFAULTCOL]
  if default == "":
    default="1"
  rt['default']=default
  rt['valtype']=row[VALCOL]
  rt['request']=row[REQUESTCOL]
  return rt

def parseSettings(infile):
  outData = {'important': [], 'detail': []}
  with open(infile, "r",encoding="utf-8") as csvin:
    rd = csv.reader(csvin)
    lastrow = None
    for row in rd:
      if len(row) < MINLEN:
        print("row to short, ignore %s\n" % ",".join(row))
        continue
      for col in [CATCOL]:
        if lastrow is not None and row[col] == '' and lastrow[col] != '':
          row[col] = lastrow[col]
      lastrow = row
      if row[METHODCOL] == '':
        continue
      if outData.get(row[CATCOL]) is not None:
        outData[row[CATCOL]].append(entryFromRow(row))
  return outData


def generateJson(infile,outfile):
  outData=parseSettings(infile)
  with open(outfile,"w") as of:
    json.dump(outData,of)
    of.close()

def generateHeader(infile,outfile):
  outData = parseSettings(infile)
  (base,ext)=os.path.splitext(os.path.basename(outfile))
  gendate=datetime.datetime.now().isoformat()
  with open(outfile, "w") as of:
    of.write("//generated from %s at %s\n"%(infile,gendate))
    of.write("#ifndef %s_H\n"%base.upper())
    of.write("#define %s_H\n"%base.upper())
    of.write("#include \"UserSettingsBase.h\"\n")
    of.write("#include \"RenderSettings.h\"\n")
    of.write("#include \"S57ObjectClasses.h\"\n")
    of.write("UserSettingsList userSettings={\n")
    of.write("new UserSettingsEntryVersion(\"%s\")"%gendate)
    written=True
    for cat in outData.keys():
      catData=outData[cat]
      for entry in catData:
        if written:
          of.write(",\n")
          written=False
        type=entry['type']
        if type == 'depth' or type == 'float':
          type='double'
        if cat == 'detail':
          oname=entry['name'].replace('viz','')
          id="S57ObjectClasses::"+oname
          if id is None:
            print("WARNING: detail %s not found\n",oname)
            continue
          of.write("new UserSettingsEntryDetail(\"%s\",\"%s\",UserSettingsEntry::TYPE_BOOL,%s)"%
                   (
                    cat,
                    entry['name'],
                    id
                   ))
          written=True
          continue  
        if type == 'enum':
          et=entry['valtype']
          of.write("new UserSettingsEntryEnum<%s>(\"%s\",\"%s\",UserSettingsEntry::TYPE_%s,IV(%s),US_GETTER(%s,%s),US_SETTER(%s,%s))"%
                 (
                  et,
                 cat,
                 entry['name'],
                 entry['type'].upper(),
                 entry['values'],
                 et,
                 entry['name'],
                 et,
                 entry['name']
                 ))
          written=True
          continue
        if type == 'stringlist':
          of.write("new UserSettingsEntryStringList(\"%s\",\"%s\",UserSettingsEntry::TYPE_%s,SV(%s),US_GETTER(%s,%s),US_SETTER(%s,%s))"%
                 (
                 cat,
                 entry['name'],
                 entry['type'].upper(),
                 entry.get('values') or '',
                 'String',
                 entry['name'],
                 'String',
                 entry['name']
                 ))
          written=True
          continue
        if type == 'int' or type == 'double':
          of.write(
            "new UserSettingsEntryMinMax<%s>(\"%s\",\"%s\",UserSettingsEntry::TYPE_%s, %s,%s,US_GETTER(%s,%s),US_SETTER(%s,%s))" %
            (
             type,
             cat,
             entry['name'],
             type.upper(),
             entry['min'],
             entry['max'],
             type,
             entry['name'],
             type,
             entry['name']
             ))
          written=True
        if type == 'bool':
          of.write(
            "new UserSettingsEntryBool(\"%s\",\"%s\",UserSettingsEntry::TYPE_%s,US_GETTER(%s,%s),US_SETTER(%s,%s))" %
            (
             cat,
             entry['name'],
             entry['type'].upper(),
             type,
             entry['name'],
             type,
             entry['name']
             ))
          written=True
    of.write("\n};\n")
    of.write("#endif\n")
    of.close()
    
def generateDef(infile,outfile):
  outData = parseSettings(infile)
  (base,ext)=os.path.splitext(os.path.basename(outfile))
  with open(outfile, "w") as of:
    of.write("//generated from %s at %s\n"%(infile,datetime.datetime.now().isoformat()))
    of.write("#ifndef %s_H\n"%base.upper())
    of.write("#define %s_H\n"%base.upper())
    of.write("#include \"RenderSettingsBase.h\"\n")
    of.write("class RenderSettings : public RenderSettingsBase{\n")
    of.write("  public:\n")
    of.write("    using ConstPtr=std::shared_ptr<const RenderSettings>;\n")
    for cat in outData.keys():
      if cat == 'detail':
        continue
      catData=outData[cat]
      for entry in catData:
        type=entry['type']
        if type == 'int':
          of.write("    int %s=%s;\n"%(entry['name'],entry['default']))
        if type == 'stringlist':
          of.write("    String %s=\"%s\";\n"%(entry['name'],entry['default']))  
        if type == 'double' or type == 'depth' or type == 'float':
          of.write("    double %s=%s;\n"%(entry['name'],entry['default']))
        if type == 'bool':
          of.write("    bool %s=%s;\n"%(entry['name'],entry['default']))
        if type == 'enum':
          of.write("    %s %s=(%s)%s;\n"%(entry['valtype'],entry['name'],entry['valtype'],entry['default']))


    of.write("};\n")
    of.write("#endif\n")
    of.close()

def checkOlder(file,ts):
  if not os.path.exists(file):
    return True
  mt=os.stat(file).st_mtime
  if mt < ts:
    return True
  print("%s is newer/equal"%file)
  return False

if __name__ == '__main__':
  mode="all"
  if len(sys.argv) > 1:
    mode=sys.argv[1]
  pdir=os.path.dirname(__file__)
  infile=os.path.join(pdir,"Settings.csv")
  if not os.path.exists(infile):
    print("ERROR: settings file %s does not exist\n"%infile)
    sys.exit(1)
  inTs=os.stat(infile).st_mtime
  ods=infile.replace(".csv",".ods")
  if os.path.exists(ods):
    odsTs=os.stat(ods).st_mtime
    if inTs < odsTs:
      print("ERROR: file %s is newer then %s"%(ods,infile))
      sys.exit(1)
  if mode == "all" or mode == "json":
    jsonfile=os.path.join(pdir,"..","..","gui","generated","settings.json")
    if checkOlder(jsonfile,inTs):
      gendir=os.path.dirname(jsonfile)
      if not os.path.isdir(gendir):
        os.makedirs(gendir)
      print("generating %s from %s\n"%(jsonfile,infile))
      generateJson(infile,jsonfile)
  if mode == "all" or mode == "impl":
    header=os.path.join(pdir,"..","include","generated","UserSettings.h")
    gendir=os.path.dirname(header)
    if not os.path.isdir(gendir):
      os.makedirs(gendir)
    if checkOlder(header,inTs):
      print("generating %s from %s\n"%(header,infile))
      generateHeader(infile,header)
    cls=os.path.join(pdir,"..","include","generated","RenderSettings.h")
    if checkOlder(cls,inTs):
      print("generating %s from %s\n"%(cls,infile))
      generateDef(infile,cls)

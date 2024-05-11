# -*- coding: utf-8 -*-
# vim: ts=2 sw=2 et ai
###############################################################################
# Copyright (c) 2020-2022 Andreas Vogel andreas@wellenvogel.net
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#
###############################################################################
import urllib.parse
import zipfile
import sys
import os
import traceback
import hashlib
import glob
import shutil
import re
class S57Converter:
  def __init__(self,api,chartbase,doneUrl):
    self.api=api
    self.chartbase=chartbase
    self.pattern=re.compile(r'.*\.[0-9][0-9][0-9]$')
    self.converter=os.path.join(os.path.dirname(__file__),"tosenc.py")
    self.doneUrl=doneUrl
  def _canHandle(self,fn):
    return self.pattern.match(fn) is not None or fn.lower().endswith('.sqlite') or fn.lower().endswith('.geojson')
  def _handleZipFile(self,md5,fn):
    rt=0
    try:
      zip=zipfile.ZipFile(fn)
      for info in zip.infolist():
        if info.is_dir():
          continue
        if self._canHandle(info.filename):
          rt+=1
          md5.update(info.filename.encode(errors="ignore"))
          md5.update(str(info.date_time).encode(errors='ignore'))
          md5.update(str(info.file_size).encode(errors="ignore"))
      zip.close()
      return rt
    except Exception as e:
      self.api.error("unable to handle zipfile %s:%s"%(fn,traceback.format_exc()))
      return 0
  def countConvertibleFiles(self,dirOrFile):
    '''
    count the files that can be converted and
    also compute an md5 hash to check for changes later on
    @param dirOrFile:
    @return: (num,md5)
    '''
    if not os.path.isfile(dirOrFile) or not dirOrFile.upper().endswith('.ZIP'):
      return (0,None)
    md5=hashlib.md5()
    rt=self._handleZipFile(md5,dirOrFile)
    return (rt,md5.hexdigest())
  def _tmpBase(self):
    return os.path.join(self.chartbase,'_TMP','cnv')
  def _cleanupTmp(self):
    try:
      for f in glob.glob(self._tmpBase()+"*"):
        self.api.log("remove existing converter dir %s",f)
        try:
          shutil.rmtree(f,ignore_errors=True)
        except Exception as e:
          self.api.error("cannot remove tmp dir %s:%s",f,str(e))
    except:
      pass
  def getConverterCommand(self,input,outname):
    '''
    get the command to run the conversion
    @param input:
    @param outname:
    '''
    #as this is just called before a new conversion we clean up the tmp dir
    self._cleanupTmp()
    tmpout=self._tmpBase()+outname
    cmd=[sys.executable,self.converter,"-i","-f",self.getOutFileOrDir(outname),"-c",outname]
    if self.doneUrl is not None:
      finUrl=self.doneUrl+"?"+urllib.parse.urlencode({'name':outname})
      cmd.append('-u')
      cmd.append(finUrl)
    cmd.append(input)
    cmd.append(tmpout)
    self.api.log("converter command for %s: %s",input," ".join(cmd))
    return cmd
  def getOutFileOrDir(self,outname):
    return os.path.join(self.chartbase,outname)
  def handledExtensions(self):
    return ['zip']
  def getName(self):
    return 'ochartsng-s57'

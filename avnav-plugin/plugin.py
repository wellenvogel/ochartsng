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

import datetime
import json
import os
import signal
import re
import sys
import time
import traceback
try:
  from urllib.request import urlopen
  unicode=str
except:
  from urllib2 import urlopen
import subprocess
import platform
import shutil
import threading
import psutil

class ConfigParam:
  def __init__(self,name,type='STRING',description='',default=None,mandatory=False,rangeOrList=None):
    self.name=name
    self.description=description
    self.type=type
    self.default=default
    self.rangeOrList=rangeOrList
    self.mandatory=mandatory
  def toDict(self):
    return self.__dict__.copy()
  def convertValue(self,v):
    if v is None or v == '':
      return None
    if self.type == 'NUMBER':
      if v == '':
        return v
      return int(v)
    if self.type == 'BOOLEAN':
      return v.lower() == 'true'
    return v 
  def fromParam(self,param, doRaise=False):
    v=param.get(self.name)
    if v is None or v == '':
      v=self.default
    if v is None or v=='':
      if not self.mandatory:
        return None
      if doRaise:
        raise Exception("missing parameter %s"%self.name)
      return v
    return self.convertValue(v)
  def fromApi(self,api):
    return api.getConfigValue(self.name,self.default)
  def check(self,v):
    if v is None or v == '':
      if self.default is None or self.default == '':
        if self.mandatory:
          raise Exception("missing parameter %s"%self.name)
        return False  
    v=self.convertValue(v)
    if v is None:
      return v
    if self.rangeOrList is not None:
      if self.type == 'NUMBER':
        if len(self.rangeOrList) >= 1:
          if v < self.rangeOrList[0]:
            raise Exception("parameter %s out of range"%self.name)
        if len(self.rangeOrList) >= 2:
          if v > self.rangeOrList[1]:
            raise Exception("parameter %s out of range"%self.name)         
        return True
      if self.type == 'SELECT':
        if not v in self.rangeOrList:
          raise Exception("parameter %s not from it's list"%self.name)
    return True          


  

class Plugin:
  EXENAME="AvnavOchartsProvider"
  STARTSCRIPT="provider.sh"
  ENV_NAME="AVNAV_PROVIDER"
  ENV_WORKDIR="AVNAV_PROVIDER_WORKDIR"
  STARTPAGE="index.html"
  C_PORT=ConfigParam("port",type='NUMBER',description='the listener port for the chart provider executable',default='8082',mandatory=True)
  C_DEBUG=ConfigParam("debug",type='SELECT',default='1',description='debuglevel for provider',rangeOrList=['0','1','2'], mandatory=True)
  C_MEMPERCENT=ConfigParam("memPercent",type='NUMBER',default='50',description='percent of existing mem to be used',rangeOrList=[2,95])
  C_TILECACHEKB=ConfigParam("tileCacheKb",type='NUMBER',default='40960',description='kb memory to be used for the tile cache',rangeOrList=[100,400000])
  EDITABLE_CONFIG=[
    C_PORT,
    C_DEBUG,
    C_MEMPERCENT,
    C_TILECACHEKB
  ]
  
  C_DATADIR=ConfigParam('dataDir',type='STRING',description='base directory for data',default='$DATADIR/ochartsng', mandatory=True)
  BASE_CONFIG=[
    C_DATADIR
  ]
               
  CONFIG=EDITABLE_CONFIG+BASE_CONFIG

  @classmethod
  def pluginInfo(cls):
    """
    the description for the module
    @return: a dict with the content described below
            parts:
               * description (mandatory)
               * data: list of keys to be stored (optional)
                 * path - the key - see AVNApi.addData, all pathes starting with "gps." will be sent to the GUI
                 * description
    """
    return {
      'description': 'ochartsng provider for AvNav',
      'version': '1.0',
      'config':list(x.toDict() for x in cls.CONFIG),
      'data': [

      ]
    }

  def __init__(self,api):
    """
        initialize a plugins
        do any checks here and throw an exception on error
        do not yet start any threads!
        @param api: the api to communicate with avnav
        @type  api: AVNApi
    """
    self.api = api
    self.config={}
    self.baseUrl=None #will be set in run
    self.connected=False
    self.chartList=[]
    self.changeSequence=0
    self.stopSequence=0
    self.providerPid=-1
    self.remoteHost=None
    if hasattr(self.api,'registerEditableParameters'):
      self.api.registerEditableParameters(list( x.toDict() for x in self.EDITABLE_CONFIG),self.updateConfig)
    if hasattr(self.api,'registerRestart'):
      self.api.registerRestart(self.stop)


  def findProcessByPattern(self,exe, checkuser=False,wildcard=False):
    """
    return a list with pid,uid,name for running chartproviders
    :@param exe the name of the executable
    :return:
    """
    processList=psutil.process_iter(['name','uids','ppid','pid'])
    rtlist=[]
    for process in processList:
      try:
        if process.info is None:
          continue
        info=process.info
        uid=info.get('uids')
        if uid is None:
          continue
        if checkuser and uid.effective != os.getuid():
          continue
        nameMatch=False
        name=None
        if exe is None:
          nameMatch=True
        elif type(exe) is list:
          for n in exe:
            if wildcard:
              nameMatch=re.match(n,info['name'],re.IGNORECASE)
              if nameMatch:
                name=n
            else:
              if n == info['name']:
                name=n
                nameMatch=True
        else:
          name = exe
          if wildcard:
            nameMatch = re.match(exe, info['name'], re.IGNORECASE)
          else:
            nameMatch= info['name']  == exe
        if not nameMatch:
          continue
        rtlist.append([info.get('pid'),uid.effective,name])
      except:
        self.api.error("error fetching process list: %s",traceback.format_exc())
    return rtlist

  def isPidRunning(self,pid):
    ev=self.getEnvValueFromPid(pid)
    if ev is None:
      return False
    return ev == self.getEnvValue()

  def getEnvValueFromPid(self,pid):
    try:
      process=psutil.Process(pid)
      if process is None:
        return None
      environ=process.environ()
      return environ.get(self.ENV_NAME)
    except Exception as e:
      self.api.debug("unable to read env for pid %d: %s" % (pid, e))
    return None
  
  def filterProcessList(self,list,checkForEnv=False):
    """
    filter a list returned by findProcessByPattern for own user
    :param list:
    :param checkForParent: also filter out processes with other parent
    :return:
    """
    rt=[]
    for entry in list:
      if entry[1] != os.getuid():
        continue
      if checkForEnv:
        envValue=self.getEnvValueFromPid(entry[0])
        if envValue is None or envValue != self.getEnvValue():
          continue
      rt.append(entry)
    return rt

  #we only allow one provider per config dir
  def getEnvValue(self):
    configdir = self.C_DATADIR.fromParam(self.config)
    return platform.node()+":"+configdir

  def getCmdLine(self):
    exe=os.path.join(os.path.dirname(__file__),self.STARTSCRIPT)
    if not os.path.exists(exe):
      raise Exception("executable %s not found"%exe)
    dataDir=self.C_DATADIR.fromParam(self.config)  
    if not os.path.exists(dataDir):
      raise Exception("data dir %s not found" % dataDir)
    logname = os.path.join(dataDir, "log")
    cmdline = ["/bin/sh",exe,
               '-d',str(self.config['debug']),
               '-l' ,logname,
               '-p', str(os.getpid()),
              ]
    memPercent=self.C_MEMPERCENT.fromParam(self.config)          
    if memPercent is not None:
      cmdline= cmdline + ["-x",str(memPercent)]
    tileCacheKb=self.C_TILECACHEKB.fromParam(self.config)
    if tileCacheKb is not None:
      cmdline=cmdline + ["-c",str(tileCacheKb)]
    cmdline=cmdline + [dataDir,str(self.C_PORT.fromParam(self.config))]
    return cmdline

  def handleProcessOutput(self,process):
    buffer=process.stdout.readline()
    while buffer is not None and buffer != b"":
      try:
        self.api.log("PROVIDEROUT: %s",buffer.decode('utf-8'))
      except:
        pass
      buffer=process.stdout.readline()

  def startProvider(self):
    cmdline=self.getCmdLine()
    envValue = self.getEnvValue()
    env = os.environ.copy()
    env.update({
      self.ENV_NAME: envValue,
      self.ENV_WORKDIR: self.C_DATADIR.fromParam(self.config)
      })
    self.api.log("starting provider with command %s"%" ".join(cmdline))
    process=subprocess.Popen(cmdline,env=env,close_fds=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if process is None:
      raise Exception("unable to start provider with %s"," ".join(cmdline))
    reader=threading.Thread(target=self.handleProcessOutput,args=[process])
    reader.start()
    return process

  def listCharts(self,hostip):
    self.api.debug("listCharts %s"%hostip)
    iconUrl=None
    if self.remoteHost is not None:
      hostip=self.remoteHost
    try:
      iconUrl=self.api.getBaseUrl()+"/gui/icon.png"
    except:
      #seems to be an AvNav that still does not have getBaseUrl...
      pass
    if not self.connected:
      self.api.debug("not yet connected")
      return []
    try:
      items=self.chartList+[]
      for item in items:
        if iconUrl is not None and item.get('icon') is None:
          item['icon']=iconUrl
        for k in list(item.keys()):
          if type(item[k]) == str or type(item[k]) == unicode:
            item[k]=item[k].replace("localhost",hostip).replace("127.0.0.1",hostip)
      return items
    except:
      self.api.debug("unable to contact provider: %s"%traceback.format_exc())
      return []

  
  def updateConfig(self,newConfig):
    completeConfig=self.config.copy()
    completeConfig.update(newConfig)
    for cfg in self.CONFIG:
      v=cfg.fromParam(completeConfig,True)
      cfg.check(v)
    self.api.saveConfigValues(newConfig)
    self.changeSequence+=1

  def stop(self):
    self.api.log("stopping ocharts")
    self.stopSequence+=1
    self.changeSequence+=1

  def run(self):
    sequence=self.stopSequence
    while self.stopSequence == sequence:
      try:
        self.runInternal()
        self.stopProvider()
        self.api.log("runInternal stopped")
      except Exception as e:
        self.api.setStatus('ERROR',str(e))
        self.stopProvider()
        raise
    self.api.log("finishing run")  

  def stopProvider(self):
    backgroundList=self.filterProcessList(self.findProcessByPattern(None),True)
    for bp in backgroundList:
      pid=bp[0]
      self.api.log("killing background process %d",pid)
      try:
        os.kill(pid,signal.SIGKILL)
      except:
        self.api.error("unable to kill background process %d:%s",pid,traceback.format_exc())  
    try:
      provider=psutil.Process(self.providerPid)
      provider.wait(0)
    except:
      self.api.debug("error waiting for dead children: %s", traceback.format_exc())

  def getBooleanCfg(self,name,default):
    v=self.api.getConfigValue(name,default)
    if type(v) is str:
      return v.lower() == 'true'
    return v
  DATADIRS=['log','charts']  
  def runInternal(self):
    """
    the run method
    this will be called after successfully instantiating an instance
    this method will be called in a separate Thread
    The example simply counts the number of NMEA records that are flowing through avnav
    and writes them to the store every 10 records
    @return:
    """
    sequence=self.changeSequence
    for cfg in self.CONFIG:
      v=cfg.fromApi(self.api)
      cfg.check(v)
      self.config[cfg.name]=v
    port=self.C_PORT.fromParam(self.config)
    for name in list(self.config.keys()):
      if type(self.config[name]) == str or type(self.config[name]) == unicode:
        self.config[name]=self.config[name].replace("$DATADIR",self.api.getDataDir())
        self.config[name] = self.config[name].replace("$PLUGINDIR", os.path.dirname(__file__))
    dataBaseDir=self.C_DATADIR.fromParam(self.config)
    if not os.path.isdir(dataBaseDir):
      os.makedirs(dataBaseDir)
    if not os.path.isdir(dataBaseDir):
      raise Exception("unable to create data dir %s"%dataBaseDir)    
    for mdir in self.DATADIRS:
      mdir=os.path.join(dataBaseDir,mdir)
      if not os.path.isdir(mdir):
        os.makedirs(mdir)
      if not os.path.isdir(mdir):  
        raise Exception("unable to create directory %s"%mdir)
      
    processes=self.findProcessByPattern(self.EXENAME)
    own=self.filterProcessList(processes,True)
    alreadyRunning=False
    self.providerPid=-1
    if len(processes) > 0:
      if len(own) != len(processes):
        diff=filter(lambda e: not e in own,processes)
        diffstr=map(lambda e: unicode(e),diff)
        self.api.log("there are provider processes running from other users: %s",",".join(diffstr))
      if len(own) > 0:
        #TODO: handle more then one process
        self.api.log("we already see a provider running with pid %d, trying this one"%own[0][0])
        alreadyRunning=True
        self.providerPid=own[0][0]
    if not alreadyRunning:
      self.api.log("starting provider process")
      self.api.setStatus("STARTING","starting provider process %s"%self.STARTSCRIPT)
      try:
        process=self.startProvider()
        self.providerPid=process.pid
        time.sleep(5)
      except Exception as e:
        raise Exception("unable to start provider %s"%e)
    self.api.log("started with port %d"%port)
    host='localhost'
    self.baseUrl="http://%s:%d/list"%(host,port)
    self.api.registerChartProvider(self.listCharts)
    self.api.registerUserApp("http://$HOST:%d/static/%s"%(port,self.STARTPAGE),"gui/icon.png")
    reported=False
    errorReported=False
    self.api.setStatus("STARTED", "provider started with pid %d, connecting at %s" %(self.providerPid,self.baseUrl))
    while sequence == self.changeSequence:
      responseData=None
      try:
        response=urlopen(self.baseUrl,timeout=10)
        if response is None:
          raise Exception("no response on %s"%self.baseUrl)
        responseData=json.loads(response.read())
        if responseData is None:
          raise Exception("no response on %s"%self.baseUrl)
        status=responseData.get('status')
        if status is None or status != 'OK':
          raise Exception("invalid status from provider query")
        self.chartList=responseData['data']
      except:
        self.api.debug("exception reading from provider %s"%traceback.format_exc())
        self.connected=False
        filteredList=self.filterProcessList(self.findProcessByPattern(self.EXENAME),True)
        if len(filteredList) < 1:
          if self.isPidRunning(self.providerPid):
            self.api.debug("final executable not found, but started process is running, wait")
          else:
            self.api.setStatus("STARTED", "restarting provider")
            self.api.log("no running provider found, trying to start")
            #just see if we need to kill some old child...
            self.stopProvider()
            try:
              process=self.startProvider()
              self.providerPid=process.pid
              self.api.setStatus("STARTED", "provider restarted with pid %d, trying to connect at %s"%(self.providerPid,self.baseUrl))
            except Exception as e:
              self.api.error("unable to start provider: %s"%traceback.format_exc())
              self.api.setStatus("ERROR", "unable to start provider %s"%e)
        else:
          self.providerPid=filteredList[0][0]
          self.api.setStatus("STARTED","provider started with pid %d, trying to connect at %s" % (self.providerPid, self.baseUrl))
        if reported:
          if not errorReported:
            self.api.error("lost connection at %s"%self.baseUrl)
            errorReported=True
          reported=False
          self.api.setStatus("ERROR","lost connection at %s"%self.baseUrl)
        time.sleep(1)
        continue
      errorReported=False
      self.connected=True
      if not reported:
        self.api.log("got first provider response")
        self.api.setStatus("NMEA","provider (%d) sucessfully connected at %s"%(self.providerPid,self.baseUrl))
        reported=True
      time.sleep(1)









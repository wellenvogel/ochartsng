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
import copy
import glob
import re
import struct
import subprocess
import sys
import getopt
import os
import tempfile
import zipfile
import shutil

import senc.sqlite2senc as sqlite2senc
import senc.senc as senc

log=sqlite2senc.log

def err(txt,*args):
    print(("ERROR: "+txt)%args)
    sys.exit(1)

def convertGdal(ifile,ofile):
    log("converting %s to %s",ifile,ofile)
    proc=subprocess.run(["ogr2ogr","-f","SQLite",ofile,ifile])
    if proc.returncode != 0:
        raise Exception("converting %s to %s returned %d"%(ifile,ofile,proc.returncode))

def usage():
    print("usage: %s [-b basedir] [-d s57datadir] [-s scale] [-e none|ext|all] [-i] [-c setname] [-f finalOut] infileOrDir outfileOrDir"%sys.argv[0])

class Context:
    TMP_SUB="__tmp"
    IN_SUB="__in"
    SQL_EXT="sqlite"
    OUT_EXT="senc"
    CNV_EXT=['000','geojson']
    def __init__(self,tmpDir:tempfile.TemporaryDirectory=None,
                 zipFile: zipfile.ZipFile=None,
                 options:sqlite2senc.Options=None,
                 setname:str=None,
                 tempout: bool=False,
                 ):
        self.zipFile=zipFile
        if zipFile is not None:
            self.zipName=zipFile.filename
        else:
            self.zipName=None
        self.tmpDir=tmpDir
        self.options=options if options is not None else sqlite2senc.Options()
        self.setname=setname
        self.ignoreErrors=self.options.ignoreErrors
        self.errors=[]
        self.numFiles=0
        self.tempout=tempout
    def openZip(self,filename):
        self.zipName=filename
        tmpname=filename+".tmp"
        self.zipFile=zipfile.ZipFile(tmpname,"w",compression=zipfile.ZIP_DEFLATED)
    def getTmpDir(self,subDir=None):
        if self.tmpDir is None:
            self.tmpDir=tempfile.TemporaryDirectory()
        rt=self.tmpDir.name
        if subDir is not None:
            rt=os.path.join(rt,subDir)
            if not os.path.exists(rt):
                os.makedirs(rt)
                if not os.path.isdir(rt):
                    raise Exception("unable to create %s"%rt)
        return rt
    def addToZip(self,fileName):
        if self.zipFile is None:
            return
        arcname=os.path.basename(fileName)
        if self.setname is None:
            raise Exception("need a setname for creating zipfiles")
        arcname=self.setname+"/"+arcname
        self.zipFile.write(fileName,arcname)
    def finalize(self):
        if self.zipFile is not None:
            self.zipFile.writestr(self.setname+"/Chartinfo.txt","ChartInfo:%s\n"%self.setname)
            tmpname=self.zipFile.filename
            self.zipFile.close()
            os.replace(tmpname,self.zipName)
    def addError(self,filename,reason=None):
        self.errors.append({'name':filename,'reason':reason})
    def printErrors(self):
        if len(self.errors) < 1:
            return
        print("Errors:")
        for e in self.errors:
            print("  %s: %s",e['name'],e['reason'] or '')
    def addHandledFiles(self,filename):
        self.numFiles+=1

def handleSingleFile(context:Context,ifile:str,ofile:str):
    name,ext=os.path.splitext(os.path.basename(ifile))
    ext=ext[1:]
    options=copy.deepcopy(context.options)
    if options.basedir is None:
        options.basedir=os.path.dirname(iname)
    sqliteFile=None
    if ext in Context.CNV_EXT:
        tmpName=os.path.join(context.getTmpDir(Context.TMP_SUB),name+"."+Context.SQL_EXT)
        try:
            convertGdal(ifile,tmpName)
            if not os.path.exists(tmpName):
                raise Exception("%s not created from %s"%(tmpName,ifile))
            sqliteFile=tmpName
        except Exception as e:
            if not context.ignoreErrors:
                raise
            context.addError(iname,str(e))
            return
    elif ext == Context.SQL_EXT:
        sqliteFile=ifile
    else:
        sqlite2senc.warn("unknown file type %s",iname)
        context.addError(iname,"unknown file type")
        return
    sqlite2senc.main(sqliteFile,ofile,options)
    context.addToZip(ofile)
    context.addHandledFiles(ifile)


M_FILES=0
M_DIR=1
if __name__ == '__main__':
    s57dir=os.path.join(os.path.dirname(__file__),"senc","s57static")
    basedir=None
    scale=None
    optlist,args=getopt.getopt(sys.argv[1:],"b:d:s:e:ic:f:")
    options=sqlite2senc.Options()
    setname=None
    finalOut=None
    emodes={
        'none': senc.SencFile.EM_NONE,
        'ext': senc.SencFile.EM_EXT,
        'all': senc.SencFile.EM_ALL
    }
    for o,a in optlist:
        if o=='-d':
            options.s57dir=a
        elif o == '-b':
            options.basedir=a
        elif o == '-s':
            options.scale=int(a)
        elif o == '-e':
            if not a in emodes.keys():
                err("invalid -e parameter %s (%s)",a,'|'.join(emodes.keys()))
            options.em=emodes[a]
        elif o == '-i':
            options.ignoreErrors=True
        elif o == '-c':
            setname=a
        elif o == '-f':
            finalOut=a
        else:
            err("invalid option %s",o)
    if not os.path.isdir(s57dir):
        err("s57dir %s not found",s57dir)
    if len(args) < 2:
        usage()
        err("missing parameters")
    log("running with args %s"," ".join(sys.argv[1:]))    
    baseDirArg=options.basedir is not None
    iname=args[0]
    if not os.path.exists(iname):
        err("%s not found",iname)
    oname=args[1]
    origOname=oname
    mode=M_FILES
    context=Context(options=options,setname=setname)
    if os.path.isdir(iname):
        mode=M_DIR
    ifname,iext=os.path.splitext(iname)
    if iext.upper()=='.ZIP':
        mode=M_DIR
        inzip=zipfile.ZipFile(iname,"r")
        unzipDir=context.getTmpDir(Context.IN_SUB)
        log("extracting %s to %s",iname,unzipDir)
        inzip.extractall(unzipDir)
        iname=unzipDir
    name,ext=os.path.splitext(oname)
    if ext.upper()==".ZIP":
        if os.path.isdir(oname):
            err("zip outfile %s exists as an directory",oname)
        if setname is None:
            setname,ext=os.path.splitext(os.path.basename(oname))
            context.setname=setname
        context.openZip(oname)
        if mode == M_DIR:
            oname=context.getTmpDir(Context.TMP_SUB)
        else:
            oname=os.path.join(context.getTmpDir(Context.TMP_SUB),ifname+"."+Context.OUT_EXT)
    else:
        if os.path.exists(oname):
            if mode == M_DIR and not os.path.isdir(oname):
                err("found existing outfile %s but expected a directory",oname)
            if mode == M_FILES and os.path.isdir(oname):
                base,dummy=os.path.splitext(os.path.basename(iname))
                oname=os.path.join(oname,base+".senc")
                log("creating outfile %s",oname)
        if mode == M_DIR and not os.path.exists(oname):
            log("creating directory %s",oname)
            os.makedirs(oname)
            if not os.path.isdir(oname):
                err("unable to create directory %s",oname)
    if mode == M_FILES:
        handleSingleFile(context,iname,oname)
    else:
        KNOWN_EXT=Context.CNV_EXT+[Context.SQL_EXT]
        for file in glob.glob(iname+"/**",recursive=True):
            if os.path.isdir(file):
                continue
            name,ext=os.path.splitext(file)
            ext=ext[1:]
            basename,dummy=os.path.splitext(os.path.basename(file))
            ofile=os.path.join(oname,basename+"."+Context.OUT_EXT)
            if not baseDirArg:
                options.basedir=os.path.dirname(file)
            if ext in KNOWN_EXT:
                log("handling file %s",file)
                handleSingleFile(context,file,ofile)
    context.finalize()
    context.printErrors()
    log("created %s with %d files",origOname,context.numFiles)
    if finalOut is not None:
        try:
            fodir=os.path.dirname(finalOut)
            if fodir != "" and not os.path.exists(fodir):
                os.makedirs(fodir)
            if os.path.exists(finalOut) and os.path.isdir(finalOut):
                shutil.rmtree(finalOut,ignore_errors=True)
            os.replace(origOname,finalOut)
            log("renamed %s to %s",origOname,finalOut)
        except Exception as e:
            log("WARNING: unable to rename %s to %s",origOname,finalOut,str(e))
            try:
                if os.path.isdir(origOname):
                    shutil.rmtree(origOname,ignore_errors=True)
                else:
                    os.unlink(origOname)
            except:
                pass
    sys.exit(1 if len(context.errors)>0 else 0)

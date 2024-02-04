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
import glob
import re
import struct
import subprocess
import sys
import getopt
import os
import tempfile

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
    print("usage: %s [-b basedir] [-d s57datadir] [-s scale] [-e none|ext|all] [-i] infileOrDir outfileOrDir",sys.argv[0])
M_FILES=0
M_DIR=1
if __name__ == '__main__':
    s57dir=os.path.join(os.path.dirname(__file__),"..","provider","s57static")
    basedir=None
    scale=None
    optlist,args=getopt.getopt(sys.argv[1:],"b:d:s:e:i")
    options=sqlite2senc.Options()
    ignoreErrors=False
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
            ignoreErrors=True
        else:
            err("invalid option %s",o)
    if not os.path.isdir(s57dir):
        err("s57dir %s not found",s57dir)
    if len(args) < 2:
        usage()
        err("missing parameters")
    baseDirArg=options.basedir is not None
    iname=args[0]
    if not os.path.exists(iname):
        err("%s not found",iname)
    oname=args[1]
    mode=M_FILES
    errors=[]
    if os.path.isdir(iname):
        mode=M_DIR
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
    tmpDir=None
    if mode == M_FILES:
        name,ext=os.path.splitext(os.path.basename(iname))
        ext=ext[1:]
        if not baseDirArg:
            options.basedir=os.path.dirname(iname)
        if ext == 'sqlite':
            sqlite2senc.main(iname,oname,options)
        elif ext == '000' or ext == 'geojson':
            tmpDir=tempfile.TemporaryDirectory()
            tmpName=os.path.join(tmpDir.name,name+".sqlite")
            try:
                convertGdal(iname,tmpName)
                if not os.path.exists(tmpName):
                    raise Exception("%s not created from %s"%(tmpName,iname))
                sqlite2senc.main(tmpName,oname,options)
            except Exception as e:
                if not ignoreErrors:
                    raise
                errors.append(iname)
        else:
            err("unknown file type %s",iname)
    else:
        KNOWN_EXT=['sqlite','000','geojson']
        for file in glob.glob(iname+"/**",recursive=True):
            if os.path.isdir(file):
                continue
            name,ext=os.path.splitext(file)
            ext=ext[1:]
            basename,dummy=os.path.splitext(os.path.basename(file))
            ofile=os.path.join(oname,basename+".senc")
            if not baseDirArg:
                options.basedir=os.path.dirname(file)
            if ext in KNOWN_EXT:
                log("handling file %s",file)
                ifile=file
                if ext != 'sqlite':
                    if tmpDir is None:
                        tmpDir=tempfile.TemporaryDirectory()
                    tmpFile=os.path.join(tmpDir.name,basename+".sqlite")
                    try:
                        convertGdal(file,tmpFile)
                        if not os.path.exists(tmpFile):
                            raise Exception("%s does not exist after conversion from %s"%(tmpFile,file))
                        sqlite2senc.main(tmpFile,ofile,options)
                    except Exception as e:
                        if not ignoreErrors:
                            raise
                        else:
                            errors.append(file)
                else:
                    sqlite2senc.main(file,ofile,options)
    if len(errors) > 0:
        print("ERRORS in files:","\n".join(errors))
    log("done")

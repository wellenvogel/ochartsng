/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Service
 * Author:   Andreas Vogel
 *
 ***************************************************************************
 *   Copyright (C) 2022 by Andreas Vogel   *
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
 */
package de.wellenvogel.ochartsprovider;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

// Used to load the 'native-lib' library on application startup.
class OutputHandler implements Runnable{
    private final InputStream is;
    private final File outFile;
    OutputHandler(File outFile, InputStream is){
        this.is=is;
        this.outFile=outFile;
    }
    @Override
    public void run() {
        Log.i(Constants.PRFX,"output handler started for "+outFile.getName());
        byte[] buffer = new byte[1024];
        try {
            FileOutputStream fos = new FileOutputStream(outFile);
            int rd=0;
            while ((rd=is.read(buffer)) > 0){
                fos.write(buffer,0,rd);
            }
        }catch (Exception e){
            Log.e(Constants.PRFX,"unable to log to "+outFile.getName(),e);
        }
        Log.i(Constants.PRFX,"output handler finished for "+outFile.getName());
    }
}

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
import android.widget.Button;
import android.widget.TextView;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class ProcessHandler {
    private Process process = null;
    String name = null;
    String logFile = null;
    Context ctx;
    ProcessState state=new ProcessState();
    HashMap<String,String> environment=new HashMap<>();
    ProcessHandler(Context ctx,String name, String logFile) {
        this.name = name;
        this.logFile = logFile;
        this.ctx=ctx;
        isAlive();
    }
    void setEnv(String key,String val){
        environment.put(key,val);
    }
    void unsetEnv(String key){
        environment.remove(key);
    }
    String translateExitValue(int ev){
        return "Error: "+ev;
    }
    boolean isAlive(){
        if (process != null){
            try{
                state.exitValue=process.exitValue();
                if (state.exitValue != 0){
                    state.error=translateExitValue(state.exitValue);
                }
                state.isRunning=false;
            }catch (IllegalThreadStateException e){
                state.isRunning=true;
                return true;
            }
            catch(Exception e){
                state.error=e.getMessage();
                state.isRunning=false;
                Log.e(Constants.PRFX,"check process "+name+": ",e);
            }
        }
        else{
            state.isRunning=false;
        }
        return false;
    }

    ProcessState getState(){
        isAlive();
        return state;
    }
    void stop() {
        if (process == null) return;
        try {
            process.destroy();
            isAlive();
            process=null;
        } catch (Exception e) {
            Log.e(Constants.PRFX, "unable to stop process " + name + ": ", e);
        }
        isAlive();
    }
    void start(List<String> args) {
        Log.i(Constants.PRFX, "start exe");
        stop();
        String libDirName = ctx.getApplicationInfo().nativeLibraryDir;
        String exeName = libDirName + File.separator + name;
        ArrayList<String> cmd=new ArrayList<>();
        cmd.add(exeName);
        cmd.addAll(args);
        ProcessBuilder pb = new ProcessBuilder(cmd);
        Map<String, String> env = pb.environment();
        env.put("LD_LIBRARY_PATH", libDirName);
        for (String ek :environment.keySet()){
            env.put(ek,environment.get(ek));
        }
        pb.redirectErrorStream(true);
        try {
            state=new ProcessState();
            process = pb.start();
            OutputHandler oh = new OutputHandler(ctx, process.getInputStream(), logFile);
            Thread oth = new Thread(oh);
            oth.start();
        } catch (Exception e) {
            isAlive();
            state.error=e.getMessage();
            Log.e(Constants.PRFX, "unable to start " + exeName + ":", e);
            process = null;
            return;
        }
        isAlive();
    }

}

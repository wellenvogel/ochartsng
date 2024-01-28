package de.wellenvogel.ochartsprovider;

import android.content.Context;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

class LibHandler extends ProcessHandler {
    LibHandler(Context ctx,String name, String logFile) {
        super(ctx, name, logFile);
    }

    @Override
    void stop() {
        stopNative();
    }

    @Override
    void start(List<String> args) {
        Thread runner=new Thread(new Runnable() {
            @Override
            public void run() {
                state=new ProcessState();
                String libDirName = ctx.getApplicationInfo().nativeLibraryDir;
                String exeName = libDirName + File.separator + name;
                ArrayList<String> cmd=new ArrayList<>();
                cmd.add(exeName);
                cmd.addAll(args);
                String[] sparam=cmd.toArray(new String[0]);
                state.isRunning=true;
                state.exitValue=runProvider(sparam);
                if (state.exitValue != 0) {
                    state.error="Error "+state.exitValue;
                }
                state.isRunning=false;
            }
        });
        runner.setDaemon(true);
        runner.setName("Provider");
        runner.start();
    }

    @Override
    boolean isAlive() {
        state.isRunning=isNativeRunning();
        return state.isRunning;
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    protected native int runProvider(String[] param);
    protected native boolean isNativeRunning();
    protected native void stopNative();
}
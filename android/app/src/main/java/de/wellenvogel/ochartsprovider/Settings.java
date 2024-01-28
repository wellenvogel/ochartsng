package de.wellenvogel.ochartsprovider;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;
import android.widget.Toast;

import androidx.preference.PreferenceManager;

public class Settings {
    public static class SettingsException extends Exception {
        public SettingsException(String reason) {
            super(reason);
        }
    };
    private int port;
    private int debugLevel;
    private boolean testMode;
    private int shutdownSec;
    private int memoryPercent;

    public boolean isAlternateKey() {
        return alternateKey;
    }

    private boolean alternateKey=false;

    public String getExternalKey() {
        return externalKey;
    }

    private String externalKey;


    public int getPort() {
        return port;
    }

    public int getDebugLevel() {
        return debugLevel;
    }

    public boolean isTestMode() {
        return testMode;
    }

    public int getShutdownSec() {
        return shutdownSec;
    }

    public int getMemoryPercent() {
        return memoryPercent;
    }
    private void setDefaults(Context ctx) {
        try {
            setFrom(new Getter() {
                @Override
                public String getString(String id, String defv) {
                    return defv;
                }

                @Override
                public boolean getBool(String id, boolean defv) {
                    return defv;
                }

                @Override
                public int getInt(String id, int defv) {
                    return defv;
                }
            },ctx);
        } catch (SettingsException e) {
            Log.e(Constants.PRFX,"unable to set default settings");
        }
    }

    public interface Getter{
        String getString(String id, String defv);
        boolean getBool(String id, boolean defv);
        int getInt(String id, int defv);
    }

    private void checkMinMax(String title, int val, int min, int max) throws SettingsException {
        if (val < min || val > max){
            throw new SettingsException(title+" "+val+" is out of range "+min+"..."+max);
        }
    }
    private void checkMinMax(String title,Context ctx,int val,int minId,int maxId) throws SettingsException {
        int min=ctx.getResources().getInteger(minId);
        int max=ctx.getResources().getInteger(maxId);
        checkMinMax(title,val,min,max);
    }
    private void setFrom(Getter getter, Context ctx) throws SettingsException{
        port=Integer.parseInt(getter.getString(ctx.getString(R.string.s_port),ctx.getString(R.string.s_port_default)));
        checkMinMax("port",port,1024, ((1 << 16) - 1));
        debugLevel=Integer.parseInt(getter.getString(ctx.getString(R.string.s_debug),ctx.getString(R.string.s_debug_default)));
        checkMinMax("debugLevel",debugLevel,0,2);
        testMode=getter.getBool(ctx.getString(R.string.s_testmode),ctx.getResources().getBoolean(R.bool.s_testmode_default));
        memoryPercent=getter.getInt(ctx.getString(R.string.s_memory),ctx.getResources().getInteger(R.integer.s_memory_default));
        checkMinMax("memory",ctx,memoryPercent,R.integer.s_memory_min,R.integer.s_memory_max);
        shutdownSec=getter.getInt(ctx.getString(R.string.s_shutdown),ctx.getResources().getInteger(R.integer.s_shutdown_default));
        checkMinMax("shutdown",ctx,shutdownSec,R.integer.s_shutdown_min,R.integer.s_shutdown_max);
        alternateKey=getter.getBool(ctx.getString(R.string.s_altkey),false);
        externalKey=getter.getString(ctx.getString(R.string.s_extkey),"");
        if (alternateKey && externalKey.isEmpty()){
            throw new SettingsException("empty alternate key");
        }
    }
    private Settings(){}
    public static Settings getSettings(SharedPreferences prefs, Context ctx) throws SettingsException {
        return getSettings(new Getter() {
            @Override
            public String getString(String id, String defv) {
                return prefs.getString(id,defv);
            }

            @Override
            public boolean getBool(String id, boolean defv) {
                return prefs.getBoolean(id,defv);
            }

            @Override
            public int getInt(String id, int defv) {
                return prefs.getInt(id,defv);
            }
        },ctx);
    }
    public static Settings getSettings(Getter getter,Context ctx) throws SettingsException {
        Settings rt=new Settings();
        rt.setFrom(getter,ctx);
        return rt;
    }
    public static Settings getSettings(Context ctx, boolean show){
        try {
            return getSettings(PreferenceManager.getDefaultSharedPreferences(ctx),ctx);
        }catch (Throwable t){
            if (show) {
                Toast.makeText(ctx, "invalid setting " + t.getMessage() + ", using defaults", Toast.LENGTH_LONG).show();
            }
        }
        Settings rt=new Settings();
        rt.setDefaults(ctx);
        return rt;
    }
}

package de.wellenvogel.ochartsprovider;

public class Constants {
    public static final String LOGFILE="logname";
    public static final String BC_STOPAPPL = BuildConfig.APPLICATION_ID+".STOPAPPL";
    public static final String BC_HEARTBEAT = BuildConfig.APPLICATION_ID+".HEARTBEAT";
    public static final String BC_SEND="de.wellenvogel.avnav.PLUGIN";

    static final String POUT="providerStdout.log";
    static final String PLOG="provider.log";
    static final String EXE="libAvnavOchartsProvider.so";
    static final String ASSET_ROOT="assets"; //unpacked data root
    static final String LOGDIR="log"; //below filesDir
    static final String PRFX="Ocharts";
    static final String STARTPAGE="/static/index.html";
    static final String PREF_UNIQUE_ID = "uniqueId" ;

    static final String EXTRA_TXT="permissionText";
    static final String EXTRA_PERMISSIONS="permissions";
    static final int SAVE_KEY_REQ=1000;
    static final String KEY_FILE_NAME="ochartsng.key";
}

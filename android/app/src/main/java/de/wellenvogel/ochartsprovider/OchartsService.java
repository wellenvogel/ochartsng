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

import android.Manifest;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.media.MediaDrm;
import android.os.Binder;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.SystemClock;
import android.provider.Settings;
import android.util.Log;
import android.widget.RemoteViews;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.util.UUID;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import androidx.annotation.RequiresApi;
import androidx.core.app.NotificationCompat;
import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceManager;


public class OchartsService extends Service implements ChartListFetcher.ResultHandler {
    static String tprfx=(BuildConfig.BUILD_TYPE.equals("release"))?"":BuildConfig.BUILD_TYPE+":";
    static final String CHANNEL_ID = "ocharts";
    public static final int NOTIFY_ID = 1;
    private BroadcastReceiver broadCastReceiverStop;
    private BroadcastReceiver broadCastReceiverAvNav;
    private String aParameter=null;
    private boolean isRunning;
    private long lastTrigger = 0;
    private long shutdownInterval=0;
    boolean preventShutdown=false;
    long timerSequence = 0;
    private static final long timerInterval = 1000;
    private static final long broadcastInterval = 4000;
    private long lastBroadcast=0;
    private final Handler handler = new Handler();
    private class ChartListInfo {
        JSONArray chartList = null;
        String chartListError;
    };
    ChartListInfo chartList;
    long lastChartList = 0;
    private final Object chartListLock=new Object();
    ChartListFetcher fetcher=null;

    ProcessHandler processHandler;
    private final IBinder binder = new LocalBinder();
    int port;

    static {
        if (!BuildConfig.AVNAV_EXE) {
            System.loadLibrary("AvnavOchartsProvider");
        }
    }
    boolean permissionsRequested=false;

    @Override
    public void storeResult(JSONArray o, String error) {
        ChartListInfo newInfo=new ChartListInfo();
        newInfo.chartList=o;
        newInfo.chartListError=error;
        if (error != null){
            Log.e(Constants.PRFX,"error reading chart list:"+error);
        }
        else{
            Log.d(Constants.PRFX,"chartList: "+o.toString());
        }
        synchronized (chartListLock){
            chartList=newInfo;
            lastChartList=SystemClock.uptimeMillis();
        }
        handler.post(new Runnable() {
            @Override
            public void run() {
                broadcastInfo();
            }
        });
    }

    public class LocalBinder extends Binder {
        OchartsService getService() {
            // Return this instance of LocalService so clients can call public methods
            return OchartsService.this;
        }
    }

    public OchartsService() {
    }

    private void createNotificationChannel() {
        // Create the NotificationChannel, but only on API 26+ because
        // the NotificationChannel class is new and not in the support library
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            CharSequence name = getString(R.string.channel_name);
            String description = getString(R.string.channel_description);
            int importance = NotificationManager.IMPORTANCE_DEFAULT;
            NotificationChannel channel = new NotificationChannel(CHANNEL_ID, name, importance);
            channel.setDescription(description);
            channel.setSound(null, null);
            // Register the channel with the system; you can't change the importance
            // or other notification behaviors after this
            NotificationManager notificationManager = getSystemService(NotificationManager.class);
            notificationManager.createNotificationChannel(channel);
        }
    }
    private boolean notificationStarted=false;
    private void startNotification(boolean startForeground, int port) {
        if (! checkPermissions(false)) {
            notificationStarted=false;
            return;
        }
        if (notificationStarted) return;
        notificationStarted=true;
        createNotificationChannel();
        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent contentIntent = PendingIntent.getActivity(this, 0,
                notificationIntent, buildPiFlags(PendingIntent.FLAG_UPDATE_CURRENT,false));
        Intent broadcastIntentStop = new Intent();
        broadcastIntentStop.setAction(Constants.BC_STOPAPPL);
        PendingIntent stopAppl = PendingIntent.getBroadcast(this, 1, broadcastIntentStop, buildPiFlags(PendingIntent.FLAG_CANCEL_CURRENT,true));
        String nTitle=getString(R.string.notifyTitle);
        if (BuildConfig.BUILD_TYPE.equals("debug")) nTitle+="-debug";
        if (BuildConfig.BUILD_TYPE.equals("beta")) nTitle+="-beta";
        NotificationCompat.Builder notificationBuilder =
                new NotificationCompat.Builder(this, CHANNEL_ID);
        notificationBuilder.setSmallIcon(R.mipmap.ic_launcher);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
            notificationBuilder.setContentTitle(nTitle);
            notificationBuilder.setContentText("port " + Integer.toString(port));
        }
        notificationBuilder.setContentIntent(contentIntent);
        notificationBuilder.setOngoing(true);
        notificationBuilder.setAutoCancel(false);
        notificationBuilder.addAction(R.drawable.close_black48,"Close",stopAppl);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            notificationBuilder.setVisibility(NotificationCompat.VISIBILITY_PUBLIC);
        }
        NotificationManager mNotificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (startForeground) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE){
                startForeground(NOTIFY_ID,notificationBuilder.build(), ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
            }
            else {
                startForeground(NOTIFY_ID, notificationBuilder.build());
            }
        } else {
            mNotificationManager.notify(NOTIFY_ID, notificationBuilder.build());
        }
    }

    void stopNotification() {
        notificationStarted=false;
        NotificationManager mNotificationManager =
                (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        mNotificationManager.cancel(NOTIFY_ID);
        try {
            stopForeground(true);
        } catch (Throwable t) {
        }
    }

    private boolean checkPermissions(boolean request){
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                if (request) {
                    if(permissionsRequested) return true;
                    permissionsRequested=true;
                    PermissionActivity.runPermissionRequest(this, new String[]{Manifest.permission.POST_NOTIFICATIONS}, R.string.needNotifications);
                }
                return false;

            }
        }
        return true;
    }


    public ProcessState getProcessState(boolean trigger){
        if (trigger) {
            lastTrigger = SystemClock.uptimeMillis();
        }
        if (processHandler != null) return processHandler.getState();
        return new ProcessState();
    }

    @Override
    public void onCreate() {
        super.onCreate();
        IntentFilter filter=new IntentFilter(Constants.BC_STOPAPPL);
        broadCastReceiverStop =new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Log.i(Constants.PRFX,"received stop appl");
                shutDown();
            }
        };
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(broadCastReceiverStop,filter, RECEIVER_EXPORTED);
        }
        else{
            registerReceiver(broadCastReceiverStop,filter);
        }
        IntentFilter filterAvNav=new IntentFilter(Constants.BC_HEARTBEAT);
        broadCastReceiverAvNav =new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                Log.i(Constants.PRFX,"received heartbeat notification");
                lastTrigger= SystemClock.uptimeMillis();
            }
        };
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(broadCastReceiverAvNav, filterAvNav,RECEIVER_EXPORTED);
        }
        else{
            registerReceiver(broadCastReceiverAvNav, filterAvNav);
        }
        isRunning=false;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (processHandler != null){
            processHandler.stop();
        }
        isRunning=false;
        unregisterReceiver(broadCastReceiverStop);
        unregisterReceiver(broadCastReceiverAvNav);
        stopNotification();
        timerSequence++;
    }

    public static String getOCPNWVID() {
        String rv = "";
        MediaDrm mediaDrm;
        final UUID WIDEVINE_UUID = UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");
        try {
            mediaDrm = new MediaDrm(WIDEVINE_UUID);
        } catch (Exception e) {
            return rv;
        }
        byte[] deviceUniqueIdArray = mediaDrm.getPropertyByteArray(MediaDrm.PROPERTY_DEVICE_UNIQUE_ID);
        StringBuilder sb = new StringBuilder();
        for (byte b : deviceUniqueIdArray) {
            sb.append(String.format("%02X", b));
        }
        return sb.toString();
    }
    private static String getUUID(Context ctx){
        final String androidId = Settings.Secure.getString(
                ctx.getContentResolver(), Settings.Secure.ANDROID_ID);
        if(androidId.isEmpty())
            return "";

        final UUID uuid;
        try{
            uuid = UUID.nameUUIDFromBytes(androidId.getBytes("utf8"));
        } catch (UnsupportedEncodingException e) {
            throw new RuntimeException(e);
        }
        return uuid.toString();
    }
    private String getAParameter(){
        if (aParameter != null){
            //we have an imported Identity
            return aParameter;
        }
        if (Build.VERSION.SDK_INT < 29){
            return "-z:"+getUUID(this);
        }
        else{
            return "-y:"+getOCPNWVID()+":-z:"+getUUID(this);
        }
    }

    public static String getDefaultAParameter(Context ctx){
        if (Build.VERSION.SDK_INT < 29){
            return "-z:"+getUUID(ctx);
        }
        else{
            return "-y:"+getOCPNWVID()+":-z:"+getUUID(ctx);
        }
    }

    public static String getOCPNuniqueID(Context ctx) {
        SharedPreferences sharedPrefs = ctx.getSharedPreferences(Constants.PREF_UNIQUE_ID, Context.MODE_PRIVATE);
        String OCPNuniqueID = sharedPrefs.getString(Constants.PREF_UNIQUE_ID, null);
        if (OCPNuniqueID == null) {
            OCPNuniqueID = UUID.randomUUID().toString();
            SharedPreferences.Editor editor = sharedPrefs.edit();
            editor.putString(Constants.PREF_UNIQUE_ID, OCPNuniqueID);
            editor.commit();
        }
        return OCPNuniqueID;
    }

    /**
     * same code like in OpenCPN
     * @return
     */
    public static String getSystemName( Context ctx){
        String name = android.os.Build.DEVICE;
        if(name.length() > 10)
            name = name.substring(0, 9);
        String UUID = getUUID(ctx);
        String OCPNUUID = getOCPNuniqueID(ctx);
        String OCPNWVID = getOCPNWVID();
        String selID;
        if( (UUID != null) && (UUID.length() > 0) && ( Build.VERSION.SDK_INT < 29) )   // Strictly less than Android 10
            selID = UUID;
        else if(OCPNWVID != null && ! OCPNWVID.isEmpty())
            selID = OCPNWVID;
        else
            selID = OCPNUUID;
        // Get a hash of the supplied string
        int uuidHash = -(selID.hashCode());
        // And take the mod(10000) result
        int val = Math.abs(uuidHash) % 10000;
        name += String.valueOf(val);

        Pattern replace = Pattern.compile("[-_/()~#%&*{}:;?|<>!$^+=@ ]");
        Matcher matcher2 = replace.matcher(name);
        name = matcher2.replaceAll("0");

        name = name.toLowerCase(Locale.ROOT);
        return name+"a";

    }

    private static boolean recursiveDelete(File file, boolean deleteRoot) {
        // Recursively delete all contents.
        File[] files = file.listFiles();

        if (files != null) {
            for (File childFile:files) {
                if (childFile.isDirectory()) {
                    if (!recursiveDelete(childFile,true)) {
                        return false;
                    }
                } else {
                    if (!childFile.delete()) {
                        return false;
                    }
                }
            }
        }
        if (deleteRoot) {
            if (!file.delete()) {
                return false;
            }
        }
        return true;
    }

    private boolean copyAssetDir(String assetDir,File targetSub){
        try{
            String [] assetFiles=getAssets().list(assetDir);
            if (assetFiles.length < 1) return false;
            if (! targetSub.isDirectory()){
                targetSub.mkdirs();
            }
            for (String assetFile:assetFiles){
                String assetPath=assetDir+"/"+assetFile;
                Log.i(Constants.PRFX,"copy asset file "+assetPath);
                File target=new File(targetSub,assetFile);
                if (copyAssetDir(assetPath,target)){
                    continue;
                }
                InputStream is=getAssets().open(assetPath);
                OutputStream os=new FileOutputStream(target);
                int BUFFERSIZE=10*1024;
                byte []buffer=new byte[BUFFERSIZE];
                int rd=0;
                while ((rd=is.read(buffer,0,BUFFERSIZE)) > 0){
                    os.write(buffer,0,rd);
                }
                is.close();
                os.close();
            }
            return true;
        }catch (IOException e){

        }
        return false;
    }
    private void copyAssets() throws Exception {
        String[] subDirs=new String[]{BuildConfig.ASSETS_GUI,BuildConfig.ASSETS_S57, BuildConfig.ASSETS_PLUGIN};
        File targetBase=new File(getFilesDir(), Constants.ASSET_ROOT);
        if (!targetBase.isDirectory()){
            targetBase.mkdirs();
        }
        if (!targetBase.isDirectory()){
            throw new Exception("Unable to create "+targetBase.getAbsolutePath());
        }
        for (String sub:subDirs){
            Log.i(Constants.PRFX,"asset copy "+sub);
            File targetSub=new File(targetBase,sub);
            if (! targetSub.isDirectory()){
                targetSub.mkdirs();
                if (! targetSub.isDirectory()){
                    throw new Exception("unable to create "+targetSub.getAbsolutePath());
                }
            }
            else{
                recursiveDelete(targetSub,false);
            }
            copyAssetDir(sub,targetSub);
        }
    }
    boolean startUp(){
        try {
            try{
                copyAssets();
            }catch (Throwable t){
                Toast.makeText(this,tprfx+"unable to copy assets for ocharts: "+t.getMessage(),Toast.LENGTH_LONG).show();
                return false;
            }
            checkPermissions(true);
            de.wellenvogel.ochartsprovider.Settings settings= de.wellenvogel.ochartsprovider.Settings.getSettings(this,true);
            port = settings.getPort();
            shutdownInterval = settings.getShutdownSec()*1000;
            int debugLevel = settings.getDebugLevel();
            boolean testMode=settings.isTestMode();
            String pid=settings.getExternalKey();
            int memPercent=settings.getMemoryPercent();
            if (settings.isAlternateKey() && ! pid.isEmpty()){
                aParameter=pid;
            }
            else{
                aParameter=null;
            }
            Log.i(Constants.PRFX,"starting provider, port="+port+", debug="+debugLevel+", testMode="+testMode+", alt key="+settings.isAlternateKey());
            if (BuildConfig.AVNAV_EXE){
                processHandler =new ProcessHandler(this,Constants.EXE,Constants.LOGDIR+"/"+Constants.POUT);
            }
            else{
                processHandler =new LibHandler(this,Constants.EXE,Constants.LOGDIR+"/"+Constants.POUT);
            }
            File logDir=new File(getFilesDir(),Constants.LOGDIR);
            if (! logDir.isDirectory()){
                logDir.mkdirs();
            }
            if (!logDir.isDirectory()){
                Toast.makeText(this, tprfx+"cannot create logdir "+logDir.getAbsolutePath(),Toast.LENGTH_LONG).show();
                return false;
            }
            ArrayList<String> args=new ArrayList<>();
            if (BuildConfig.AVNAV_EXE){
                args.add("-p");
                args.add(Integer.toString(android.os.Process.myPid()));
            }
            args.addAll(Arrays.asList("-l", getFilesDir().getAbsolutePath(),
                    "-a",getAParameter(),
                    "-b", getSystemName(this),
                    "-l", getFilesDir().getAbsolutePath()+"/"+Constants.LOGDIR,
                    "-d",Integer.toString(debugLevel),
                    "-x",Integer.toString(memPercent),
                    "-g",getFilesDir().getAbsolutePath()+"/"+ Constants.ASSET_ROOT+"/"+BuildConfig.ASSETS_GUI,
                    "-t",getFilesDir().getAbsolutePath()+"/"+ Constants.ASSET_ROOT+"/"+BuildConfig.ASSETS_S57,
                    getFilesDir().getAbsolutePath(),
                    Integer.toString(port))
            );
            if (testMode){
                processHandler.setEnv("AVNAV_TEST_KEY","Decrypted");
            }
            else{
                processHandler.unsetEnv("AVNAV_TEST_KEY");
            }
            processHandler.start(args);
            ProcessState pstate=processHandler.getState();
            if (pstate.isRunning) {
                startNotification(true, port);
                broadcastInfo();
                lastTrigger = SystemClock.uptimeMillis();
                preventShutdown = false;
                fetcher = new ChartListFetcher(this, "http://127.0.0.1:" + port + "/list", 3000);
            }
            else{
                throw new Exception(pstate.error);
            }
        }catch (Throwable t){
            Log.e(Constants.PRFX,"unable to start up",t);
            ProcessState s=new ProcessState();
            s.error=t.getMessage();
            Toast.makeText(this,tprfx+"cannot start ocharts: "+s.error,Toast.LENGTH_LONG).show();
            return false;
        }
        return true;
    }

    void shutDown(){
        preventShutdown=false;
        if (processHandler !=null) processHandler.stop();
        timerSequence++;
        stopNotification();
        isRunning=false;
        if (fetcher != null) fetcher.stop();
        stopSelf();
    }
    private void broadcastInfo(){
        ProcessState pstate=getProcessState(false);
        lastBroadcast=SystemClock.uptimeMillis();
        //intentionally set lastBroadcaset before check to avoid error flood
        if (!pstate.isRunning){
            Log.i(Constants.PRFX,"provider is stopped in broadcastInfo");
            return;
        }
        try {
            String iconUrl="http://127.0.0.1:" + port+"/static/icon.png";
            Intent info = new Intent(Constants.BC_SEND);
            info.putExtra("name", "ocharts");
            info.putExtra("package", BuildConfig.APPLICATION_ID);
            info.putExtra("heartbeat",Constants.BC_HEARTBEAT);
            info.putExtra("base", PluginDataProvider.getUri(null).toString());
            info.putExtra("icon",iconUrl);
            JSONObject o = new JSONObject();
            ChartListInfo charts=null;
            synchronized (chartListLock) {
                charts = chartList;
            }
            if (charts != null && charts.chartListError == null && charts.chartList != null) {
                o.put("charts", charts.chartList);
            }
            JSONArray userApps=new JSONArray();
            JSONObject userApp=new JSONObject();
            userApp.put("url","http://127.0.0.1:" + port + Constants.STARTPAGE);
            userApp.put("name","ocharts");
            userApp.put("icon",iconUrl);
            userApps.put(userApp);
            o.put("userApps",userApps);
            info.putExtra("plugin.json", o.toString());
            sendBroadcast(info);
            Log.i(Constants.PRFX,"broadcast");
        }catch (Throwable t){
            Log.e(Constants.PRFX,"error sending broadcast ",t);
        }
    }
    void timerAction() throws JSONException{
        if (processHandler == null) return;
        ProcessState s= processHandler.getState();
        if (! s.isRunning){
            Toast.makeText(this,tprfx+"ocharts provider stopped"+(s.error == null || s.error.isEmpty()?"":": "+s.error),Toast.LENGTH_LONG).show();
            shutDown();
            return;
        }
        startNotification(true,port);
        long now=SystemClock.uptimeMillis();
        if ((lastBroadcast + broadcastInterval) <= now){
            if (fetcher != null) {
                fetcher.query(); //will trigger broadcast in result
                if ((lastBroadcast + 2 * broadcastInterval) > now){
                    return;
                }
            }
            broadcastInfo();
        }
        if (! preventShutdown && shutdownInterval != 0 && now > (lastTrigger+shutdownInterval)){
            Toast.makeText(this,tprfx+"ochart provider auto shutdown",Toast.LENGTH_SHORT).show();
            shutDown();
        }

    }

    public void switchAutoShutdown(boolean on){
        preventShutdown = ! on;
        lastTrigger=SystemClock.uptimeMillis();
    }

    private class TimerRunnable implements Runnable{
        private long sequence=0;
        TimerRunnable(long seq){sequence=seq;}
        public void run(){
            if (! isRunning) return;
            if (timerSequence != sequence) return;
            try {
                timerAction();
            } catch (JSONException e) {
                Log.e(Constants.PRFX, "error in timer", e);
            }
            handler.postDelayed(this, timerInterval);
        }
    }


    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        super.onStartCommand(intent, flags, startId);
        int license=0;
        try{
            license= PreferenceManager.getDefaultSharedPreferences(this).getInt(Constants.PREF_LICENSE_ACCEPTED,0);
        }catch (Throwable t){}
        if (license != Constants.LICENSE_VERSION) {
            Toast.makeText(this,tprfx+"License not accepted",Toast.LENGTH_LONG).show();
            stopSelf();
            return Service.START_NOT_STICKY;
        }
        if (! isRunning){
            if (startUp()) {
                isRunning = true;
                timerSequence++;
                handler.postDelayed(new TimerRunnable(timerSequence), timerInterval);
            }
        }
        return Service.START_STICKY;
    }
    @Override
    public IBinder onBind(Intent intent) {
        return binder;
    }

    public static int buildPiFlags(int flags, boolean immutable){
        int rt=flags;
        if (Build.VERSION.SDK_INT >= 31) {
            rt|=immutable? PendingIntent.FLAG_IMMUTABLE:PendingIntent.FLAG_MUTABLE;
        }
        return rt;
    }
}
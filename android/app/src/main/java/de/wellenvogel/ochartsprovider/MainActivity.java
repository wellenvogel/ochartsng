/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Main
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
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.text.Html;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;

import androidx.activity.result.ActivityResultCallback;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContract;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.PreferenceManager;

public class MainActivity extends AppCompatActivity {

    private final Handler handler=new Handler();
    ProcessState state=new ProcessState();
    OchartsService service=null;
    boolean bound=false;
    ImageView iconR;
    ImageView iconG;
    TextView pid;
    Button startButton;
    Button stopButton;
    Button launchButton;
    boolean running=false;
    ArrayList<ActivityResultLauncher<Integer>> neededQueries=new ArrayList<>();
    ActivityResultLauncher<Integer> licenseQuery=registerForActivityResult(new ActivityResultContract<Integer, Object>() {
        @Override
        public Object parseResult(int i, @Nullable Intent intent) {
            if (i != 1) {
                finish();
                return null;
            }
            PreferenceManager.getDefaultSharedPreferences(MainActivity.this).edit().putInt(Constants.PREF_LICENSE_ACCEPTED,Constants.LICENSE_VERSION).commit();
            nextQuery();
            return null;
        }

        @NonNull
        @Override
        public Intent createIntent(@NonNull Context context, Integer integer) {
            return new Intent(MainActivity.this,LicenseActivity.class);
        }
    }, new ActivityResultCallback<Object>() {
        @Override
        public void onActivityResult(Object o) {
        }
    });

    ActivityResultLauncher<Integer> permissionQuery= registerForActivityResult(new ActivityResultContract<Integer, Object>() {
        @Override
        public Object parseResult(int i, @Nullable Intent intent) {
            return null;
        }

        @NonNull
        @Override
        public Intent createIntent(@NonNull Context context, Integer integer) {
            Intent i=new Intent(MainActivity.this, PermissionActivity.class);
            i.setFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT | Intent.FLAG_ACTIVITY_MULTIPLE_TASK|Intent.FLAG_ACTIVITY_NEW_TASK);
            Bundle extra=new Bundle();
            extra.putInt(Constants.EXTRA_TXT,R.string.needNotifications);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                extra.putStringArray(Constants.EXTRA_PERMISSIONS,new String[]{Manifest.permission.POST_NOTIFICATIONS});
            }
            i.putExtras(extra);
            return i;
        }
    }, new ActivityResultCallback<Object>() {
        @Override
        public void onActivityResult(Object result) {
            nextQuery();
        }
    });

    private void nextQuery(){
        if (neededQueries.size() >0){
            neededQueries.get(0).launch(1);
            neededQueries.remove(0);
        }
    }
    String launchUrl="";
    private final Runnable timer = new Runnable() {
        @Override
        public void run() {
            if (! running) return;
            if (! bound){
                if (state.isRunning){
                    state=new ProcessState();
                }
            }
            else{
                state=service.getProcessState(true);
            }
            if (state.isRunning){
                pid.setText("running");
                iconG.setVisibility(View.VISIBLE);
                iconR.setVisibility(View.GONE);
                stopButton.setEnabled(true);
                startButton.setEnabled(false);
                launchButton.setEnabled(true);
            }
            else{
                if (state.error != null && ! state.error.isEmpty()){
                    pid.setText(state.error);
                }
                else {
                    pid.setText("stopped");
                }
                iconR.setVisibility(View.VISIBLE);
                iconG.setVisibility(View.GONE);
                startButton.setEnabled(true);
                stopButton.setEnabled(false);
                launchButton.setEnabled(false);
            }
            handler.postDelayed(timer,1000);
        }
    };

    public static SharedPreferences getPrefs(Context ctx){
        //return ctx.getSharedPreferences(Constants.PREFNAME, Context.MODE_PRIVATE);
        return PreferenceManager.getDefaultSharedPreferences(ctx);
    }
    public SharedPreferences getPrefs(){
        return getPrefs(this);
    }

    Settings loadSettings(){
        return Settings.getSettings(this,true);
    }

    static class InfoItem{
      public String key;
      public String value;
      public InfoItem(String key, String value){
          this.key=key;
          this.value=value;
      }
    };
    class InfoListAdapter extends BaseAdapter{
        private Settings settings;
        @Override
        public int getCount() {
            if ("debug".equals(BuildConfig.BUILD_TYPE)) {
                return 4;
            }
            else{
                return 3;
            }
        }

        @Override
        public Object getItem(int i) {
            switch(i){
                case 0:
                    return new InfoItem("Port",Integer.toString(settings.getPort()));
                case 1:
                    return new InfoItem("LogLevel",getResources().getStringArray(R.array.debugEntries)[settings.getDebugLevel()]);
                case 2:
                    return new InfoItem("Alt Key",settings.isAlternateKey()?"On":"Off");
                case 3:
                    return new InfoItem("Testmode",settings.isTestMode()?"On":"Off");
            }
            return null;
        }

        @Override
        public long getItemId(int i) {
            return i;
        }


        @Override
        public View getView(int i, View view, ViewGroup viewGroup) {
            if (view == null) {
                view = getLayoutInflater().inflate(android.R.layout.simple_list_item_2, viewGroup, false);
            }
            InfoItem info=(InfoItem)getItem(i);
            if (info != null) {
                TextView v = view.findViewById(android.R.id.text1);
                v.setText(info.key);
                v = view.findViewById(android.R.id.text2);
                v.setText(info.value);
            }
            return view;
        }

        public void updateSettings(Settings s){
            settings=s;
            notifyDataSetChanged();
        }
    }

    private InfoListAdapter infoListAdapter=new InfoListAdapter();
    private ListView infoView;

    private void updateFromSettings(){
        Settings s=loadSettings();
        infoListAdapter.updateSettings(s);
        launchUrl="http://127.0.0.1:"+s.getPort()+Constants.STARTPAGE;
    }
    private boolean checkPermissions(){
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PreferenceManager.setDefaultValues(this,R.xml.root_preferences,false);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setContentView(R.layout.activity_main);
        infoView=findViewById(R.id.mainSettingsInfo);
        infoView.setAdapter(infoListAdapter);
        infoView.setOnItemClickListener((adapterView, view, i, l) -> showSettings());
        updateFromSettings();
        iconR=findViewById(R.id.imageViewRed);
        iconG=findViewById(R.id.imageViewGreen);
        pid=findViewById(R.id.pid);
        startButton=findViewById(R.id.btStart);
        stopButton=findViewById(R.id.btStop);
        launchButton=findViewById(R.id.btLaunch);
        TextView intro=findViewById(R.id.labelIntro);
        intro.setText(Html.fromHtml(getString(R.string.intro)));

        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                try {
                    Intent intent = new Intent(MainActivity.this, OchartsService.class);
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        startForegroundService(intent);
                    } else {
                        startService(intent);
                    }
                }catch (Throwable t){
                    Toast.makeText(MainActivity.this,"unable to start service: "+t.getMessage(),Toast.LENGTH_LONG).show();
                }
            }
        });
        stopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (bound){
                    service.shutDown();
                }
            }
        });
        findViewById(R.id.btLog).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                File logBase=new File(getFilesDir(),Constants.LOGDIR);
                File logFile=new File(logBase,Constants.PLOG);
                Uri uri= LogProvider.getDocumentUri(LogProvider.LOG_ROOT,logFile);
                Intent li=new Intent(Intent.ACTION_SEND);
                li.putExtra(Intent.EXTRA_STREAM,uri);
                li.putExtra(Intent.EXTRA_SUBJECT,logFile.getName());
                li.setType("text/plain");
                li.addFlags(
                        Intent.FLAG_GRANT_READ_URI_PERMISSION);
                startActivity(Intent.createChooser(li, "select application"));
                /*
                Intent li=new Intent(MainActivity.this,LogActivity.class);
                li.putExtra(Constants.LOGFILE, Constants.PLOG);
                startActivity(li);
                */

            }
        });
        findViewById(R.id.btOut).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent li=new Intent(MainActivity.this,LogActivity.class);
                li.putExtra(Constants.LOGFILE, Constants.POUT);
                startActivity(li);
            }
        });
        launchButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent li=new Intent(Intent.ACTION_VIEW, Uri.parse(launchUrl));
                startActivity(li);
            }
        });
        getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        int license=0;
        try{
            license=PreferenceManager.getDefaultSharedPreferences(this).getInt(Constants.PREF_LICENSE_ACCEPTED,0);
        }catch (Throwable t){}
        if (license != Constants.LICENSE_VERSION){
            neededQueries.add(licenseQuery);
        }
        if (! checkPermissions()){
            neededQueries.add(permissionQuery);
        }
        running=true;
        nextQuery();
        handler.postDelayed(timer,1000);
    }
    @Override
    public void onResume() {
        updateFromSettings();
        super.onResume();
    }
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        running=false;
        handler.removeCallbacks(timer);
    }

    private void showSettings(){
        if (bound){
            service.shutDown();
        }
        Intent s=new Intent(MainActivity.this,SettingsActivity.class);
        startActivity(s);
    }
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch(item.getItemId()){
            case android.R.id.home:
                finish();
                return true;
            case R.id.settings:
                showSettings();
                return true;
        }
        return super.onOptionsItemSelected(item);

    }

    private ServiceConnection connection = new ServiceConnection() {



        @Override
        public void onServiceConnected(ComponentName className,
                                       IBinder ibinder) {
            // We've bound to LocalService, cast the IBinder and get LocalService instance
            Log.i(Constants.PRFX,"service bound");
            OchartsService.LocalBinder binder = (OchartsService.LocalBinder) ibinder;
            service = binder.getService();
            bound = true;
        }

        @Override
        public void onServiceDisconnected(ComponentName arg0) {
            Log.i(Constants.PRFX,"service unbound");
            bound = false;
        }
    };
    @Override
    protected void onStart() {
        super.onStart();
        Intent intent = new Intent(this, OchartsService.class);
        bindService(intent, connection, Context.BIND_AUTO_CREATE);
    }

    @Override
    protected void onStop() {
        super.onStop();
    }
}
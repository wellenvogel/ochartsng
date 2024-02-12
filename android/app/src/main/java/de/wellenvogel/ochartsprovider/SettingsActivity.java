package de.wellenvogel.ochartsprovider;

import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.preference.CheckBoxPreference;
import androidx.preference.EditTextPreference;
import androidx.preference.EditTextPreferenceDialogFragmentCompat;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.SeekBarPreference;

public class SettingsActivity extends AppCompatActivity {
    static final String FTAG="settings";
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.settings_activity);
        if (savedInstanceState == null) {
            getSupportFragmentManager()
                    .beginTransaction()
                    .replace(R.id.settings, new SettingsFragment(),FTAG)
                    .commit();
        }
        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
        }
    }

    public static class MyETDialogFragment extends EditTextPreferenceDialogFragmentCompat{
        @NonNull
        @Override
        public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
            Dialog rt= super.onCreateDialog(savedInstanceState);
            AlertDialog art=(AlertDialog) rt;
            if (BuildConfig.SHOW_IMPORT_OPENCPN) {
                art.setButton(DialogInterface.BUTTON_NEUTRAL, getString(R.string.btimport), (dialogInterface, i) -> {
                });
            }
            art.setOnShowListener(dialogInterface -> {
                //we must override the button onClick here again to prevent closing the dialog
                Button nb=art.getButton(DialogInterface.BUTTON_NEUTRAL);
                if (nb != null) nb.setOnClickListener(view -> {
                    SigningImporter importer=new SigningImporter(getContext());
                    String id = importer.retrieveIdentity();
                    if (id != null){
                        EditText et=art.findViewById(android.R.id.edit);
                        if (et != null){
                            et.setText(id);
                        }
                    }
                    else{
                        Toast.makeText(getContext(),"unable to retrieve key",Toast.LENGTH_LONG).show();
                    }
                    Log.i("ocharts","neutral clicked");
                });
            });
            return rt;
        }
    }

    public static class SettingsFragment extends PreferenceFragmentCompat implements PreferenceFragmentCompat.OnPreferenceDisplayDialogCallback {
        EditTextPreference port=null;
        static final int[] numberprefs={R.string.s_port};

        private void setSpecials(String rootKey){
            port=findPreference(getString(R.string.s_port));
            for (int i : numberprefs) {
                EditTextPreference p=findPreference(getString(i));
                if (p != null) {
                    p.setOnBindEditTextListener(new EditTextPreference.OnBindEditTextListener() {
                        @Override
                        public void onBindEditText(@NonNull EditText editText) {
                            editText.setInputType(InputType.TYPE_CLASS_NUMBER);
                        }
                    });
                }
            }
            Preference reset=findPreference(getString(R.string.s_reset));
            reset.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener() {
                @Override
                public boolean onPreferenceClick(@NonNull Preference preference) {
                    getPreferenceManager().getSharedPreferences().edit().clear().commit();
                    PreferenceManager.setDefaultValues(getActivity(),R.xml.root_preferences,true);
                    setPreferenceScreen(null);
                    setPreferencesFromResource(R.xml.root_preferences, rootKey);
                    setSpecials(rootKey);
                    return true;
                }
            });
            Preference altkey=findPreference(getString(R.string.s_altkey));
            altkey.setOnPreferenceChangeListener(new Preference.OnPreferenceChangeListener() {
                @Override
                public boolean onPreferenceChange(@NonNull Preference preference, Object newValue) {
                    Boolean nv=(Boolean)newValue;
                    AlertDialog.Builder b=new AlertDialog.Builder(getActivity());
                    b.setMessage(R.string.altkeyMsg);
                    b.setTitle(R.string.altKeyTitle);
                    b.setNegativeButton(R.string.cancel, (dialogInterface, i) ->{
                            dialogInterface.dismiss();
                    });
                    b.setPositiveButton(R.string.ok, (dialogInterface, i) -> {
                        dialogInterface.dismiss();
                        ((CheckBoxPreference)altkey).setChecked(nv);
                    });
                    b.create().show();
                    return false;
                }
            });
        }
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferencesFromResource(R.xml.root_preferences, rootKey);
            setSpecials(rootKey);
        }

        @Override
        public boolean onPreferenceDisplayDialog(@NonNull PreferenceFragmentCompat caller, @NonNull Preference pref) {
            //https://issuetracker.google.com/issues/124153118?pli=1
            if (pref.getKey().equals(getString(R.string.s_extkey))){
                MyETDialogFragment f=new MyETDialogFragment();
                f.setTargetFragment(caller,0);
                final Bundle b = new Bundle(1);
                b.putString("key", pref.getKey());
                f.setArguments(b);
                f.show(caller.getParentFragmentManager(),"AKEYPREF");
                return true;
            }
            return false;
        }
    }
    private boolean checkSettings(){
        Fragment f=getSupportFragmentManager().findFragmentByTag(FTAG);
        if (f != null && f.isVisible() && f instanceof SettingsFragment) {
            SettingsFragment fragment=(SettingsFragment) f;
            try {
                //check our settings
                Settings.getSettings(new Settings.Getter() {
                    @Override
                    public String getString(String id, String defv) {
                        Preference p=fragment.findPreference(id);
                        if (p != null){
                            if (p instanceof EditTextPreference){
                                return ((EditTextPreference) p).getText();
                            }
                            if (p instanceof ListPreference){
                                return ((ListPreference) p).getValue();
                            }
                        }
                        return defv;
                    }

                    @Override
                    public boolean getBool(String id, boolean defv) {
                        Preference p=fragment.findPreference(id);
                        if (p instanceof CheckBoxPreference){
                            return ((CheckBoxPreference) p).isChecked();
                        }
                        return defv;
                    }

                    @Override
                    public int getInt(String id, int defv) {
                        Preference p=fragment.findPreference(id);
                        if (p instanceof SeekBarPreference){
                            return ((SeekBarPreference) p).getValue();
                        }
                        return defv;
                    }
                },this);
            } catch (Throwable r) {
                Toast.makeText(this, "invalid settings: " + r.getMessage(), Toast.LENGTH_LONG).show();
                return false;
            }
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch(item.getItemId()){
            case android.R.id.home:
                if (! checkSettings()) return true;
        }
        return super.onOptionsItemSelected(item);

    }
}
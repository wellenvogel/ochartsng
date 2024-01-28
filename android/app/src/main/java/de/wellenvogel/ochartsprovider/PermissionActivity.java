package de.wellenvogel.ochartsprovider;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.provider.Settings;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;

public class PermissionActivity extends AppCompatActivity {
    public static class MyAlertDialogFragment extends DialogFragment {
        int title=0;
        @Override
        public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
            title=getArguments().getInt(Constants.EXTRA_TXT);
            AlertDialog.Builder builder= new AlertDialog.Builder(getActivity());
            builder.setMessage(R.string.grantQuestion)
                    .setTitle(getText(title)+ " "+getText(R.string.notGranted));
            builder.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialogInterface, int i) {
                    Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                    Uri uri = Uri.fromParts("package", getActivity().getPackageName(), null);
                    intent.setData(uri);
                    startActivity(intent);
                    getActivity().finish();
                }
            });
            builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialogInterface, int i) {
                    Toast.makeText(getContext(),getText(title)+" "+getText(R.string.notGranted), Toast.LENGTH_SHORT).show();
                    getActivity().finish();
                }
            });
            return builder.create();
        }
    }
    String [] permissions=null;
    int title=0;
    private final ActivityResultLauncher<String> requestPermissionLauncher=registerForActivityResult(new ActivityResultContracts.RequestPermission(), result -> {
        if (result) finish();
        if (! checkPermissionDialog())finish();
    });
    private final ActivityResultLauncher<String[]> requestMultiPermissionLauncher=registerForActivityResult(new ActivityResultContracts.RequestMultiplePermissions(), result -> {
        boolean ok=true;
        for (boolean v:result.values()){
            if (!v) ok=false;
        }
        if (ok) finish();
        if (! checkPermissionDialog()) finish();
    });

    private boolean checkPermissionDialog() {
        boolean showDialog = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            for (String perm : permissions) {
                if (!shouldShowRequestPermissionRationale(perm))
                    showDialog = true;
            }
        }
        if (showDialog) {
            MyAlertDialogFragment frag = new MyAlertDialogFragment();
            Bundle args = new Bundle();
            args.putInt(Constants.EXTRA_TXT, title);
            frag.setArguments(args);
            frag.show(getSupportFragmentManager(), "dialog");
            return true;
        }
        Toast.makeText(this,getText(title)+" "+getText(R.string.notGranted), Toast.LENGTH_SHORT).show();
        return false;
    }
    @Override
    protected void onStart() {
        super.onStart();
        Bundle extras=getIntent().getExtras();
        if (extras == null) finish();
        title=extras.getInt(Constants.EXTRA_TXT);
        permissions=extras.getStringArray(Constants.EXTRA_PERMISSIONS);
        if (title == 0 || permissions == null || permissions.length == 0) finish();
        boolean needsRequest=false;
        for (String perm:permissions){
            if (checkSelfPermission(perm) != PackageManager.PERMISSION_GRANTED){
                needsRequest=true;
                break;
            }
        }
        if (! needsRequest) finish();
        if (permissions.length > 1){
            requestMultiPermissionLauncher.launch(permissions);
        }
        else {
            requestPermissionLauncher.launch(permissions[0]);
        }
    }

    public static void runPermissionRequest(Context ctx,String [] permissions, int title){
        Intent i=new Intent(ctx, PermissionActivity.class);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT | Intent.FLAG_ACTIVITY_MULTIPLE_TASK|Intent.FLAG_ACTIVITY_NEW_TASK);
        Bundle extra=new Bundle();
        extra.putInt(Constants.EXTRA_TXT,title);
        extra.putStringArray(Constants.EXTRA_PERMISSIONS,permissions);
        i.putExtras(extra);
        ctx.startActivity(i);
    }
}

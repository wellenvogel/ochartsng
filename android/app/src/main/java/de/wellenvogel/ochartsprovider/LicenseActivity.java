package de.wellenvogel.ochartsprovider;

import android.os.Bundle;
import android.text.Editable;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.widget.Button;
import android.widget.TextView;

import org.xml.sax.XMLReader;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

import androidx.appcompat.app.AppCompatActivity;

public class LicenseActivity extends AppCompatActivity {


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_license);
        Button cancel=findViewById(R.id.btLCancel);
        cancel.setOnClickListener(view -> {
            setResult(0);
            finish();
        });
        Button accept=findViewById(R.id.btLOk);
        accept.setOnClickListener(view -> {
            setResult(1);
            finish();
        });
        try {
            String baseName="license.html";
            String page=baseName;
            if ("debug".equals(BuildConfig.BUILD_TYPE)){
                page="license-debug.html";
            }
            if ("beta".equals(BuildConfig.BUILD_TYPE)){
                page="license-beta.html";
            }
            InputStream is=null;
            try{
                is=getAssets().open(page);
            }catch (IOException nf){
                if (!baseName.equals(page)){
                    is=getAssets().open(baseName);
                }
            }
            StringBuilder sb=new StringBuilder();
            byte []buf=new byte[1000];
            int rd=0;
            while ((rd=is.read(buf))> 0){
                sb.append(new String(buf,0,rd, StandardCharsets.UTF_8));
            }
            TextView v=findViewById(R.id.ltext);
            v.setText(Html.fromHtml(sb.toString(), Html.FROM_HTML_MODE_LEGACY, null, new Html.TagHandler() {
                @Override
                public void handleTag(boolean opening, String s, Editable editable, XMLReader xmlReader) {
                    if (s.equals("license") && opening){
                        editable.append(Integer.toString(Constants.LICENSE_VERSION));
                    }
                }
            }));
            v.setMovementMethod(LinkMovementMethod.getInstance());
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

    }


}
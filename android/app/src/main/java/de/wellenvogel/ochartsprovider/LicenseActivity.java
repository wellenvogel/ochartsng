package de.wellenvogel.ochartsprovider;

import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.widget.Button;
import android.widget.TextView;

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
            InputStream is=getAssets().open("license.html");
            StringBuilder sb=new StringBuilder();
            byte []buf=new byte[1000];
            while (is.read(buf)> 0){
                sb.append(new String(buf, StandardCharsets.UTF_8));
            }
            TextView v=findViewById(R.id.ltext);
            v.setText(Html.fromHtml(sb.toString(),Html.FROM_HTML_MODE_LEGACY));
            v.setMovementMethod(LinkMovementMethod.getInstance());
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

    }


}
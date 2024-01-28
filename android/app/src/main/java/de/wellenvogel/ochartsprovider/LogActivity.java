/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Log activity
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

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.InputStreamReader;

public class LogActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_log);
        Button close=findViewById(R.id.btClose);
        close.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                LogActivity.this.finish();
            }
        });
        TextView logView=findViewById(R.id.logText);
        try {
            String logname=getIntent().getStringExtra(Constants.LOGFILE);
            File logBase=new File(getFilesDir(),Constants.LOGDIR);
            File logFile=new File(logBase,logname);
            BufferedReader rd=new BufferedReader(new FileReader(logFile));
            String line=null;
            logView.setText("");
            while ((line=rd.readLine()) != null){
                logView.append(line);
                logView.append("\n");
            }
        }catch (Exception e){
            logView.setText("unable to read log file ");
        }
    }
}
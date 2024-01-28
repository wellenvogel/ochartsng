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

import android.content.ContentProviderClient;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Log;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class SigningImporter {
    Context ctx;
    public SigningImporter(Context ctx){
        this.ctx=ctx;
    }
    //sha fingerprints of the packages we accept as id providers
    static final String[] CERTS_SHA_256=new String[]{
            "3A:58:C9:2C:DE:34:2A:25:67:3F:BD:E5:EB:75:E4:74:03:88:B3:D7:7B:06:E6:0D:85:50:C9:79:53:5E:6B:80"  //avnav
    };
    static final String CONTENT_PROVIDER=BuildConfig.IDPROVIDER_PKG+"."+BuildConfig.IDPROVIDER_NAME;
    private static String getSHA256Fingerprint(byte[] cert) {
        String fingerprintSHA = null;
        try {
            MessageDigest md = MessageDigest.getInstance("SHA256");
            if (md != null) {
                byte[] sha1digest = md.digest(cert);
                StringBuilder sb = new StringBuilder();
                for (byte b : sha1digest) {
                    sb.append((Integer
                            .toHexString((b & 0xFF) | 0x100))
                            .substring(1, 3));
                }
                fingerprintSHA = sb.toString().toUpperCase();
            }

        } catch (NoSuchAlgorithmException e1) {
            Log.e(Constants.PRFX,"unable to create sha for cert",e1);
        }
        return fingerprintSHA;
    }
    private boolean isSignedBy(String pkg, String cert) {
        Signature[] signatures;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                PackageInfo info = ctx.getPackageManager().getPackageInfo(pkg, PackageManager.GET_SIGNING_CERTIFICATES);
                if (info.signingInfo.hasMultipleSigners()) {
                    signatures = info.signingInfo.getApkContentsSigners();
                } else {
                    signatures = info.signingInfo.getSigningCertificateHistory();
                }
            } else {
                PackageInfo info = ctx.getPackageManager().getPackageInfo(pkg, PackageManager.GET_SIGNATURES);
                if (info == null) return false;
                signatures = info.signatures;
            }
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
        for (Signature sig : signatures) {
            String sha = getSHA256Fingerprint(sig.toByteArray());
            if (cert.equals(sha)) return true;
        }
        return false;
    }

    String retrieveIdentity(){
        ProviderInfo info=ctx.getPackageManager().resolveContentProvider(CONTENT_PROVIDER,0);
        if (info == null){
            Log.e(Constants.PRFX,"unable to retrieve provider "+CONTENT_PROVIDER);
            return null;
        }
        String pkg=info.packageName;
        boolean acceptedProvider=false;
        for (String cert:CERTS_SHA_256){
            if (isSignedBy(pkg,cert.replace(":",""))) {
                acceptedProvider=true;
                break;
            }
        }
        if (! acceptedProvider){
            Log.e(Constants.PRFX,"identity provider "+pkg+" has no known certificate");
            return null;
        }
        ContentProviderClient client=ctx.getContentResolver().acquireContentProviderClient(CONTENT_PROVIDER);
        if (client == null){
            return null;
        }
        try {
            Bundle res=client.call("identity",null,null);
            String rt=res.getString("identity");
            if (rt != null){
                Log.d(Constants.PRFX,"fetched identity "+rt);
            }
            else{
                Log.i(Constants.PRFX,"unable to fetch an identity from "+CONTENT_PROVIDER);
            }
            client.close();
            return rt;
        } catch (RemoteException e) {
            Log.e(Constants.PRFX,"unable to query "+CONTENT_PROVIDER,e);
            client.close();
        }
        return null;
    }
}

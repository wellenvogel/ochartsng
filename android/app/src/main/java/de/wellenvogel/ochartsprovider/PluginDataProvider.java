/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  plugin files provider
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

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class PluginDataProvider extends ContentProvider {
    private static final String AUTHORITY=BuildConfig.APPLICATION_ID+".pluginprovider";
    private static final UriMatcher uriMatcher=new UriMatcher(UriMatcher.NO_MATCH);
    private static final String ROOT="plugin";
    static{
        uriMatcher.addURI(AUTHORITY,ROOT+"/*",1);
    }
    @Override
    public boolean onCreate() {
        return false;
    }

    @Nullable
    @Override
    public Cursor query(@NonNull Uri uri, @Nullable String[] projection, @Nullable String selection, @Nullable String[] selectionArgs, @Nullable String sortOrder) {
        return null;
    }

    @Nullable
    @Override
    public String getType(@NonNull Uri uri) {
        return null;
    }

    @Nullable
    @Override
    public Uri insert(@NonNull Uri uri, @Nullable ContentValues values) {
        return null;
    }

    @Override
    public int delete(@NonNull Uri uri, @Nullable String selection, @Nullable String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(@NonNull Uri uri, @Nullable ContentValues values, @Nullable String selection, @Nullable String[] selectionArgs) {
        return 0;
    }

    @Nullable
    @Override
    public AssetFileDescriptor openAssetFile(@NonNull Uri uri, @NonNull String mode) throws FileNotFoundException {
        if (!"r".equals(mode)){
            throw new FileNotFoundException(("only read access allowed"));
        }
        int match=uriMatcher.match(uri);
        switch(match){
            case 1:
                try {
                    return getContext().getAssets().openFd(BuildConfig.ASSETS_PLUGIN+"/"+uri.getLastPathSegment());
                } catch (IOException e) {
                    throw new FileNotFoundException(e.getMessage());
                }
            default:
                throw new FileNotFoundException("invalid uri "+uri.toString());
        }
    }
    public static Uri getUri(String fileName){
        Uri.Builder builder=new Uri.Builder();
        builder.scheme("content").authority(AUTHORITY).path(ROOT);
        if (fileName != null) {
            builder.appendPath(fileName);
        }
        return builder.build();
    }

}

/******************************************************************************
 *
 * Project:  AvNav ocharts-provider
 * Purpose:  Log files probider
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

import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.DocumentsContract.Root;
import android.provider.DocumentsProvider;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.HashMap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class LogProvider extends DocumentsProvider {
    static final String PRFX="ocharts-doc";
    static final String LOG_ROOT=Constants.LOGDIR;
    static final String AUTHORITY=BuildConfig.APPLICATION_ID+".logprovider";
    File baseDir;
    public LogProvider(){
        super();
    }

    @Override
    public boolean onCreate() {
        Settings s=Settings.getSettings(getContext(),false);
        baseDir=new File(s.getWorkDir(),LOG_ROOT);
        return true;
    }

    /**
     * only files directly at root (no sub dirs!)
     * @param file
     * @param root
     * @return
     */
    public static String getDocIdForFile(File file,String root) {
        String path = file.getName();
        return root + ':' + path;
    }
    public static Uri getDocumentUri(String root, String f){
        if (LOG_ROOT.equals(root)) {
            Uri uri=DocumentsContract.buildDocumentUri(AUTHORITY,root+":"+f);
            return uri;
        }
        return null;
    }
    public static Uri getDocumentUri(String root, File f){
        return getDocumentUri(root,f.getName());
    }
    public static Uri getRootUri(String root){
        if (LOG_ROOT.equals(root) ) {
            Uri uri=DocumentsContract.buildDocumentUri(AUTHORITY,root+":");
            return uri;
        }
        return null;
    }

    @Override
    public Cursor queryRoots(String[] strings) throws FileNotFoundException {
        Log.d(PRFX,"query roots");
        final MatrixCursor result = new MatrixCursor(resolveRootProjection(strings));
        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Root.COLUMN_ROOT_ID,"log:");
        row.add(Root.COLUMN_DOCUMENT_ID,LOG_ROOT);
        row.add(Root.COLUMN_SUMMARY,"log files");
        row.add(Root.COLUMN_TITLE,"avnav ocharts");
        row.add(Root.COLUMN_MIME_TYPES,"text/plain");
        row.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
        row.add(Root.COLUMN_FLAGS,0);
        return result;
    }

    private File findFileForDocument(String document){
        if (! document.startsWith(LOG_ROOT+":")){
            return null;
        }
        if (! baseDir.isDirectory()){
            return null;
        }
        for (File f:baseDir.listFiles()){
            if (getDocIdForFile(f,LOG_ROOT).equals(document)){
                return f;
            }
        }
        return null;
    }

    private void setFile(MatrixCursor result, String document) {
        File f = findFileForDocument(document);
        if (f == null) {
            return;
        }
        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, document);
        row.add(Document.COLUMN_DISPLAY_NAME, f.getName());
        row.add(Document.COLUMN_SIZE, f.length());
        row.add(Document.COLUMN_MIME_TYPE, "text/plain");
        row.add(Document.COLUMN_LAST_MODIFIED, f.lastModified());
        row.add(Document.COLUMN_FLAGS, 0);
    }
    @Override
    public Cursor queryDocument(String document, String[] projection) throws FileNotFoundException {
        Log.d(PRFX, "query document " + document);
        final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
        if (!baseDir.isDirectory()) {
            return result;
        }
        final MatrixCursor.RowBuilder row = result.newRow();
        if (document.equals(LOG_ROOT) || document.equals(LOG_ROOT+":")) {
            row.add(Document.COLUMN_DOCUMENT_ID, LOG_ROOT+":");
            row.add(Document.COLUMN_DISPLAY_NAME, baseDir.getName());
            row.add(Document.COLUMN_SIZE, baseDir.length());
            row.add(Document.COLUMN_MIME_TYPE, Document.MIME_TYPE_DIR);
            row.add(Document.COLUMN_LAST_MODIFIED, baseDir.lastModified());
            row.add(Document.COLUMN_FLAGS, 0);
            return result;
        }
        else{
            setFile(result,document);
        }
        return result;
    }

    @Override
    public Cursor queryChildDocuments(String root, String[] projection, String s1) throws FileNotFoundException {
        if (!(LOG_ROOT+":").equals(root)) {
            return null;
        }
        final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
        if (! baseDir.isDirectory()){
            return result;
        }
        for (File f : baseDir.listFiles()){
            if (f.getName().startsWith("provider.log") && f.isFile()){
                final MatrixCursor.RowBuilder row = result.newRow();
                row.add(Document.COLUMN_DOCUMENT_ID,getDocIdForFile(f,LOG_ROOT));
                row.add(Document.COLUMN_DISPLAY_NAME,f.getName());
                row.add(Document.COLUMN_SIZE, f.length());
                row.add(Document.COLUMN_MIME_TYPE, "text/plain");
                row.add(Document.COLUMN_LAST_MODIFIED, f.lastModified());
            }
        }
        return result;
    }

    @Override
    public ParcelFileDescriptor openDocument(String s, String s1, @Nullable CancellationSignal cancellationSignal) throws FileNotFoundException {
        File f=findFileForDocument(s);
        if (f == null) {
            return null;
        }
        return ParcelFileDescriptor.open(f,ParcelFileDescriptor.MODE_READ_ONLY);
    }

    // Use these as the default columns to return information about a root if no specific
    // columns are requested in a query.
    private static final String[] DEFAULT_ROOT_PROJECTION = new String[]{
            Root.COLUMN_ROOT_ID,
            Root.COLUMN_MIME_TYPES,
            Root.COLUMN_FLAGS,
            Root.COLUMN_ICON,
            Root.COLUMN_TITLE,
            Root.COLUMN_SUMMARY,
            Root.COLUMN_DOCUMENT_ID
    };
    private static String[] resolveRootProjection(String[] projection) {
        return projection != null ? projection : DEFAULT_ROOT_PROJECTION;
    }

    // Use these as the default columns to return information about a document if no specific
    // columns are requested in a query.
    private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[]{
            Document.COLUMN_DOCUMENT_ID,
            Document.COLUMN_MIME_TYPE,
            Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_LAST_MODIFIED,
            Document.COLUMN_FLAGS,
            Document.COLUMN_SIZE
    };

    private static String[] resolveDocumentProjection(String[] projection) {
        return projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION;
    }
}

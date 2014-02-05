/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Distribution;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserContract.CommonColumns;
import org.mozilla.gecko.db.BrowserContract.FaviconColumns;
import org.mozilla.gecko.db.BrowserContract.Favicons;
import org.mozilla.gecko.db.BrowserContract.History;
import org.mozilla.gecko.db.BrowserContract.Schema;
import org.mozilla.gecko.db.BrowserContract.SyncColumns;
import org.mozilla.gecko.db.BrowserContract.Thumbnails;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.db.PerProfileDatabases.DatabaseHelperFactory;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.util.GeckoJarReader;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.app.SearchManager;
import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.OperationApplicationException;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.DatabaseUtils;
import android.database.MatrixCursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class BrowserProvider extends ContentProvider {
    private static final String LOGTAG = "GeckoBrowserProvider";
    private Context mContext;

    private PerProfileDatabases<BrowserDatabaseHelper> mDatabases;

    static final String DATABASE_NAME = "browser.db";

    static final int DATABASE_VERSION = 17;

    // Maximum age of deleted records to be cleaned up (20 days in ms)
    static final long MAX_AGE_OF_DELETED_RECORDS = 86400000 * 20;

    // Number of records marked as deleted to be removed
    static final long DELETED_RECORDS_PURGE_LIMIT = 5;

    // How many records to reposition in a single query.
    // This should be less than the SQLite maximum number of query variables
    // (currently 999) divided by the number of variables used per positioning
    // query (currently 3).
    static final int MAX_POSITION_UPDATES_PER_QUERY = 100;

    // Minimum number of records to keep when expiring history.
    static final int DEFAULT_EXPIRY_RETAIN_COUNT = 2000;
    static final int AGGRESSIVE_EXPIRY_RETAIN_COUNT = 500;

    // Minimum duration to keep when expiring.
    static final long DEFAULT_EXPIRY_PRESERVE_WINDOW = 1000L * 60L * 60L * 24L * 28L;     // Four weeks.
    // Minimum number of thumbnails to keep around.
    static final int DEFAULT_EXPIRY_THUMBNAIL_COUNT = 15;

    static final String TABLE_BOOKMARKS = "bookmarks";
    static final String TABLE_HISTORY = "history";
    static final String TABLE_FAVICONS = "favicons";
    static final String TABLE_THUMBNAILS = "thumbnails";

    static final String TABLE_BOOKMARKS_TMP = TABLE_BOOKMARKS + "_tmp";
    static final String TABLE_HISTORY_TMP = TABLE_HISTORY + "_tmp";
    static final String TABLE_IMAGES_TMP = Obsolete.TABLE_IMAGES + "_tmp";

    static final String VIEW_COMBINED = "combined";

    static final String VIEW_BOOKMARKS_WITH_FAVICONS = "bookmarks_with_favicons";
    static final String VIEW_HISTORY_WITH_FAVICONS = "history_with_favicons";
    static final String VIEW_COMBINED_WITH_FAVICONS = "combined_with_favicons";

    // Bookmark matches
    static final int BOOKMARKS = 100;
    static final int BOOKMARKS_ID = 101;
    static final int BOOKMARKS_FOLDER_ID = 102;
    static final int BOOKMARKS_PARENT = 103;
    static final int BOOKMARKS_POSITIONS = 104;

    // History matches
    static final int HISTORY = 200;
    static final int HISTORY_ID = 201;
    static final int HISTORY_OLD = 202;

    // Favicon matches
    static final int FAVICONS = 300;
    static final int FAVICON_ID = 301;

    // Schema matches
    static final int SCHEMA = 400;

    // Combined bookmarks and history matches
    static final int COMBINED = 500;

    // Control matches
    static final int CONTROL = 600;

    // Search Suggest matches
    static final int SEARCH_SUGGEST = 700;

    // Thumbnail matches
    static final int THUMBNAILS = 800;
    static final int THUMBNAIL_ID = 801;

    static final String DEFAULT_BOOKMARKS_SORT_ORDER = Bookmarks.TYPE
            + " ASC, " + Bookmarks.POSITION + " ASC, " + Bookmarks._ID
            + " ASC";

    static final String DEFAULT_HISTORY_SORT_ORDER = History.DATE_LAST_VISITED + " DESC";

    static final String TABLE_BOOKMARKS_JOIN_FAVICONS = TABLE_BOOKMARKS + " LEFT OUTER JOIN " +
            TABLE_FAVICONS + " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.FAVICON_ID) + " = " +
            qualifyColumn(TABLE_FAVICONS, Favicons._ID);

    static final String TABLE_HISTORY_JOIN_FAVICONS = TABLE_HISTORY + " LEFT OUTER JOIN " +
            TABLE_FAVICONS + " ON " + qualifyColumn(TABLE_HISTORY, History.FAVICON_ID) + " = " +
            qualifyColumn(TABLE_FAVICONS, Favicons._ID);

    static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);

    static final Map<String, String> BOOKMARKS_PROJECTION_MAP;
    static final Map<String, String> HISTORY_PROJECTION_MAP;
    static final Map<String, String> COMBINED_PROJECTION_MAP;
    static final Map<String, String> SCHEMA_PROJECTION_MAP;
    static final Map<String, String> SEARCH_SUGGEST_PROJECTION_MAP;
    static final Map<String, String> FAVICONS_PROJECTION_MAP;
    static final Map<String, String> THUMBNAILS_PROJECTION_MAP;

    static final class Obsolete {
        public static final String TABLE_IMAGES = "images";
        public static final String VIEW_BOOKMARKS_WITH_IMAGES = "bookmarks_with_images";
        public static final String VIEW_HISTORY_WITH_IMAGES = "history_with_images";
        public static final String VIEW_COMBINED_WITH_IMAGES = "combined_with_images";

        public static final class Images implements CommonColumns, SyncColumns {
            private Images() {}

            public static final String URL = "url_key";
            public static final String FAVICON_URL = "favicon_url";
            public static final String FAVICON = "favicon";
            public static final String THUMBNAIL = "thumbnail";
            public static final String _ID = "_id";
            public static final String GUID = "guid";
            public static final String DATE_CREATED = "created";
            public static final String DATE_MODIFIED = "modified";
            public static final String IS_DELETED = "deleted";
        }

        public static final class Combined {
            private Combined() {}

            public static final String THUMBNAIL = "thumbnail";
        }

        static final String TABLE_BOOKMARKS_JOIN_IMAGES = TABLE_BOOKMARKS + " LEFT OUTER JOIN " +
                Obsolete.TABLE_IMAGES + " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " +
                qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.URL);

        static final String TABLE_HISTORY_JOIN_IMAGES = TABLE_HISTORY + " LEFT OUTER JOIN " +
                Obsolete.TABLE_IMAGES + " ON " + qualifyColumn(TABLE_HISTORY, History.URL) + " = " +
                qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.URL);

        static final String FAVICON_DB = "favicon_urls.db";
    }

    static {
        // We will reuse this.
        HashMap<String, String> map;

        // Bookmarks
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks", BOOKMARKS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/#", BOOKMARKS_ID);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/parents", BOOKMARKS_PARENT);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/positions", BOOKMARKS_POSITIONS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "bookmarks/folder/#", BOOKMARKS_FOLDER_ID);

        map = new HashMap<String, String>();
        map.put(Bookmarks._ID, Bookmarks._ID);
        map.put(Bookmarks.TITLE, Bookmarks.TITLE);
        map.put(Bookmarks.URL, Bookmarks.URL);
        map.put(Bookmarks.FAVICON, Bookmarks.FAVICON);
        map.put(Bookmarks.FAVICON_ID, Bookmarks.FAVICON_ID);
        map.put(Bookmarks.FAVICON_URL, Bookmarks.FAVICON_URL);
        map.put(Bookmarks.TYPE, Bookmarks.TYPE);
        map.put(Bookmarks.PARENT, Bookmarks.PARENT);
        map.put(Bookmarks.POSITION, Bookmarks.POSITION);
        map.put(Bookmarks.TAGS, Bookmarks.TAGS);
        map.put(Bookmarks.DESCRIPTION, Bookmarks.DESCRIPTION);
        map.put(Bookmarks.KEYWORD, Bookmarks.KEYWORD);
        map.put(Bookmarks.DATE_CREATED, Bookmarks.DATE_CREATED);
        map.put(Bookmarks.DATE_MODIFIED, Bookmarks.DATE_MODIFIED);
        map.put(Bookmarks.GUID, Bookmarks.GUID);
        map.put(Bookmarks.IS_DELETED, Bookmarks.IS_DELETED);
        BOOKMARKS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // History
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history", HISTORY);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history/#", HISTORY_ID);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "history/old", HISTORY_OLD);

        map = new HashMap<String, String>();
        map.put(History._ID, History._ID);
        map.put(History.TITLE, History.TITLE);
        map.put(History.URL, History.URL);
        map.put(History.FAVICON, History.FAVICON);
        map.put(History.FAVICON_ID, History.FAVICON_ID);
        map.put(History.FAVICON_URL, History.FAVICON_URL);
        map.put(History.VISITS, History.VISITS);
        map.put(History.DATE_LAST_VISITED, History.DATE_LAST_VISITED);
        map.put(History.DATE_CREATED, History.DATE_CREATED);
        map.put(History.DATE_MODIFIED, History.DATE_MODIFIED);
        map.put(History.GUID, History.GUID);
        map.put(History.IS_DELETED, History.IS_DELETED);
        HISTORY_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Favicons
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "favicons", FAVICONS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "favicons/#", FAVICON_ID);

        map = new HashMap<String, String>();
        map.put(Favicons._ID, Favicons._ID);
        map.put(Favicons.URL, Favicons.URL);
        map.put(Favicons.DATA, Favicons.DATA);
        map.put(Favicons.DATE_CREATED, Favicons.DATE_CREATED);
        map.put(Favicons.DATE_MODIFIED, Favicons.DATE_MODIFIED);
        FAVICONS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Thumbnails
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "thumbnails", THUMBNAILS);
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "thumbnails/#", THUMBNAIL_ID);

        map = new HashMap<String, String>();
        map.put(Thumbnails._ID, Thumbnails._ID);
        map.put(Thumbnails.URL, Thumbnails.URL);
        map.put(Thumbnails.DATA, Thumbnails.DATA);
        THUMBNAILS_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Combined bookmarks and history
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "combined", COMBINED);

        map = new HashMap<String, String>();
        map.put(Combined._ID, Combined._ID);
        map.put(Combined.BOOKMARK_ID, Combined.BOOKMARK_ID);
        map.put(Combined.HISTORY_ID, Combined.HISTORY_ID);
        map.put(Combined.DISPLAY, "MAX(" + Combined.DISPLAY + ") AS " + Combined.DISPLAY);
        map.put(Combined.URL, Combined.URL);
        map.put(Combined.TITLE, Combined.TITLE);
        map.put(Combined.VISITS, Combined.VISITS);
        map.put(Combined.DATE_LAST_VISITED, Combined.DATE_LAST_VISITED);
        map.put(Combined.FAVICON, Combined.FAVICON);
        map.put(Combined.FAVICON_ID, Combined.FAVICON_ID);
        map.put(Combined.FAVICON_URL, Combined.FAVICON_URL);
        COMBINED_PROJECTION_MAP = Collections.unmodifiableMap(map);

        // Schema
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "schema", SCHEMA);

        map = new HashMap<String, String>();
        map.put(Schema.VERSION, Schema.VERSION);
        SCHEMA_PROJECTION_MAP = Collections.unmodifiableMap(map);


        // Control
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, "control", CONTROL);

        // Search Suggest
        URI_MATCHER.addURI(BrowserContract.AUTHORITY, SearchManager.SUGGEST_URI_PATH_QUERY + "/*", SEARCH_SUGGEST);

        map = new HashMap<String, String>();
        map.put(SearchManager.SUGGEST_COLUMN_TEXT_1,
                Combined.TITLE + " AS " + SearchManager.SUGGEST_COLUMN_TEXT_1);
        map.put(SearchManager.SUGGEST_COLUMN_TEXT_2_URL,
                Combined.URL + " AS " + SearchManager.SUGGEST_COLUMN_TEXT_2_URL);
        map.put(SearchManager.SUGGEST_COLUMN_INTENT_DATA,
                Combined.URL + " AS " + SearchManager.SUGGEST_COLUMN_INTENT_DATA);
        SEARCH_SUGGEST_PROJECTION_MAP = Collections.unmodifiableMap(map);
    }

    private interface BookmarkMigrator {
        public void updateForNewTable(ContentValues bookmark);
    }

    private class BookmarkMigrator3to4 implements BookmarkMigrator {
        @Override
        public void updateForNewTable(ContentValues bookmark) {
            Integer isFolder = bookmark.getAsInteger("folder");
            if (isFolder == null || isFolder != 1) {
                bookmark.put(Bookmarks.TYPE, Bookmarks.TYPE_BOOKMARK);
            } else {
                bookmark.put(Bookmarks.TYPE, Bookmarks.TYPE_FOLDER);
            }

            bookmark.remove("folder");
        }
    }

    static final String qualifyColumn(String table, String column) {
        return table + "." + column;
    }

    private static boolean hasFaviconsInProjection(String[] projection) {
        if (projection == null) return true;
        for (int i = 0; i < projection.length; ++i) {
            if (projection[i].equals(FaviconColumns.FAVICON) ||
                projection[i].equals(FaviconColumns.FAVICON_URL))
                return true;
        }

        return false;
    }

    // Calculate these once, at initialization. isLoggable is too expensive to
    // have in-line in each log call.
    private static boolean logDebug   = Log.isLoggable(LOGTAG, Log.DEBUG);
    private static boolean logVerbose = Log.isLoggable(LOGTAG, Log.VERBOSE);
    protected static void trace(String message) {
        if (logVerbose) {
            Log.v(LOGTAG, message);
        }
    }

    protected static void debug(String message) {
        if (logDebug) {
            Log.d(LOGTAG, message);
        }
    }

    final class BrowserDatabaseHelper extends SQLiteOpenHelper {
        public BrowserDatabaseHelper(Context context, String databasePath) {
            super(context, databasePath, null, DATABASE_VERSION);
        }

        private void createBookmarksTable(SQLiteDatabase db) {
            debug("Creating " + TABLE_BOOKMARKS + " table");

            // Android versions older than Froyo ship with an sqlite
            // that doesn't support foreign keys.
            String foreignKeyOnParent = null;
            if (Build.VERSION.SDK_INT >= 8) {
                foreignKeyOnParent = ", FOREIGN KEY (" + Bookmarks.PARENT +
                    ") REFERENCES " + TABLE_BOOKMARKS + "(" + Bookmarks._ID + ")";
            }

            db.execSQL("CREATE TABLE " + TABLE_BOOKMARKS + "(" +
                    Bookmarks._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    Bookmarks.TITLE + " TEXT," +
                    Bookmarks.URL + " TEXT," +
                    Bookmarks.TYPE + " INTEGER NOT NULL DEFAULT " + Bookmarks.TYPE_BOOKMARK + "," +
                    Bookmarks.PARENT + " INTEGER," +
                    Bookmarks.POSITION + " INTEGER NOT NULL," +
                    Bookmarks.KEYWORD + " TEXT," +
                    Bookmarks.DESCRIPTION + " TEXT," +
                    Bookmarks.TAGS + " TEXT," +
                    Bookmarks.DATE_CREATED + " INTEGER," +
                    Bookmarks.DATE_MODIFIED + " INTEGER," +
                    Bookmarks.GUID + " TEXT NOT NULL," +
                    Bookmarks.IS_DELETED + " INTEGER NOT NULL DEFAULT 0" +
                    (foreignKeyOnParent != null ? foreignKeyOnParent : "") +
                    ");");

            db.execSQL("CREATE INDEX bookmarks_url_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.URL + ")");
            db.execSQL("CREATE INDEX bookmarks_type_deleted_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.TYPE + ", " + Bookmarks.IS_DELETED + ")");
            db.execSQL("CREATE UNIQUE INDEX bookmarks_guid_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.GUID + ")");
            db.execSQL("CREATE INDEX bookmarks_modified_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.DATE_MODIFIED + ")");
        }

        private void createBookmarksTableOn13(SQLiteDatabase db) {
            debug("Creating " + TABLE_BOOKMARKS + " table");

            // Android versions older than Froyo ship with an sqlite
            // that doesn't support foreign keys.
            String foreignKeyOnParent = null;
            if (Build.VERSION.SDK_INT >= 8) {
                foreignKeyOnParent = ", FOREIGN KEY (" + Bookmarks.PARENT +
                    ") REFERENCES " + TABLE_BOOKMARKS + "(" + Bookmarks._ID + ")";
            }

            db.execSQL("CREATE TABLE " + TABLE_BOOKMARKS + "(" +
                    Bookmarks._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    Bookmarks.TITLE + " TEXT," +
                    Bookmarks.URL + " TEXT," +
                    Bookmarks.TYPE + " INTEGER NOT NULL DEFAULT " + Bookmarks.TYPE_BOOKMARK + "," +
                    Bookmarks.PARENT + " INTEGER," +
                    Bookmarks.POSITION + " INTEGER NOT NULL," +
                    Bookmarks.KEYWORD + " TEXT," +
                    Bookmarks.DESCRIPTION + " TEXT," +
                    Bookmarks.TAGS + " TEXT," +
                    Bookmarks.FAVICON_ID + " INTEGER," +
                    Bookmarks.DATE_CREATED + " INTEGER," +
                    Bookmarks.DATE_MODIFIED + " INTEGER," +
                    Bookmarks.GUID + " TEXT NOT NULL," +
                    Bookmarks.IS_DELETED + " INTEGER NOT NULL DEFAULT 0" +
                    (foreignKeyOnParent != null ? foreignKeyOnParent : "") +
                    ");");

            db.execSQL("CREATE INDEX bookmarks_url_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.URL + ")");
            db.execSQL("CREATE INDEX bookmarks_type_deleted_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.TYPE + ", " + Bookmarks.IS_DELETED + ")");
            db.execSQL("CREATE UNIQUE INDEX bookmarks_guid_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.GUID + ")");
            db.execSQL("CREATE INDEX bookmarks_modified_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.DATE_MODIFIED + ")");
        }

        private void createHistoryTable(SQLiteDatabase db) {
            debug("Creating " + TABLE_HISTORY + " table");
            db.execSQL("CREATE TABLE " + TABLE_HISTORY + "(" +
                    History._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    History.TITLE + " TEXT," +
                    History.URL + " TEXT NOT NULL," +
                    History.VISITS + " INTEGER NOT NULL DEFAULT 0," +
                    History.DATE_LAST_VISITED + " INTEGER," +
                    History.DATE_CREATED + " INTEGER," +
                    History.DATE_MODIFIED + " INTEGER," +
                    History.GUID + " TEXT NOT NULL," +
                    History.IS_DELETED + " INTEGER NOT NULL DEFAULT 0" +
                    ");");

            db.execSQL("CREATE INDEX history_url_index ON " + TABLE_HISTORY + "("
                    + History.URL + ")");
            db.execSQL("CREATE UNIQUE INDEX history_guid_index ON " + TABLE_HISTORY + "("
                    + History.GUID + ")");
            db.execSQL("CREATE INDEX history_modified_index ON " + TABLE_HISTORY + "("
                    + History.DATE_MODIFIED + ")");
            db.execSQL("CREATE INDEX history_visited_index ON " + TABLE_HISTORY + "("
                    + History.DATE_LAST_VISITED + ")");
        }

        private void createHistoryTableOn13(SQLiteDatabase db) {
            debug("Creating " + TABLE_HISTORY + " table");
            db.execSQL("CREATE TABLE " + TABLE_HISTORY + "(" +
                    History._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    History.TITLE + " TEXT," +
                    History.URL + " TEXT NOT NULL," +
                    History.VISITS + " INTEGER NOT NULL DEFAULT 0," +
                    History.FAVICON_ID + " INTEGER," +
                    History.DATE_LAST_VISITED + " INTEGER," +
                    History.DATE_CREATED + " INTEGER," +
                    History.DATE_MODIFIED + " INTEGER," +
                    History.GUID + " TEXT NOT NULL," +
                    History.IS_DELETED + " INTEGER NOT NULL DEFAULT 0" +
                    ");");

            db.execSQL("CREATE INDEX history_url_index ON " + TABLE_HISTORY + "("
                    + History.URL + ")");
            db.execSQL("CREATE UNIQUE INDEX history_guid_index ON " + TABLE_HISTORY + "("
                    + History.GUID + ")");
            db.execSQL("CREATE INDEX history_modified_index ON " + TABLE_HISTORY + "("
                    + History.DATE_MODIFIED + ")");
            db.execSQL("CREATE INDEX history_visited_index ON " + TABLE_HISTORY + "("
                    + History.DATE_LAST_VISITED + ")");
        }

        private void createImagesTable(SQLiteDatabase db) {
            debug("Creating " + Obsolete.TABLE_IMAGES + " table");
            db.execSQL("CREATE TABLE " + Obsolete.TABLE_IMAGES + " (" +
                    Obsolete.Images._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    Obsolete.Images.URL + " TEXT UNIQUE NOT NULL," +
                    Obsolete.Images.FAVICON + " BLOB," +
                    Obsolete.Images.FAVICON_URL + " TEXT," +
                    Obsolete.Images.THUMBNAIL + " BLOB," +
                    Obsolete.Images.DATE_CREATED + " INTEGER," +
                    Obsolete.Images.DATE_MODIFIED + " INTEGER," +
                    Obsolete.Images.GUID + " TEXT NOT NULL," +
                    Obsolete.Images.IS_DELETED + " INTEGER NOT NULL DEFAULT 0" +
                    ");");

            db.execSQL("CREATE INDEX images_url_index ON " + Obsolete.TABLE_IMAGES + "("
                    + Obsolete.Images.URL + ")");
            db.execSQL("CREATE UNIQUE INDEX images_guid_index ON " + Obsolete.TABLE_IMAGES + "("
                    + Obsolete.Images.GUID + ")");
            db.execSQL("CREATE INDEX images_modified_index ON " + Obsolete.TABLE_IMAGES + "("
                    + Obsolete.Images.DATE_MODIFIED + ")");
        }

        private void createFaviconsTable(SQLiteDatabase db) {
            debug("Creating " + TABLE_FAVICONS + " table");
            db.execSQL("CREATE TABLE " + TABLE_FAVICONS + " (" +
                    Favicons._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    Favicons.URL + " TEXT UNIQUE," +
                    Favicons.DATA + " BLOB," +
                    Favicons.DATE_CREATED + " INTEGER," +
                    Favicons.DATE_MODIFIED + " INTEGER" +
                    ");");

            db.execSQL("CREATE INDEX favicons_url_index ON " + TABLE_FAVICONS + "("
                    + Favicons.URL + ")");
            db.execSQL("CREATE INDEX favicons_modified_index ON " + TABLE_FAVICONS + "("
                    + Favicons.DATE_MODIFIED + ")");
        }

        private void createThumbnailsTable(SQLiteDatabase db) {
            debug("Creating " + TABLE_THUMBNAILS + " table");
            db.execSQL("CREATE TABLE " + TABLE_THUMBNAILS + " (" +
                    Thumbnails._ID + " INTEGER PRIMARY KEY AUTOINCREMENT," +
                    Thumbnails.URL + " TEXT UNIQUE," +
                    Thumbnails.DATA + " BLOB" +
                    ");");

            db.execSQL("CREATE INDEX thumbnails_url_index ON " + TABLE_THUMBNAILS + "("
                    + Thumbnails.URL + ")");
        }

        private void createBookmarksWithImagesView(SQLiteDatabase db) {
            debug("Creating " + Obsolete.VIEW_BOOKMARKS_WITH_IMAGES + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + Obsolete.VIEW_BOOKMARKS_WITH_IMAGES + " AS " +
                    "SELECT " + qualifyColumn(TABLE_BOOKMARKS, "*") +
                    ", " + Obsolete.Images.FAVICON + ", " + Obsolete.Images.THUMBNAIL + " FROM " +
                    Obsolete.TABLE_BOOKMARKS_JOIN_IMAGES);
        }

        private void createBookmarksWithFaviconsView(SQLiteDatabase db) {
            debug("Creating " + VIEW_BOOKMARKS_WITH_FAVICONS + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_BOOKMARKS_WITH_FAVICONS + " AS " +
                    "SELECT " + qualifyColumn(TABLE_BOOKMARKS, "*") +
                    ", " + qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + Bookmarks.FAVICON +
                    ", " + qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + Bookmarks.FAVICON_URL +
                    " FROM " + TABLE_BOOKMARKS_JOIN_FAVICONS);
        }

        private void createHistoryWithImagesView(SQLiteDatabase db) {
            debug("Creating " + Obsolete.VIEW_HISTORY_WITH_IMAGES + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + Obsolete.VIEW_HISTORY_WITH_IMAGES + " AS " +
                    "SELECT " + qualifyColumn(TABLE_HISTORY, "*") +
                    ", " + Obsolete.Images.FAVICON + ", " + Obsolete.Images.THUMBNAIL + " FROM " +
                    Obsolete.TABLE_HISTORY_JOIN_IMAGES);
        }

        private void createHistoryWithFaviconsView(SQLiteDatabase db) {
            debug("Creating " + VIEW_HISTORY_WITH_FAVICONS + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_HISTORY_WITH_FAVICONS + " AS " +
                    "SELECT " + qualifyColumn(TABLE_HISTORY, "*") +
                    ", " + qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + History.FAVICON +
                    ", " + qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + History.FAVICON_URL +
                    " FROM " + TABLE_HISTORY_JOIN_FAVICONS);
        }

        private void createCombinedWithImagesView(SQLiteDatabase db) {
            debug("Creating " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " AS" +
                    " SELECT " + Combined.BOOKMARK_ID + ", " +
                                 Combined.HISTORY_ID + ", " +
                                 // We need to return an _id column because CursorAdapter requires it for its
                                 // default implementation for the getItemId() method. However, since
                                 // we're not using this feature in the parts of the UI using this view,
                                 // we can just use 0 for all rows.
                                 "0 AS " + Combined._ID + ", " +
                                 Combined.URL + ", " +
                                 Combined.TITLE + ", " +
                                 Combined.VISITS + ", " +
                                 Combined.DATE_LAST_VISITED + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.FAVICON) + " AS " + Combined.FAVICON + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.THUMBNAIL) + " AS " + Obsolete.Combined.THUMBNAIL +
                    " FROM (" +
                        // Bookmarks without history.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                                     "-1 AS " + Combined.HISTORY_ID + ", " +
                                     "-1 AS " + Combined.VISITS + ", " +
                                     "-1 AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_BOOKMARKS +
                        " WHERE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                        " UNION ALL" +
                        // History with and without bookmark.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + ", " +
                                     // Prioritze bookmark titles over history titles, since the user may have
                                     // customized the title for a bookmark.
                                     "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                                   qualifyColumn(TABLE_HISTORY, History.TITLE) +")" + " AS " + Combined.TITLE + ", " +
                                     qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                            " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                        " WHERE " + qualifyColumn(TABLE_HISTORY, History.URL) + " IS NOT NULL AND " +
                                    qualifyColumn(TABLE_HISTORY, History.IS_DELETED)  + " = 0 AND (" +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + ")" +
                    ") LEFT OUTER JOIN " + Obsolete.TABLE_IMAGES +
                        " ON " + Combined.URL + " = " + qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.URL));
        }

        private void createCombinedWithImagesViewOn9(SQLiteDatabase db) {
            debug("Creating " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " AS" +
                    " SELECT " + Combined.BOOKMARK_ID + ", " +
                                 Combined.HISTORY_ID + ", " +
                                 // We need to return an _id column because CursorAdapter requires it for its
                                 // default implementation for the getItemId() method. However, since
                                 // we're not using this feature in the parts of the UI using this view,
                                 // we can just use 0 for all rows.
                                 "0 AS " + Combined._ID + ", " +
                                 Combined.URL + ", " +
                                 Combined.TITLE + ", " +
                                 Combined.VISITS + ", " +
                                 Combined.DISPLAY + ", " +
                                 Combined.DATE_LAST_VISITED + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.FAVICON) + " AS " + Combined.FAVICON + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.THUMBNAIL) + " AS " + Obsolete.Combined.THUMBNAIL +
                    " FROM (" +
                        // Bookmarks without history.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     "-1 AS " + Combined.HISTORY_ID + ", " +
                                     "-1 AS " + Combined.VISITS + ", " +
                                     "-1 AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_BOOKMARKS +
                        " WHERE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                        " UNION ALL" +
                        // History with and without bookmark.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + ", " +
                                     // Prioritze bookmark titles over history titles, since the user may have
                                     // customized the title for a bookmark.
                                     "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                                   qualifyColumn(TABLE_HISTORY, History.TITLE) +")" + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                            " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                        " WHERE " + qualifyColumn(TABLE_HISTORY, History.URL) + " IS NOT NULL AND " +
                                    qualifyColumn(TABLE_HISTORY, History.IS_DELETED)  + " = 0 AND (" +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + ")" +
                    ") LEFT OUTER JOIN " + Obsolete.TABLE_IMAGES +
                        " ON " + Combined.URL + " = " + qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.URL));
        }

        private void createCombinedWithImagesViewOn10(SQLiteDatabase db) {
            debug("Creating " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " AS" +
                    " SELECT " + Combined.BOOKMARK_ID + ", " +
                                 Combined.HISTORY_ID + ", " +
                                 // We need to return an _id column because CursorAdapter requires it for its
                                 // default implementation for the getItemId() method. However, since
                                 // we're not using this feature in the parts of the UI using this view,
                                 // we can just use 0 for all rows.
                                 "0 AS " + Combined._ID + ", " +
                                 Combined.URL + ", " +
                                 Combined.TITLE + ", " +
                                 Combined.VISITS + ", " +
                                 Combined.DISPLAY + ", " +
                                 Combined.DATE_LAST_VISITED + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.FAVICON) + " AS " + Combined.FAVICON + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.THUMBNAIL) + " AS " + Obsolete.Combined.THUMBNAIL +
                    " FROM (" +
                        // Bookmarks without history.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     "-1 AS " + Combined.HISTORY_ID + ", " +
                                     "-1 AS " + Combined.VISITS + ", " +
                                     "-1 AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_BOOKMARKS +
                        " WHERE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                        " UNION ALL" +
                        // History with and without bookmark.
                        " SELECT " + "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) +  " ELSE NULL END AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + ", " +
                                     // Prioritze bookmark titles over history titles, since the user may have
                                     // customized the title for a bookmark.
                                     "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                                   qualifyColumn(TABLE_HISTORY, History.TITLE) +")" + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                            " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                        " WHERE " + qualifyColumn(TABLE_HISTORY, History.URL) + " IS NOT NULL AND " +
                                    qualifyColumn(TABLE_HISTORY, History.IS_DELETED)  + " = 0 AND (" +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + ")" +
                    ") LEFT OUTER JOIN " + Obsolete.TABLE_IMAGES +
                        " ON " + Combined.URL + " = " + qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.URL));
        }

        private void createCombinedWithImagesViewOn11(SQLiteDatabase db) {
            debug("Creating " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " AS" +
                    " SELECT " + Combined.BOOKMARK_ID + ", " +
                                 Combined.HISTORY_ID + ", " +
                                 // We need to return an _id column because CursorAdapter requires it for its
                                 // default implementation for the getItemId() method. However, since
                                 // we're not using this feature in the parts of the UI using this view,
                                 // we can just use 0 for all rows.
                                 "0 AS " + Combined._ID + ", " +
                                 Combined.URL + ", " +
                                 Combined.TITLE + ", " +
                                 Combined.VISITS + ", " +
                                 Combined.DISPLAY + ", " +
                                 Combined.DATE_LAST_VISITED + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.FAVICON) + " AS " + Combined.FAVICON + ", " +
                                 qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.THUMBNAIL) + " AS " + Obsolete.Combined.THUMBNAIL +
                    " FROM (" +
                        // Bookmarks without history.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     "-1 AS " + Combined.HISTORY_ID + ", " +
                                     "-1 AS " + Combined.VISITS + ", " +
                                     "-1 AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_BOOKMARKS +
                        " WHERE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                        " UNION ALL" +
                        // History with and without bookmark.
                        " SELECT " + "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) +  " ELSE NULL END AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + ", " +
                                     // Prioritze bookmark titles over history titles, since the user may have
                                     // customized the title for a bookmark.
                                     "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                                   qualifyColumn(TABLE_HISTORY, History.TITLE) +")" + " AS " + Combined.TITLE + ", " +
                                     // Only use DISPLAY_READER if the matching bookmark entry inside reading
                                     // list folder is not marked as deleted.
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN CASE " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " + Bookmarks.FIXED_READING_LIST_ID +
                                        " THEN " + Combined.DISPLAY_READER + " ELSE " + Combined.DISPLAY_NORMAL + " END ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                            " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                        " WHERE " + qualifyColumn(TABLE_HISTORY, History.URL) + " IS NOT NULL AND " +
                                    qualifyColumn(TABLE_HISTORY, History.IS_DELETED)  + " = 0 AND (" +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + ") " +
                    ") LEFT OUTER JOIN " + Obsolete.TABLE_IMAGES +
                        " ON " + Combined.URL + " = " + qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.URL));
        }

        private void createCombinedViewOn12(SQLiteDatabase db) {
            debug("Creating " + VIEW_COMBINED + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED + " AS" +
                    " SELECT " + Combined.BOOKMARK_ID + ", " +
                                 Combined.HISTORY_ID + ", " +
                                 // We need to return an _id column because CursorAdapter requires it for its
                                 // default implementation for the getItemId() method. However, since
                                 // we're not using this feature in the parts of the UI using this view,
                                 // we can just use 0 for all rows.
                                 "0 AS " + Combined._ID + ", " +
                                 Combined.URL + ", " +
                                 Combined.TITLE + ", " +
                                 Combined.VISITS + ", " +
                                 Combined.DISPLAY + ", " +
                                 Combined.DATE_LAST_VISITED +
                    " FROM (" +
                        // Bookmarks without history.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     "-1 AS " + Combined.HISTORY_ID + ", " +
                                     "-1 AS " + Combined.VISITS + ", " +
                                     "-1 AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_BOOKMARKS +
                        " WHERE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                        " UNION ALL" +
                        // History with and without bookmark.
                        " SELECT " + "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) +  " ELSE NULL END AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + ", " +
                                     // Prioritze bookmark titles over history titles, since the user may have
                                     // customized the title for a bookmark.
                                     "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                                   qualifyColumn(TABLE_HISTORY, History.TITLE) +")" + " AS " + Combined.TITLE + ", " +
                                     // Only use DISPLAY_READER if the matching bookmark entry inside reading
                                     // list folder is not marked as deleted.
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN CASE " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " + Bookmarks.FIXED_READING_LIST_ID +
                                        " THEN " + Combined.DISPLAY_READER + " ELSE " + Combined.DISPLAY_NORMAL + " END ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED +
                        " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                            " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                        " WHERE " + qualifyColumn(TABLE_HISTORY, History.URL) + " IS NOT NULL AND " +
                                    qualifyColumn(TABLE_HISTORY, History.IS_DELETED)  + " = 0 AND (" +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + ") " +
                    ")");

            debug("Creating " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES + " AS" +
                    " SELECT *, " +
                        qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.FAVICON) + " AS " + Combined.FAVICON + ", " +
                        qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.THUMBNAIL) + " AS " + Obsolete.Combined.THUMBNAIL +
                    " FROM " + VIEW_COMBINED + " LEFT OUTER JOIN " + Obsolete.TABLE_IMAGES +
                        " ON " + Combined.URL + " = " + qualifyColumn(Obsolete.TABLE_IMAGES, Obsolete.Images.URL));
        }

        private void createCombinedViewOn13(SQLiteDatabase db) {
            debug("Creating " + VIEW_COMBINED + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED + " AS" +
                    " SELECT " + Combined.BOOKMARK_ID + ", " +
                                 Combined.HISTORY_ID + ", " +
                                 // We need to return an _id column because CursorAdapter requires it for its
                                 // default implementation for the getItemId() method. However, since
                                 // we're not using this feature in the parts of the UI using this view,
                                 // we can just use 0 for all rows.
                                 "0 AS " + Combined._ID + ", " +
                                 Combined.URL + ", " +
                                 Combined.TITLE + ", " +
                                 Combined.VISITS + ", " +
                                 Combined.DISPLAY + ", " +
                                 Combined.DATE_LAST_VISITED + ", " +
                                 Combined.FAVICON_ID +
                    " FROM (" +
                        // Bookmarks without history.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     "-1 AS " + Combined.HISTORY_ID + ", " +
                                     "-1 AS " + Combined.VISITS + ", " +
                                     "-1 AS " + Combined.DATE_LAST_VISITED + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.FAVICON_ID) + " AS " + Combined.FAVICON_ID +
                        " FROM " + TABLE_BOOKMARKS +
                        " WHERE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                        " UNION ALL" +
                        // History with and without bookmark.
                        " SELECT " + "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) +  " ELSE NULL END AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + ", " +
                                     // Prioritize bookmark titles over history titles, since the user may have
                                     // customized the title for a bookmark.
                                     "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                                   qualifyColumn(TABLE_HISTORY, History.TITLE) +")" + " AS " + Combined.TITLE + ", " +
                                     // Only use DISPLAY_READER if the matching bookmark entry inside reading
                                     // list folder is not marked as deleted.
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN CASE " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " + Bookmarks.FIXED_READING_LIST_ID +
                                        " THEN " + Combined.DISPLAY_READER + " ELSE " + Combined.DISPLAY_NORMAL + " END ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.FAVICON_ID) + " AS " + Combined.FAVICON_ID +
                        " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                            " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                        " WHERE " + qualifyColumn(TABLE_HISTORY, History.URL) + " IS NOT NULL AND " +
                                    qualifyColumn(TABLE_HISTORY, History.IS_DELETED)  + " = 0 AND (" +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + ") " +
                    ")");

            debug("Creating " + VIEW_COMBINED_WITH_FAVICONS + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED_WITH_FAVICONS + " AS" +
                    " SELECT " + qualifyColumn(VIEW_COMBINED, "*") + ", " +
                        qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + Combined.FAVICON_URL + ", " +
                        qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + Combined.FAVICON +
                    " FROM " + VIEW_COMBINED + " LEFT OUTER JOIN " + TABLE_FAVICONS +
                        " ON " + Combined.FAVICON_ID + " = " + qualifyColumn(TABLE_FAVICONS, Favicons._ID));
        }

        private void createCombinedViewOn16(SQLiteDatabase db) {
            debug("Creating " + VIEW_COMBINED + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED + " AS" +
                    " SELECT " + Combined.BOOKMARK_ID + ", " +
                                 Combined.HISTORY_ID + ", " +
                                 // We need to return an _id column because CursorAdapter requires it for its
                                 // default implementation for the getItemId() method. However, since
                                 // we're not using this feature in the parts of the UI using this view,
                                 // we can just use 0 for all rows.
                                 "0 AS " + Combined._ID + ", " +
                                 Combined.URL + ", " +
                                 Combined.TITLE + ", " +
                                 Combined.VISITS + ", " +
                                 Combined.DISPLAY + ", " +
                                 Combined.DATE_LAST_VISITED + ", " +
                                 Combined.FAVICON_ID +
                    " FROM (" +
                        // Bookmarks without history.
                        " SELECT " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " AS " + Combined.URL + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + " AS " + Combined.TITLE + ", " +
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_READING_LIST_ID + " THEN " + Combined.DISPLAY_READER + " ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     "-1 AS " + Combined.HISTORY_ID + ", " +
                                     "-1 AS " + Combined.VISITS + ", " +
                                     "-1 AS " + Combined.DATE_LAST_VISITED + ", " +
                                     qualifyColumn(TABLE_BOOKMARKS, Bookmarks.FAVICON_ID) + " AS " + Combined.FAVICON_ID +
                        " FROM " + TABLE_BOOKMARKS +
                        " WHERE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + " AND " +
                                    // Ignore pinned bookmarks.
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT)  + " <> " + Bookmarks.FIXED_PINNED_LIST_ID + " AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED)  + " = 0 AND " +
                                    qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) +
                                        " NOT IN (SELECT " + History.URL + " FROM " + TABLE_HISTORY + ")" +
                        " UNION ALL" +
                        // History with and without bookmark.
                        " SELECT " + "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN " +
                                        // Give pinned bookmarks a NULL ID so that they're not treated as bookmarks. We can't
                                        // completely ignore them here because they're joined with history entries we care about.
                                        "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " +
                                        Bookmarks.FIXED_PINNED_LIST_ID + " THEN NULL ELSE " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks._ID) + " END " +
                                     "ELSE NULL END AS " + Combined.BOOKMARK_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.URL) + " AS " + Combined.URL + ", " +
                                     // Prioritize bookmark titles over history titles, since the user may have
                                     // customized the title for a bookmark.
                                     "COALESCE(" + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TITLE) + ", " +
                                                   qualifyColumn(TABLE_HISTORY, History.TITLE) +")" + " AS " + Combined.TITLE + ", " +
                                     // Only use DISPLAY_READER if the matching bookmark entry inside reading
                                     // list folder is not marked as deleted.
                                     "CASE " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.IS_DELETED) + " WHEN 0 THEN CASE " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.PARENT) + " WHEN " + Bookmarks.FIXED_READING_LIST_ID +
                                        " THEN " + Combined.DISPLAY_READER + " ELSE " + Combined.DISPLAY_NORMAL + " END ELSE " +
                                        Combined.DISPLAY_NORMAL + " END AS " + Combined.DISPLAY + ", " +
                                     qualifyColumn(TABLE_HISTORY, History._ID) + " AS " + Combined.HISTORY_ID + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.VISITS) + " AS " + Combined.VISITS + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.DATE_LAST_VISITED) + " AS " + Combined.DATE_LAST_VISITED + ", " +
                                     qualifyColumn(TABLE_HISTORY, History.FAVICON_ID) + " AS " + Combined.FAVICON_ID +
                        " FROM " + TABLE_HISTORY + " LEFT OUTER JOIN " + TABLE_BOOKMARKS +
                            " ON " + qualifyColumn(TABLE_BOOKMARKS, Bookmarks.URL) + " = " + qualifyColumn(TABLE_HISTORY, History.URL) +
                        " WHERE " + qualifyColumn(TABLE_HISTORY, History.URL) + " IS NOT NULL AND " +
                                    qualifyColumn(TABLE_HISTORY, History.IS_DELETED)  + " = 0 AND (" +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE) + " IS NULL OR " +
                                        qualifyColumn(TABLE_BOOKMARKS, Bookmarks.TYPE)  + " = " + Bookmarks.TYPE_BOOKMARK + ") " +
                    ")");

            debug("Creating " + VIEW_COMBINED_WITH_FAVICONS + " view");

            db.execSQL("CREATE VIEW IF NOT EXISTS " + VIEW_COMBINED_WITH_FAVICONS + " AS" +
                    " SELECT " + qualifyColumn(VIEW_COMBINED, "*") + ", " +
                        qualifyColumn(TABLE_FAVICONS, Favicons.URL) + " AS " + Combined.FAVICON_URL + ", " +
                        qualifyColumn(TABLE_FAVICONS, Favicons.DATA) + " AS " + Combined.FAVICON +
                    " FROM " + VIEW_COMBINED + " LEFT OUTER JOIN " + TABLE_FAVICONS +
                        " ON " + Combined.FAVICON_ID + " = " + qualifyColumn(TABLE_FAVICONS, Favicons._ID));
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            debug("Creating browser.db: " + db.getPath());

            createBookmarksTableOn13(db);
            createHistoryTableOn13(db);
            createFaviconsTable(db);
            createThumbnailsTable(db);

            createBookmarksWithFaviconsView(db);
            createHistoryWithFaviconsView(db);
            createCombinedViewOn16(db);

            createOrUpdateSpecialFolder(db, Bookmarks.PLACES_FOLDER_GUID,
                R.string.bookmarks_folder_places, 0);

            createOrUpdateAllSpecialFolders(db);

            // Create distribution bookmarks before our own default bookmarks
            int pos = createDistributionBookmarks(db);
            createDefaultBookmarks(db, pos);
        }

        private String getLocalizedProperty(JSONObject bookmark, String property, Locale locale) throws JSONException {
            // Try the full locale
            String fullLocale = property + "." + locale.toString();
            if (bookmark.has(fullLocale)) {
                return bookmark.getString(fullLocale);
            }
            // Try without a variant
            if (!TextUtils.isEmpty(locale.getVariant())) {
                String noVariant = fullLocale.substring(0, fullLocale.lastIndexOf("_"));
                if (bookmark.has(noVariant)) {
                    return bookmark.getString(noVariant);
                }
            }
            // Try just the language
            String lang = property + "." + locale.getLanguage();
            if (bookmark.has(lang)) {
                return bookmark.getString(lang);
            }
            // Default to the non-localized property name
            return bookmark.getString(property);
        }

        // Returns the number of bookmarks inserted in the db
        private int createDistributionBookmarks(SQLiteDatabase db) {
            JSONArray bookmarks = Distribution.getBookmarks(mContext);
            if (bookmarks == null) {
                return 0;
            }

            Locale locale = Locale.getDefault();
            int pos = 0;
            Integer mobileFolderId = getMobileFolderId(db);
            if (mobileFolderId == null) {
                Log.e(LOGTAG, "Error creating distribution bookmarks: mobileFolderId is null");
                return 0;
            }

            for (int i = 0; i < bookmarks.length(); i++) {
                try {
                    final JSONObject bookmark = bookmarks.getJSONObject(i);

                    String title = getLocalizedProperty(bookmark, "title", locale);
                    final String url = getLocalizedProperty(bookmark, "url", locale);
                    createBookmark(db, title, url, pos, mobileFolderId);

                    if (bookmark.has("pinned")) {
                        try {
                            // Create a fake bookmark in the hidden pinned folder to pin bookmark
                            // to about:home top sites. Pass pos as the pinned position to pin
                            // sites in the order that bookmarks are specified in bookmarks.json.
                            if (bookmark.getBoolean("pinned")) {
                                createBookmark(db, title, url, pos, Bookmarks.FIXED_PINNED_LIST_ID);
                            }
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "Error pinning bookmark to top sites", e);
                        }
                    }

                    pos++;

                    // return early if there is no icon for this bookmark
                    if (!bookmark.has("icon")) {
                        continue;
                    }

                    // create icons in a separate thread to avoid blocking about:home on startup
                    ThreadUtils.postToBackgroundThread(new Runnable() {
                        @Override
                        public void run() {
                            SQLiteDatabase db = getWritableDatabase();
                            try {
                                String iconData = bookmark.getString("icon");
                                Bitmap icon = BitmapUtils.getBitmapFromDataURI(iconData);
                                if (icon != null) {
                                    createFavicon(db, url, icon);
                                }
                            } catch (JSONException e) {
                                Log.e(LOGTAG, "Error creating distribution bookmark icon", e);
                            }
                        }
                    });
                } catch (JSONException e) {
                    Log.e(LOGTAG, "Error creating distribution bookmark", e);
                }
            }
            return pos;
        }

        // Inserts default bookmarks, starting at a specified position
        private void createDefaultBookmarks(SQLiteDatabase db, int pos) {
            Class<?> stringsClass = R.string.class;
            Field[] fields = stringsClass.getFields();
            Pattern p = Pattern.compile("^bookmarkdefaults_title_");

            Integer mobileFolderId = getMobileFolderId(db);
            if (mobileFolderId == null) {
                Log.e(LOGTAG, "Error creating default bookmarks: mobileFolderId is null");
                return;
            }

            for (int i = 0; i < fields.length; i++) {
                final String name = fields[i].getName();
                Matcher m = p.matcher(name);
                if (!m.find()) {
                    continue;
                }
                try {
                    int titleid = fields[i].getInt(null);
                    String title = mContext.getString(titleid);

                    Field urlField = stringsClass.getField(name.replace("_title_", "_url_"));
                    int urlId = urlField.getInt(null);
                    final String url = mContext.getString(urlId);
                    createBookmark(db, title, url, pos, mobileFolderId);

                    // create icons in a separate thread to avoid blocking about:home on startup
                    ThreadUtils.postToBackgroundThread(new Runnable() {
                        @Override
                        public void run() {
                            SQLiteDatabase db = getWritableDatabase();
                            Bitmap icon = getDefaultFaviconFromPath(name);
                            if (icon == null) {
                                icon = getDefaultFaviconFromDrawable(name);
                            }
                            if (icon != null) {
                                createFavicon(db, url, icon);
                            }
                        }
                    });
                    pos++;
                } catch (java.lang.IllegalAccessException ex) {
                    Log.e(LOGTAG, "Can't create bookmark " + name, ex);
                } catch (java.lang.NoSuchFieldException ex) {
                    Log.e(LOGTAG, "Can't create bookmark " + name, ex);
                }
            }
        }

        private void createBookmark(SQLiteDatabase db, String title, String url, int pos, int parent) {
            ContentValues bookmarkValues = new ContentValues();
            bookmarkValues.put(Bookmarks.PARENT, parent);

            long now = System.currentTimeMillis();
            bookmarkValues.put(Bookmarks.DATE_CREATED, now);
            bookmarkValues.put(Bookmarks.DATE_MODIFIED, now);

            bookmarkValues.put(Bookmarks.TITLE, title);
            bookmarkValues.put(Bookmarks.URL, url);
            bookmarkValues.put(Bookmarks.GUID, Utils.generateGuid());
            bookmarkValues.put(Bookmarks.POSITION, pos);
            db.insertOrThrow(TABLE_BOOKMARKS, Bookmarks.TITLE, bookmarkValues);
        }

        private void createFavicon(SQLiteDatabase db, String url, Bitmap icon) {
            ByteArrayOutputStream stream = new ByteArrayOutputStream();

            ContentValues iconValues = new ContentValues();
            iconValues.put(Favicons.PAGE_URL, url);

            byte[] data = null;
            if (icon.compress(Bitmap.CompressFormat.PNG, 100, stream)) {
                data = stream.toByteArray();
            } else {
                Log.w(LOGTAG, "Favicon compression failed.");
            }
            iconValues.put(Favicons.DATA, data);

            insertFavicon(db, iconValues);
        }

        private Bitmap getDefaultFaviconFromPath(String name) {
            Class<?> stringClass = R.string.class;
            try {
                // Look for a drawable with the id R.drawable.bookmarkdefaults_favicon_*
                Field faviconField = stringClass.getField(name.replace("_title_", "_favicon_"));
                if (faviconField == null) {
                    return null;
                }
                int faviconId = faviconField.getInt(null);
                String path = mContext.getString(faviconId);

                String apkPath = mContext.getPackageResourcePath();
                File apkFile = new File(apkPath);
                String bitmapPath = "jar:jar:" + apkFile.toURI() + "!/" + AppConstants.OMNIJAR_NAME + "!/" + path;
                return GeckoJarReader.getBitmap(mContext.getResources(), bitmapPath);
            } catch (java.lang.IllegalAccessException ex) {
                Log.e(LOGTAG, "[Path] Can't create favicon " + name, ex);
            } catch (java.lang.NoSuchFieldException ex) {
                // If the field does not exist, that means we intend to load via a drawable
            }
            return null;
        }

        private Bitmap getDefaultFaviconFromDrawable(String name) {
            Class<?> drawablesClass = R.drawable.class;
            try {
                // Look for a drawable with the id R.drawable.bookmarkdefaults_favicon_*
                Field faviconField = drawablesClass.getField(name.replace("_title_", "_favicon_"));
                if (faviconField == null) {
                    return null;
                }
                int faviconId = faviconField.getInt(null);
                return BitmapUtils.decodeResource(mContext, faviconId);
            } catch (java.lang.IllegalAccessException ex) {
                Log.e(LOGTAG, "[Drawable] Can't create favicon " + name, ex);
            } catch (java.lang.NoSuchFieldException ex) {
                // If the field does not exist, that means we intend to load via a file path
            }
            return null;
        }

        private void createOrUpdateAllSpecialFolders(SQLiteDatabase db) {
            createOrUpdateSpecialFolder(db, Bookmarks.MOBILE_FOLDER_GUID,
                R.string.bookmarks_folder_mobile, 0);
            createOrUpdateSpecialFolder(db, Bookmarks.TOOLBAR_FOLDER_GUID,
                R.string.bookmarks_folder_toolbar, 1);
            createOrUpdateSpecialFolder(db, Bookmarks.MENU_FOLDER_GUID,
                R.string.bookmarks_folder_menu, 2);
            createOrUpdateSpecialFolder(db, Bookmarks.TAGS_FOLDER_GUID,
                R.string.bookmarks_folder_tags, 3);
            createOrUpdateSpecialFolder(db, Bookmarks.UNFILED_FOLDER_GUID,
                R.string.bookmarks_folder_unfiled, 4);
            createOrUpdateSpecialFolder(db, Bookmarks.READING_LIST_FOLDER_GUID,
                R.string.bookmarks_folder_reading_list, 5);
            createOrUpdateSpecialFolder(db, Bookmarks.PINNED_FOLDER_GUID,
                R.string.bookmarks_folder_pinned, 6);
        }

        private void createOrUpdateSpecialFolder(SQLiteDatabase db,
                String guid, int titleId, int position) {
            ContentValues values = new ContentValues();
            values.put(Bookmarks.GUID, guid);
            values.put(Bookmarks.TYPE, Bookmarks.TYPE_FOLDER);
            values.put(Bookmarks.POSITION, position);

            if (guid.equals(Bookmarks.PLACES_FOLDER_GUID))
                values.put(Bookmarks._ID, Bookmarks.FIXED_ROOT_ID);
            else if (guid.equals(Bookmarks.READING_LIST_FOLDER_GUID))
                values.put(Bookmarks._ID, Bookmarks.FIXED_READING_LIST_ID);
            else if (guid.equals(Bookmarks.PINNED_FOLDER_GUID))
                values.put(Bookmarks._ID, Bookmarks.FIXED_PINNED_LIST_ID);

            // Set the parent to 0, which sync assumes is the root
            values.put(Bookmarks.PARENT, Bookmarks.FIXED_ROOT_ID);

            String title = mContext.getResources().getString(titleId);
            values.put(Bookmarks.TITLE, title);

            long now = System.currentTimeMillis();
            values.put(Bookmarks.DATE_CREATED, now);
            values.put(Bookmarks.DATE_MODIFIED, now);

            int updated = db.update(TABLE_BOOKMARKS, values,
                                    Bookmarks.GUID + " = ?",
                                    new String[] { guid });

            if (updated == 0) {
                db.insert(TABLE_BOOKMARKS, Bookmarks.GUID, values);
                debug("Inserted special folder: " + guid);
            } else {
                debug("Updated special folder: " + guid);
            }
        }

        private boolean isSpecialFolder(ContentValues values) {
            String guid = values.getAsString(Bookmarks.GUID);
            if (guid == null)
                return false;

            return guid.equals(Bookmarks.MOBILE_FOLDER_GUID) ||
                   guid.equals(Bookmarks.MENU_FOLDER_GUID) ||
                   guid.equals(Bookmarks.TOOLBAR_FOLDER_GUID) ||
                   guid.equals(Bookmarks.UNFILED_FOLDER_GUID) ||
                   guid.equals(Bookmarks.TAGS_FOLDER_GUID);
        }

        private void migrateBookmarkFolder(SQLiteDatabase db, int folderId,
                BookmarkMigrator migrator) {
            Cursor c = null;

            debug("Migrating bookmark folder with id = " + folderId);

            String selection = Bookmarks.PARENT + " = " + folderId;
            String[] selectionArgs = null;

            boolean isRootFolder = (folderId == Bookmarks.FIXED_ROOT_ID);

            // If we're loading the root folder, we have to account for
            // any previously created special folder that was created without
            // setting a parent id (e.g. mobile folder) and making sure we're
            // not adding any infinite recursion as root's parent is root itself.
            if (isRootFolder) {
                selection = Bookmarks.GUID + " != ?" + " AND (" +
                            selection + " OR " + Bookmarks.PARENT + " = NULL)";
                selectionArgs = new String[] { Bookmarks.PLACES_FOLDER_GUID };
            }

            List<Integer> subFolders = new ArrayList<Integer>();
            List<ContentValues> invalidSpecialEntries = new ArrayList<ContentValues>();

            try {
                c = db.query(TABLE_BOOKMARKS_TMP,
                             null,
                             selection,
                             selectionArgs,
                             null, null, null);

                // The key point here is that bookmarks should be added in
                // parent order to avoid any problems with the foreign key
                // in Bookmarks.PARENT.
                while (c.moveToNext()) {
                    ContentValues values = new ContentValues();

                    // We're using a null projection in the query which
                    // means we're getting all columns from the table.
                    // It's safe to simply transform the row into the
                    // values to be inserted on the new table.
                    DatabaseUtils.cursorRowToContentValues(c, values);

                    boolean isSpecialFolder = isSpecialFolder(values);

                    // The mobile folder used to be created with PARENT = NULL.
                    // We want fix that here.
                    if (values.getAsLong(Bookmarks.PARENT) == null && isSpecialFolder)
                        values.put(Bookmarks.PARENT, Bookmarks.FIXED_ROOT_ID);

                    if (isRootFolder && !isSpecialFolder) {
                        invalidSpecialEntries.add(values);
                        continue;
                    }

                    if (migrator != null)
                        migrator.updateForNewTable(values);

                    debug("Migrating bookmark: " + values.getAsString(Bookmarks.TITLE));
                    db.insert(TABLE_BOOKMARKS, Bookmarks.URL, values);

                    Integer type = values.getAsInteger(Bookmarks.TYPE);
                    if (type != null && type == Bookmarks.TYPE_FOLDER)
                        subFolders.add(values.getAsInteger(Bookmarks._ID));
                }
            } finally {
                if (c != null)
                    c.close();
            }

            // At this point is safe to assume that the mobile folder is
            // in the new table given that we've always created it on
            // database creation time.
            final int nInvalidSpecialEntries = invalidSpecialEntries.size();
            if (nInvalidSpecialEntries > 0) {
                Integer mobileFolderId = getMobileFolderId(db);
                if (mobileFolderId == null) {
                    Log.e(LOGTAG, "Error migrating invalid special folder entries: mobile folder id is null");
                    return;
                }

                debug("Found " + nInvalidSpecialEntries + " invalid special folder entries");
                for (int i = 0; i < nInvalidSpecialEntries; i++) {
                    ContentValues values = invalidSpecialEntries.get(i);
                    values.put(Bookmarks.PARENT, mobileFolderId);

                    db.insert(TABLE_BOOKMARKS, Bookmarks.URL, values);
                }
            }

            final int nSubFolders = subFolders.size();
            for (int i = 0; i < nSubFolders; i++) {
                int subFolderId = subFolders.get(i);
                migrateBookmarkFolder(db, subFolderId, migrator);
            }
        }

        private void migrateBookmarksTable(SQLiteDatabase db) {
            migrateBookmarksTable(db, null);
        }

        private void migrateBookmarksTable(SQLiteDatabase db, BookmarkMigrator migrator) {
            debug("Renaming bookmarks table to " + TABLE_BOOKMARKS_TMP);
            db.execSQL("ALTER TABLE " + TABLE_BOOKMARKS +
                       " RENAME TO " + TABLE_BOOKMARKS_TMP);

            debug("Dropping views and indexes related to " + TABLE_BOOKMARKS);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_BOOKMARKS_WITH_IMAGES);

            db.execSQL("DROP INDEX IF EXISTS bookmarks_url_index");
            db.execSQL("DROP INDEX IF EXISTS bookmarks_type_deleted_index");
            db.execSQL("DROP INDEX IF EXISTS bookmarks_guid_index");
            db.execSQL("DROP INDEX IF EXISTS bookmarks_modified_index");

            createBookmarksTable(db);
            createBookmarksWithImagesView(db);

            createOrUpdateSpecialFolder(db, Bookmarks.PLACES_FOLDER_GUID,
                R.string.bookmarks_folder_places, 0);

            migrateBookmarkFolder(db, Bookmarks.FIXED_ROOT_ID, migrator);

            // Ensure all special folders exist and have the
            // right folder hierarchy.
            createOrUpdateAllSpecialFolders(db);

            debug("Dropping bookmarks temporary table");
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_BOOKMARKS_TMP);
        }


        private void migrateHistoryTable(SQLiteDatabase db) {
            debug("Renaming history table to " + TABLE_HISTORY_TMP);
            db.execSQL("ALTER TABLE " + TABLE_HISTORY +
                       " RENAME TO " + TABLE_HISTORY_TMP);

            debug("Dropping views and indexes related to " + TABLE_HISTORY);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_HISTORY_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);

            db.execSQL("DROP INDEX IF EXISTS history_url_index");
            db.execSQL("DROP INDEX IF EXISTS history_guid_index");
            db.execSQL("DROP INDEX IF EXISTS history_modified_index");
            db.execSQL("DROP INDEX IF EXISTS history_visited_index");

            createHistoryTable(db);
            createHistoryWithImagesView(db);
            createCombinedWithImagesView(db);

            db.execSQL("INSERT INTO " + TABLE_HISTORY + " SELECT * FROM " + TABLE_HISTORY_TMP);

            debug("Dropping history temporary table");
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_HISTORY_TMP);
        }

        private void migrateImagesTable(SQLiteDatabase db) {
            debug("Renaming images table to " + TABLE_IMAGES_TMP);
            db.execSQL("ALTER TABLE " + Obsolete.TABLE_IMAGES +
                       " RENAME TO " + TABLE_IMAGES_TMP);

            debug("Dropping views and indexes related to " + Obsolete.TABLE_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_HISTORY_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);

            db.execSQL("DROP INDEX IF EXISTS images_url_index");
            db.execSQL("DROP INDEX IF EXISTS images_guid_index");
            db.execSQL("DROP INDEX IF EXISTS images_modified_index");

            createImagesTable(db);
            createHistoryWithImagesView(db);
            createCombinedWithImagesView(db);

            db.execSQL("INSERT INTO " + Obsolete.TABLE_IMAGES + " SELECT * FROM " + TABLE_IMAGES_TMP);

            debug("Dropping images temporary table");
            db.execSQL("DROP TABLE IF EXISTS " + TABLE_IMAGES_TMP);
        }

        private void upgradeDatabaseFrom1to2(SQLiteDatabase db) {
            migrateBookmarksTable(db);
        }

        private void upgradeDatabaseFrom2to3(SQLiteDatabase db) {
            debug("Dropping view: " + Obsolete.VIEW_BOOKMARKS_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_BOOKMARKS_WITH_IMAGES);

            createBookmarksWithImagesView(db);

            debug("Dropping view: " + Obsolete.VIEW_HISTORY_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_HISTORY_WITH_IMAGES);

            createHistoryWithImagesView(db);
        }

        private void upgradeDatabaseFrom3to4(SQLiteDatabase db) {
            migrateBookmarksTable(db, new BookmarkMigrator3to4());
        }

        private void upgradeDatabaseFrom4to5(SQLiteDatabase db) {
            createCombinedWithImagesView(db);
        }

        private void upgradeDatabaseFrom5to6(SQLiteDatabase db) {
            debug("Dropping view: " + Obsolete.VIEW_COMBINED_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);

            createCombinedWithImagesView(db);
        }

        private void upgradeDatabaseFrom6to7(SQLiteDatabase db) {
            debug("Removing history visits with NULL GUIDs");
            db.execSQL("DELETE FROM " + TABLE_HISTORY + " WHERE " + History.GUID + " IS NULL");

            debug("Update images with NULL GUIDs");
            String[] columns = new String[] { Obsolete.Images._ID };
            Cursor cursor = null;
            try {
              cursor = db.query(Obsolete.TABLE_IMAGES, columns, Obsolete.Images.GUID + " IS NULL", null, null ,null, null, null);
              ContentValues values = new ContentValues();
              if (cursor.moveToFirst()) {
                  do {
                      values.put(Obsolete.Images.GUID, Utils.generateGuid());
                      db.update(Obsolete.TABLE_IMAGES, values, Obsolete.Images._ID + " = ?", new String[] {
                        cursor.getString(cursor.getColumnIndexOrThrow(Obsolete.Images._ID))
                      });
                  } while (cursor.moveToNext());
              }
            } finally {
              if (cursor != null)
                cursor.close();
            }

            migrateBookmarksTable(db);
            migrateHistoryTable(db);
            migrateImagesTable(db);
        }

        private void upgradeDatabaseFrom7to8(SQLiteDatabase db) {
            debug("Combining history entries with the same URL");

            final String TABLE_DUPES = "duped_urls";
            final String TOTAL = "total";
            final String LATEST = "latest";
            final String WINNER = "winner";

            db.execSQL("CREATE TEMP TABLE " + TABLE_DUPES + " AS" +
                      " SELECT " + History.URL + ", " +
                                  "SUM(" + History.VISITS + ") AS " + TOTAL + ", " +
                                  "MAX(" + History.DATE_MODIFIED + ") AS " + LATEST + ", " +
                                  "MAX(" + History._ID + ") AS " + WINNER +
                      " FROM " + TABLE_HISTORY +
                      " GROUP BY " + History.URL +
                      " HAVING count(" + History.URL + ") > 1");

            db.execSQL("CREATE UNIQUE INDEX " + TABLE_DUPES + "_url_index ON " +
                       TABLE_DUPES + " (" + History.URL + ")");

            final String fromClause = " FROM " + TABLE_DUPES + " WHERE " +
                                      qualifyColumn(TABLE_DUPES, History.URL) + " = " +
                                      qualifyColumn(TABLE_HISTORY, History.URL);

            db.execSQL("UPDATE " + TABLE_HISTORY +
                      " SET " + History.VISITS + " = (SELECT " + TOTAL + fromClause + "), " +
                                History.DATE_MODIFIED + " = (SELECT " + LATEST + fromClause + "), " +
                                History.IS_DELETED + " = " +
                                    "(" + History._ID + " <> (SELECT " + WINNER + fromClause + "))" +
                      " WHERE " + History.URL + " IN (SELECT " + History.URL + " FROM " + TABLE_DUPES + ")");

            db.execSQL("DROP TABLE " + TABLE_DUPES);
        }

        private void upgradeDatabaseFrom8to9(SQLiteDatabase db) {
            createOrUpdateSpecialFolder(db, Bookmarks.READING_LIST_FOLDER_GUID,
                R.string.bookmarks_folder_reading_list, 5);

            debug("Dropping view: " + Obsolete.VIEW_COMBINED_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);

            createCombinedWithImagesViewOn9(db);
        }

        private void upgradeDatabaseFrom9to10(SQLiteDatabase db) {
            debug("Dropping view: " + Obsolete.VIEW_COMBINED_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);

            createCombinedWithImagesViewOn10(db);
        }

        private void upgradeDatabaseFrom10to11(SQLiteDatabase db) {
            debug("Dropping view: " + Obsolete.VIEW_COMBINED_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);

            db.execSQL("CREATE INDEX bookmarks_type_deleted_index ON " + TABLE_BOOKMARKS + "("
                    + Bookmarks.TYPE + ", " + Bookmarks.IS_DELETED + ")");

            createCombinedWithImagesViewOn11(db);
        }

        private void upgradeDatabaseFrom11to12(SQLiteDatabase db) {
            debug("Dropping view: " + Obsolete.VIEW_COMBINED_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);

            createCombinedViewOn12(db);
        }

        private void upgradeDatabaseFrom12to13(SQLiteDatabase db) {
            // Update images table with favicon URLs
            SQLiteDatabase faviconsDb = null;
            Cursor c = null;
            try {
                final String FAVICON_TABLE = "favicon_urls";
                final String FAVICON_URL = "favicon_url";
                final String FAVICON_PAGE = "page_url";

                String dbPath = mContext.getDatabasePath(Obsolete.FAVICON_DB).getPath();
                faviconsDb = SQLiteDatabase.openDatabase(dbPath, null, SQLiteDatabase.OPEN_READONLY);
                String[] columns = new String[] { FAVICON_URL, FAVICON_PAGE };
                c = faviconsDb.query(FAVICON_TABLE, columns, null, null, null, null, null, null);
                int faviconIndex = c.getColumnIndexOrThrow(FAVICON_URL);
                int pageIndex = c.getColumnIndexOrThrow(FAVICON_PAGE);
                while (c.moveToNext()) {
                    ContentValues values = new ContentValues(1);
                    String faviconUrl = c.getString(faviconIndex);
                    String pageUrl = c.getString(pageIndex);
                    values.put(FAVICON_URL, faviconUrl);
                    db.update(Obsolete.TABLE_IMAGES, values, Obsolete.Images.URL + " = ?", new String[] { pageUrl });
                }
            } catch (SQLException e) {
                // If we can't read from the database for some reason, we won't
                // be able to import the favicon URLs. This isn't a fatal
                // error, so continue the upgrade.
                Log.e(LOGTAG, "Exception importing from " + Obsolete.FAVICON_DB, e);
            } finally {
                if (c != null)
                    c.close();
                if (faviconsDb != null)
                    faviconsDb.close();
            }

            createFaviconsTable(db);

            // Import favicons into the favicons table
            db.execSQL("ALTER TABLE " + TABLE_HISTORY
                    + " ADD COLUMN " + History.FAVICON_ID + " INTEGER");
            db.execSQL("ALTER TABLE " + TABLE_BOOKMARKS
                    + " ADD COLUMN " + Bookmarks.FAVICON_ID + " INTEGER");

            try {
                c = db.query(Obsolete.TABLE_IMAGES,
                        new String[] {
                            Obsolete.Images.URL,
                            Obsolete.Images.FAVICON_URL,
                            Obsolete.Images.FAVICON,
                            Obsolete.Images.DATE_MODIFIED,
                            Obsolete.Images.DATE_CREATED
                        },
                        Obsolete.Images.FAVICON + " IS NOT NULL",
                        null, null, null, null);

                while (c.moveToNext()) {
                    long faviconId = -1;
                    int faviconUrlIndex = c.getColumnIndexOrThrow(Obsolete.Images.FAVICON_URL);
                    String faviconUrl = null;
                    if (!c.isNull(faviconUrlIndex)) {
                        faviconUrl = c.getString(faviconUrlIndex);
                        Cursor c2 = null;
                        try {
                            c2 = db.query(TABLE_FAVICONS,
                                    new String[] { Favicons._ID },
                                    Favicons.URL + " = ?",
                                    new String[] { faviconUrl },
                                    null, null, null);
                            if (c2.moveToFirst()) {
                                faviconId = c2.getLong(c2.getColumnIndexOrThrow(Favicons._ID));
                            }
                        } finally {
                            if (c2 != null)
                                c2.close();
                        }
                    }

                    if (faviconId == -1) {
                        ContentValues values = new ContentValues(4);
                        values.put(Favicons.URL, faviconUrl);
                        values.put(Favicons.DATA, c.getBlob(c.getColumnIndexOrThrow(Obsolete.Images.FAVICON)));
                        values.put(Favicons.DATE_MODIFIED, c.getLong(c.getColumnIndexOrThrow(Obsolete.Images.DATE_MODIFIED)));
                        values.put(Favicons.DATE_CREATED, c.getLong(c.getColumnIndexOrThrow(Obsolete.Images.DATE_CREATED)));
                        faviconId = db.insert(TABLE_FAVICONS, null, values);
                    }

                    ContentValues values = new ContentValues(1);
                    values.put(FaviconColumns.FAVICON_ID, faviconId);
                    db.update(TABLE_HISTORY, values, History.URL + " = ?",
                            new String[] { c.getString(c.getColumnIndexOrThrow(Obsolete.Images.URL)) });
                    db.update(TABLE_BOOKMARKS, values, Bookmarks.URL + " = ?",
                            new String[] { c.getString(c.getColumnIndexOrThrow(Obsolete.Images.URL)) });
                }
            } finally {
                if (c != null)
                    c.close();
            }

            createThumbnailsTable(db);

            // Import thumbnails into the thumbnails table
            db.execSQL("INSERT INTO " + TABLE_THUMBNAILS + " ("
                    + Thumbnails.URL + ", "
                    + Thumbnails.DATA + ") "
                    + "SELECT " + Obsolete.Images.URL + ", " + Obsolete.Images.THUMBNAIL
                    + " FROM " + Obsolete.TABLE_IMAGES
                    + " WHERE " + Obsolete.Images.THUMBNAIL + " IS NOT NULL");

            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_BOOKMARKS_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_HISTORY_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + Obsolete.VIEW_COMBINED_WITH_IMAGES);
            db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED);

            createBookmarksWithFaviconsView(db);
            createHistoryWithFaviconsView(db);
            createCombinedViewOn13(db);

            db.execSQL("DROP TABLE IF EXISTS " + Obsolete.TABLE_IMAGES);
        }

        private void upgradeDatabaseFrom13to14(SQLiteDatabase db) {
            createOrUpdateSpecialFolder(db, Bookmarks.PINNED_FOLDER_GUID,
                R.string.bookmarks_folder_pinned, 6);
        }

        private void upgradeDatabaseFrom14to15(SQLiteDatabase db) {
            Cursor c = null;
            try {
                // Get all the pinned bookmarks
                c = db.query(TABLE_BOOKMARKS,
                             new String[] { Bookmarks._ID, Bookmarks.URL },
                             Bookmarks.PARENT + " = ?",
                             new String[] { Integer.toString(Bookmarks.FIXED_PINNED_LIST_ID) },
                             null, null, null);

                while (c.moveToNext()) {
                    // Check if this URL can be parsed as a URI with a valid scheme.
                    String url = c.getString(c.getColumnIndexOrThrow(Bookmarks.URL));
                    if (Uri.parse(url).getScheme() != null) {
                        continue;
                    }

                    // If it can't, update the URL to be an encoded "user-entered" value.
                    ContentValues values = new ContentValues(1);
                    String newUrl = Uri.fromParts("user-entered", url, null).toString();
                    values.put(Bookmarks.URL, newUrl);
                    db.update(TABLE_BOOKMARKS, values, Bookmarks._ID + " = ?",
                              new String[] { Integer.toString(c.getInt(c.getColumnIndexOrThrow(Bookmarks._ID))) });
                }
            } finally {
                if (c != null) {
                    c.close();
                }
            }
        }

        private void upgradeDatabaseFrom15to16(SQLiteDatabase db) {
            db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED);
            db.execSQL("DROP VIEW IF EXISTS " + VIEW_COMBINED_WITH_FAVICONS);

            createCombinedViewOn16(db);
        }

        private void upgradeDatabaseFrom16to17(SQLiteDatabase db) {
            // Purge any 0-byte favicons/thumbnails
            try {
                db.execSQL("DELETE FROM " + TABLE_FAVICONS +
                        " WHERE length(" + Favicons.DATA + ") = 0");
                db.execSQL("DELETE FROM " + TABLE_THUMBNAILS +
                        " WHERE length(" + Thumbnails.DATA + ") = 0");
            } catch (SQLException e) {
                Log.e(LOGTAG, "Error purging invalid favicons or thumbnails", e);
            }
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            debug("Upgrading browser.db: " + db.getPath() + " from " +
                    oldVersion + " to " + newVersion);

            // We have to do incremental upgrades until we reach the current
            // database schema version.
            for (int v = oldVersion + 1; v <= newVersion; v++) {
                switch(v) {
                    case 2:
                        upgradeDatabaseFrom1to2(db);
                        break;

                    case 3:
                        upgradeDatabaseFrom2to3(db);
                        break;

                    case 4:
                        upgradeDatabaseFrom3to4(db);
                        break;

                    case 5:
                        upgradeDatabaseFrom4to5(db);
                        break;

                    case 6:
                        upgradeDatabaseFrom5to6(db);
                        break;

                    case 7:
                        upgradeDatabaseFrom6to7(db);
                        break;

                    case 8:
                        upgradeDatabaseFrom7to8(db);
                        break;

                    case 9:
                        upgradeDatabaseFrom8to9(db);
                        break;

                    case 10:
                        upgradeDatabaseFrom9to10(db);
                        break;

                    case 11:
                        upgradeDatabaseFrom10to11(db);
                        break;

                    case 12:
                        upgradeDatabaseFrom11to12(db);
                        break;

                    case 13:
                        upgradeDatabaseFrom12to13(db);
                        break;

                    case 14:
                        upgradeDatabaseFrom13to14(db);
                        break;

                    case 15:
                        upgradeDatabaseFrom14to15(db);
                        break;

                    case 16:
                        upgradeDatabaseFrom15to16(db);
                        break;

                    case 17:
                        upgradeDatabaseFrom16to17(db);
                        break;
                }
            }

            // If an upgrade after 12->13 fails, the entire upgrade is rolled
            // back, but we can't undo the deletion of favicon_urls.db if we
            // delete this in step 13; therefore, we wait until all steps are
            // complete before removing it.
            if (oldVersion < 13 && newVersion >= 13
                                && mContext.getDatabasePath(Obsolete.FAVICON_DB).exists()
                                && !mContext.deleteDatabase(Obsolete.FAVICON_DB)) {
                throw new SQLException("Could not delete " + Obsolete.FAVICON_DB);
            }
        }

        @Override
        public void onOpen(SQLiteDatabase db) {
            debug("Opening browser.db: " + db.getPath());

            Cursor cursor = null;
            try {
                cursor = db.rawQuery("PRAGMA foreign_keys=ON", null);
            } finally {
                if (cursor != null)
                    cursor.close();
            }
            cursor = null;
            try {
                cursor = db.rawQuery("PRAGMA synchronous=NORMAL", null);
            } finally {
                if (cursor != null)
                    cursor.close();
            }

            // From Honeycomb on, it's possible to run several db
            // commands in parallel using multiple connections.
            if (Build.VERSION.SDK_INT >= 11) {
                db.enableWriteAheadLogging();
                db.setLockingEnabled(false);
            } else {
                // Pre-Honeycomb, we can do some lesser optimizations.
                cursor = null;
                try {
                    cursor = db.rawQuery("PRAGMA journal_mode=PERSIST", null);
                } finally {
                    if (cursor != null)
                        cursor.close();
                }
            }
        }
    }

    private static final String[] mobileIdColumns = new String[] { Bookmarks._ID };
    private static final String[] mobileIdSelectionArgs = new String[] { Bookmarks.MOBILE_FOLDER_GUID };

    private Integer getMobileFolderId(SQLiteDatabase db) {
        Cursor c = null;

        try {
            c = db.query(TABLE_BOOKMARKS,
                         mobileIdColumns,
                         Bookmarks.GUID + " = ?",
                         mobileIdSelectionArgs,
                         null, null, null);

            if (c == null || !c.moveToFirst())
                return null;

            return c.getInt(c.getColumnIndex(Bookmarks._ID));
        } finally {
            if (c != null)
                c.close();
        }
    }

    private SQLiteDatabase getReadableDatabase(Uri uri) {
        trace("Getting readable database for URI: " + uri);

        String profile = null;

        if (uri != null)
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);

        return mDatabases.getDatabaseHelperForProfile(profile, isTest(uri)).getReadableDatabase();
    }

    private SQLiteDatabase getWritableDatabase(Uri uri) {
        trace("Getting writable database for URI: " + uri);

        String profile = null;

        if (uri != null)
            profile = uri.getQueryParameter(BrowserContract.PARAM_PROFILE);

        return mDatabases.getDatabaseHelperForProfile(profile, isTest(uri)).getWritableDatabase();
    }

    private void cleanupSomeDeletedRecords(Uri fromUri, Uri targetUri, String tableName) {
        Log.d(LOGTAG, "Cleaning up deleted records from " + tableName);

        // we cleanup records marked as deleted that are older than a
        // predefined max age. It's important not be too greedy here and
        // remove only a few old deleted records at a time.

        // The PARAM_SHOW_DELETED argument is necessary to return the records
        // that were marked as deleted. We use PARAM_IS_SYNC here to ensure
        // that we'll be actually deleting records instead of flagging them.
        Uri.Builder uriBuilder = targetUri.buildUpon()
                .appendQueryParameter(BrowserContract.PARAM_LIMIT, String.valueOf(DELETED_RECORDS_PURGE_LIMIT))
                .appendQueryParameter(BrowserContract.PARAM_SHOW_DELETED, "1")
                .appendQueryParameter(BrowserContract.PARAM_IS_SYNC, "1");

        String profile = fromUri.getQueryParameter(BrowserContract.PARAM_PROFILE);
        if (!TextUtils.isEmpty(profile))
            uriBuilder = uriBuilder.appendQueryParameter(BrowserContract.PARAM_PROFILE, profile);

        if (isTest(fromUri))
            uriBuilder = uriBuilder.appendQueryParameter(BrowserContract.PARAM_IS_TEST, "1");

        Uri uriWithArgs = uriBuilder.build();

        Cursor cursor = null;

        try {
            long now = System.currentTimeMillis();
            String selection = SyncColumns.IS_DELETED + " = 1 AND " +
                    SyncColumns.DATE_MODIFIED + " <= " + (now - MAX_AGE_OF_DELETED_RECORDS);

            cursor = query(uriWithArgs,
                           new String[] { CommonColumns._ID },
                           selection,
                           null,
                           null);

            while (cursor.moveToNext()) {
                Uri uriWithId = ContentUris.withAppendedId(uriWithArgs, cursor.getLong(0));
                delete(uriWithId, null, null);

                debug("Removed old deleted item with URI: " + uriWithId);
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }
    }

    /**
     * Remove enough history items to bring the database count below <code>retain</code>,
     * removing no items with a modified time after <code>keepAfter</code>.
     *
     * Provide <code>keepAfter</code> less than or equal to zero to skip that check.
     *
     * Items will be removed according to an approximate frecency calculation.
     *
     * Call this method within a transaction.
     */
    private void expireHistory(final SQLiteDatabase db, final int retain, final long keepAfter) {
        Log.d(LOGTAG, "Expiring history.");
        final long rows = DatabaseUtils.queryNumEntries(db, TABLE_HISTORY);

        if (retain >= rows) {
            debug("Not expiring history: only have " + rows + " rows.");
            return;
        }

        final String sortOrder = BrowserContract.getFrecencySortOrder(false, true);
        final long toRemove = rows - retain;
        debug("Expiring at most " + toRemove + " rows earlier than " + keepAfter + ".");

        final String sql;
        if (keepAfter > 0) {
            sql = "DELETE FROM " + TABLE_HISTORY + " " +
                  "WHERE MAX(" + History.DATE_LAST_VISITED + ", " + History.DATE_MODIFIED +") < " + keepAfter + " " +
                  " AND " + History._ID + " IN ( SELECT " +
                    History._ID + " FROM " + TABLE_HISTORY + " " +
                    "ORDER BY " + sortOrder + " LIMIT " + toRemove +
                  ")";
        } else {
            sql = "DELETE FROM " + TABLE_HISTORY + " WHERE " + History._ID + " " +
                  "IN ( SELECT " + History._ID + " FROM " + TABLE_HISTORY + " " +
                  "ORDER BY " + sortOrder + " LIMIT " + toRemove + ")";
        }
        trace("Deleting using query: " + sql);
        db.execSQL(sql);
    }

    /**
     * Remove any thumbnails that for sites that aren't likely to be ever shown.
     * Items will be removed according to a frecency calculation and only if they are not pinned
     *
     * Call this method within a transaction.
     */
    private void expireThumbnails(final SQLiteDatabase db) {
        Log.d(LOGTAG, "Expiring thumbnails.");
        final String sortOrder = BrowserContract.getFrecencySortOrder(true, false);
        final String sql = "DELETE FROM " + TABLE_THUMBNAILS +
                           " WHERE " + Thumbnails.URL + " NOT IN ( " +
                             " SELECT " + Combined.URL +
                             " FROM " + VIEW_COMBINED +
                             " ORDER BY " + sortOrder +
                             " LIMIT " + DEFAULT_EXPIRY_THUMBNAIL_COUNT +
                           ") AND " + Thumbnails.URL + " NOT IN ( " +
                             " SELECT " + Bookmarks.URL +
                             " FROM " + TABLE_BOOKMARKS +
                             " WHERE " + Bookmarks.PARENT + " = " + Bookmarks.FIXED_PINNED_LIST_ID +
                           ")";
        trace("Clear thumbs using query: " + sql);
        db.execSQL(sql);
    }

    private boolean isCallerSync(Uri uri) {
        String isSync = uri.getQueryParameter(BrowserContract.PARAM_IS_SYNC);
        return !TextUtils.isEmpty(isSync);
    }

    private boolean isTest(Uri uri) {
        String isTest = uri.getQueryParameter(BrowserContract.PARAM_IS_TEST);
        return !TextUtils.isEmpty(isTest);
    }

    private boolean shouldShowDeleted(Uri uri) {
        String showDeleted = uri.getQueryParameter(BrowserContract.PARAM_SHOW_DELETED);
        return !TextUtils.isEmpty(showDeleted);
    }

    private boolean shouldUpdateOrInsert(Uri uri) {
        String insertIfNeeded = uri.getQueryParameter(BrowserContract.PARAM_INSERT_IF_NEEDED);
        return Boolean.parseBoolean(insertIfNeeded);
    }

    private boolean shouldIncrementVisits(Uri uri) {
        String incrementVisits = uri.getQueryParameter(BrowserContract.PARAM_INCREMENT_VISITS);
        return Boolean.parseBoolean(incrementVisits);
    }

    @Override
    public boolean onCreate() {
        debug("Creating BrowserProvider");

        synchronized (this) {
            mContext = getContext();
            mDatabases = new PerProfileDatabases<BrowserDatabaseHelper>(
                getContext(), DATABASE_NAME, new DatabaseHelperFactory<BrowserDatabaseHelper>() {
                    @Override
                    public BrowserDatabaseHelper makeDatabaseHelper(Context context, String databasePath) {
                        return new BrowserDatabaseHelper(context, databasePath);
                    }
                });
        }

        return true;
    }

    @Override
    public String getType(Uri uri) {
        final int match = URI_MATCHER.match(uri);

        trace("Getting URI type: " + uri);

        switch (match) {
            case BOOKMARKS:
                trace("URI is BOOKMARKS: " + uri);
                return Bookmarks.CONTENT_TYPE;
            case BOOKMARKS_ID:
                trace("URI is BOOKMARKS_ID: " + uri);
                return Bookmarks.CONTENT_ITEM_TYPE;
            case HISTORY:
                trace("URI is HISTORY: " + uri);
                return History.CONTENT_TYPE;
            case HISTORY_ID:
                trace("URI is HISTORY_ID: " + uri);
                return History.CONTENT_ITEM_TYPE;
            case SEARCH_SUGGEST:
                trace("URI is SEARCH_SUGGEST: " + uri);
                return SearchManager.SUGGEST_MIME_TYPE;
        }

        debug("URI has unrecognized type: " + uri);

        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int deleted = 0;

        if (Build.VERSION.SDK_INT >= 11) {
            trace("Beginning delete transaction: " + uri);
            db.beginTransaction();
            try {
                deleted = deleteInTransaction(db, uri, selection, selectionArgs);
                db.setTransactionSuccessful();
                trace("Successful delete transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            deleted = deleteInTransaction(db, uri, selection, selectionArgs);
        }

        if (deleted > 0)
            getContext().getContentResolver().notifyChange(uri, null);

        return deleted;
    }

    @SuppressWarnings("fallthrough")
    public int deleteInTransaction(SQLiteDatabase db, Uri uri, String selection, String[] selectionArgs) {
        trace("Calling delete in transaction on URI: " + uri);

        final int match = URI_MATCHER.match(uri);
        int deleted = 0;

        switch (match) {
            case BOOKMARKS_ID:
                trace("Delete on BOOKMARKS_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case BOOKMARKS: {
                trace("Deleting bookmarks: " + uri);
                deleted = deleteBookmarks(uri, selection, selectionArgs);
                deleteUnusedImages(uri);
                break;
            }

            case HISTORY_ID:
                trace("Delete on HISTORY_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                trace("Deleting history: " + uri);
                deleted = deleteHistory(uri, selection, selectionArgs);
                deleteUnusedImages(uri);
                break;
            }

            case HISTORY_OLD: {
                String priority = uri.getQueryParameter(BrowserContract.PARAM_EXPIRE_PRIORITY);
                long keepAfter = System.currentTimeMillis() - DEFAULT_EXPIRY_PRESERVE_WINDOW;
                int retainCount = DEFAULT_EXPIRY_RETAIN_COUNT;

                if (BrowserContract.ExpirePriority.AGGRESSIVE.toString().equals(priority)) {
                    keepAfter = 0;
                    retainCount = AGGRESSIVE_EXPIRY_RETAIN_COUNT;
                }
                expireHistory(db, retainCount, keepAfter);
                expireThumbnails(db);
                deleteUnusedImages(uri);
                break;
            }

            case FAVICON_ID:
                debug("Delete on FAVICON_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_FAVICONS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case FAVICONS: {
                trace("Deleting favicons: " + uri);
                deleted = deleteFavicons(uri, selection, selectionArgs);
                break;
            }

            case THUMBNAIL_ID:
                debug("Delete on THUMBNAIL_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_THUMBNAILS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case THUMBNAILS: {
                trace("Deleting thumbnails: " + uri);
                deleted = deleteThumbnails(uri, selection, selectionArgs);
                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown delete URI " + uri);
        }

        debug("Deleted " + deleted + " rows for URI: " + uri);

        return deleted;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        trace("Calling insert on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        Uri result = null;
        try {
            if (Build.VERSION.SDK_INT >= 11) {
                trace("Beginning insert transaction: " + uri);
                db.beginTransaction();
                try {
                    result = insertInTransaction(uri, values);
                    db.setTransactionSuccessful();
                    trace("Successful insert transaction: " + uri);
                } finally {
                    db.endTransaction();
                }
            } else {
                result = insertInTransaction(uri, values);
            }
        } catch (SQLException sqle) {
            Log.e(LOGTAG, "exception in DB operation", sqle);
        } catch (UnsupportedOperationException uoe) {
            Log.e(LOGTAG, "don't know how to perform that insert", uoe);
        }

        if (result != null)
            getContext().getContentResolver().notifyChange(uri, null);

        return result;
    }

    public Uri insertInTransaction(Uri uri, ContentValues values) {
        trace("Calling insert in transaction on URI: " + uri);

        int match = URI_MATCHER.match(uri);
        long id = -1;

        switch (match) {
            case BOOKMARKS: {
                trace("Insert on BOOKMARKS: " + uri);
                id = insertBookmark(uri, values);
                break;
            }

            case HISTORY: {
                trace("Insert on HISTORY: " + uri);
                id = insertHistory(uri, values);
                break;
            }

            case FAVICONS: {
                trace("Insert on FAVICONS: " + uri);
                id = insertFavicon(uri, values);
                break;
            }

            case THUMBNAILS: {
                trace("Insert on THUMBNAILS: " + uri);
                id = insertThumbnail(uri, values);
                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown insert URI " + uri);
        }

        debug("Inserted ID in database: " + id);

        if (id >= 0)
            return ContentUris.withAppendedId(uri, id);

        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Calling update on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        if (Build.VERSION.SDK_INT >= 11) {
            trace("Beginning update transaction: " + uri);
            db.beginTransaction();
            try {
                updated = updateInTransaction(uri, values, selection, selectionArgs);
                db.setTransactionSuccessful();
                trace("Successful update transaction: " + uri);
            } finally {
                db.endTransaction();
            }
        } else {
            updated = updateInTransaction(uri, values, selection, selectionArgs);
        }

        if (updated > 0)
            getContext().getContentResolver().notifyChange(uri, null);

        return updated;
    }

    @SuppressWarnings("fallthrough")
    public int updateInTransaction(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Calling update in transaction on URI: " + uri);

        int match = URI_MATCHER.match(uri);
        int updated = 0;

        switch (match) {
            // We provide a dedicated (hacky) API for callers to bulk-update the positions of
            // folder children by passing an array of GUID strings as `selectionArgs`.
            // Each child will have its position column set to its index in the provided array.
            //
            // This avoids callers having to issue a large number of UPDATE queries through
            // the usual channels. See Bug 728783.
            //
            // Note that this is decidedly not a general-purpose API; use at your own risk.
            // `values` and `selection` are ignored.
            case BOOKMARKS_POSITIONS: {
                debug("Update on BOOKMARKS_POSITIONS: " + uri);
                updated = updateBookmarkPositions(uri, selectionArgs);
                break;
            }

            case BOOKMARKS_PARENT: {
                debug("Update on BOOKMARKS_PARENT: " + uri);
                updated = updateBookmarkParents(uri, values, selection, selectionArgs);
                break;
            }

            case BOOKMARKS_ID:
                debug("Update on BOOKMARKS_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_BOOKMARKS + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case BOOKMARKS: {
                debug("Updating bookmark: " + uri);
                if (shouldUpdateOrInsert(uri))
                    updated = updateOrInsertBookmark(uri, values, selection, selectionArgs);
                else
                    updated = updateBookmarks(uri, values, selection, selectionArgs);
                break;
            }

            case HISTORY_ID:
                debug("Update on HISTORY_ID: " + uri);

                selection = DBUtils.concatenateWhere(selection, TABLE_HISTORY + "._id = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                debug("Updating history: " + uri);
                if (shouldUpdateOrInsert(uri))
                    updated = updateOrInsertHistory(uri, values, selection, selectionArgs);
                else
                    updated = updateHistory(uri, values, selection, selectionArgs);
                break;
            }

            case FAVICONS: {
                debug("Update on FAVICONS: " + uri);

                String url = values.getAsString(Favicons.URL);
                String faviconSelection = null;
                String[] faviconSelectionArgs = null;

                if (!TextUtils.isEmpty(url)) {
                    faviconSelection = Favicons.URL + " = ?";
                    faviconSelectionArgs = new String[] { url };
                }

                if (shouldUpdateOrInsert(uri))
                    updated = updateOrInsertFavicon(uri, values, faviconSelection, faviconSelectionArgs);
                else
                    updated = updateExistingFavicon(uri, values, faviconSelection, faviconSelectionArgs);

                break;
            }

            case THUMBNAILS: {
                debug("Update on THUMBNAILS: " + uri);

                String url = values.getAsString(Thumbnails.URL);

                // if no URL is provided, update all of the entries
                if (TextUtils.isEmpty(values.getAsString(Thumbnails.URL)))
                    updated = updateExistingThumbnail(uri, values, null, null);
                else if (shouldUpdateOrInsert(uri))
                    updated = updateOrInsertThumbnail(uri, values, Thumbnails.URL + " = ?",
                                                      new String[] { url });
                else
                    updated = updateExistingThumbnail(uri, values, Thumbnails.URL + " = ?",
                                                      new String[] { url });

                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown update URI " + uri);
        }

        debug("Updated " + updated + " rows for URI: " + uri);

        return updated;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        SQLiteDatabase db = getReadableDatabase(uri);
        final int match = URI_MATCHER.match(uri);

        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        String limit = uri.getQueryParameter(BrowserContract.PARAM_LIMIT);
        String groupBy = null;

        switch (match) {
            case BOOKMARKS_FOLDER_ID:
            case BOOKMARKS_ID:
            case BOOKMARKS: {
                debug("Query is on bookmarks: " + uri);

                if (match == BOOKMARKS_ID) {
                    selection = DBUtils.concatenateWhere(selection, Bookmarks._ID + " = ?");
                    selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                } else if (match == BOOKMARKS_FOLDER_ID) {
                    selection = DBUtils.concatenateWhere(selection, Bookmarks.PARENT + " = ?");
                    selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                            new String[] { Long.toString(ContentUris.parseId(uri)) });
                }

                if (!shouldShowDeleted(uri))
                    selection = DBUtils.concatenateWhere(Bookmarks.IS_DELETED + " = 0", selection);

                if (TextUtils.isEmpty(sortOrder)) {
                    sortOrder = DEFAULT_BOOKMARKS_SORT_ORDER;
                } else {
                    debug("Using sort order " + sortOrder + ".");
                }

                qb.setProjectionMap(BOOKMARKS_PROJECTION_MAP);

                if (hasFaviconsInProjection(projection))
                    qb.setTables(VIEW_BOOKMARKS_WITH_FAVICONS);
                else
                    qb.setTables(TABLE_BOOKMARKS);

                break;
            }

            case HISTORY_ID:
                selection = DBUtils.concatenateWhere(selection, History._ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case HISTORY: {
                debug("Query is on history: " + uri);

                if (!shouldShowDeleted(uri))
                    selection = DBUtils.concatenateWhere(History.IS_DELETED + " = 0", selection);

                if (TextUtils.isEmpty(sortOrder))
                    sortOrder = DEFAULT_HISTORY_SORT_ORDER;

                qb.setProjectionMap(HISTORY_PROJECTION_MAP);

                if (hasFaviconsInProjection(projection))
                    qb.setTables(VIEW_HISTORY_WITH_FAVICONS);
                else
                    qb.setTables(TABLE_HISTORY);

                break;
            }

            case FAVICON_ID:
                selection = DBUtils.concatenateWhere(selection, Favicons._ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case FAVICONS: {
                debug("Query is on favicons: " + uri);

                qb.setProjectionMap(FAVICONS_PROJECTION_MAP);
                qb.setTables(TABLE_FAVICONS);

                break;
            }

            case THUMBNAIL_ID:
                selection = DBUtils.concatenateWhere(selection, Thumbnails._ID + " = ?");
                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { Long.toString(ContentUris.parseId(uri)) });
                // fall through
            case THUMBNAILS: {
                debug("Query is on thumbnails: " + uri);

                qb.setProjectionMap(THUMBNAILS_PROJECTION_MAP);
                qb.setTables(TABLE_THUMBNAILS);

                break;
            }

            case SCHEMA: {
                debug("Query is on schema.");
                MatrixCursor schemaCursor = new MatrixCursor(new String[] { Schema.VERSION });
                schemaCursor.newRow().add(DATABASE_VERSION);

                return schemaCursor;
            }

            case COMBINED: {
                debug("Query is on combined: " + uri);

                if (TextUtils.isEmpty(sortOrder))
                    sortOrder = DEFAULT_HISTORY_SORT_ORDER;

                // This will avoid duplicate entries in the awesomebar
                // results when a history entry has multiple bookmarks.
                groupBy = Combined.URL;

                qb.setProjectionMap(COMBINED_PROJECTION_MAP);

                if (hasFaviconsInProjection(projection))
                    qb.setTables(VIEW_COMBINED_WITH_FAVICONS);
                else
                    qb.setTables(VIEW_COMBINED);

                break;
            }

            case SEARCH_SUGGEST: {
                debug("Query is on search suggest: " + uri);
                selection = DBUtils.concatenateWhere(selection, "(" + Combined.URL + " LIKE ? OR " +
                                                                      Combined.TITLE + " LIKE ?)");

                String keyword = uri.getLastPathSegment();
                if (keyword == null)
                    keyword = "";

                selectionArgs = DBUtils.appendSelectionArgs(selectionArgs,
                        new String[] { "%" + keyword + "%",
                                       "%" + keyword + "%" });

                if (TextUtils.isEmpty(sortOrder))
                    sortOrder = DEFAULT_HISTORY_SORT_ORDER;

                qb.setProjectionMap(SEARCH_SUGGEST_PROJECTION_MAP);
                qb.setTables(VIEW_COMBINED_WITH_FAVICONS);

                break;
            }

            default:
                throw new UnsupportedOperationException("Unknown query URI " + uri);
        }

        trace("Running built query.");
        Cursor cursor = qb.query(db, projection, selection, selectionArgs, groupBy,
                null, sortOrder, limit);
        cursor.setNotificationUri(getContext().getContentResolver(),
                BrowserContract.AUTHORITY_URI);

        return cursor;
    }

    int getUrlCount(SQLiteDatabase db, String table, String url) {
        Cursor c = db.query(table, new String[] { "COUNT(*)" },
                URLColumns.URL + " = ?", new String[] { url }, null, null,
                null);

        int count = 0;

        try {
            if (c.moveToFirst())
                count = c.getInt(0);
        } finally {
            c.close();
        }

        return count;
    }

    /**
     * Update the positions of bookmarks in batches.
     *
     * @see #updateBookmarkPositionsInTransaction(SQLiteDatabase, String[], int, int)
     */
    int updateBookmarkPositions(Uri uri, String[] guids) {
        if (guids == null)
            return 0;

        int guidsCount = guids.length;
        if (guidsCount == 0)
            return 0;

        final SQLiteDatabase db = getWritableDatabase(uri);
        int offset = 0;
        int updated = 0;

        db.beginTransaction();

        while (offset < guidsCount) {
            try {
                updated += updateBookmarkPositionsInTransaction(db, guids, offset,
                                                                MAX_POSITION_UPDATES_PER_QUERY);
            } catch (SQLException e) {
                Log.e(LOGTAG, "Got SQLite exception updating bookmark positions at offset " + offset, e);

                // Need to restart the transaction.
                // The only way a caller knows that anything failed is that the
                // returned update count will be smaller than the requested
                // number of records.
                db.setTransactionSuccessful();
                db.endTransaction();

                db.beginTransaction();
            }

            offset += MAX_POSITION_UPDATES_PER_QUERY;
        }

        db.setTransactionSuccessful();
        db.endTransaction();

        return updated;
    }

    /**
     * Construct and execute an update expression that will modify the positions
     * of records in-place.
     */
    int updateBookmarkPositionsInTransaction(final SQLiteDatabase db, final String[] guids,
                                             final int offset, final int max) {
        int guidsCount = guids.length;
        int processCount = Math.min(max, guidsCount - offset);

        // Each must appear twice: once in a CASE, and once in the IN clause.
        String[] args = new String[processCount * 2];
        System.arraycopy(guids, offset, args, 0, processCount);
        System.arraycopy(guids, offset, args, processCount, processCount);

        StringBuilder b = new StringBuilder("UPDATE " + TABLE_BOOKMARKS +
                                            " SET " + Bookmarks.POSITION +
                                            " = CASE guid");

        // Build the CASE statement body for GUID/index pairs from offset up to
        // the computed limit.
        final int end = offset + processCount;
        int i = offset;
        for (; i < end; ++i) {
            if (guids[i] == null) {
                // We don't want to issue the query if not every GUID is specified.
                debug("updateBookmarkPositions called with null GUID at index " + i);
                return 0;
            }
            b.append(" WHEN ? THEN " + i);
        }

        b.append(" END WHERE " + Bookmarks.GUID + " IN (");
        i = 1;
        while (i++ < processCount) {
            b.append("?, ");
        }
        b.append("?)");
        db.execSQL(b.toString(), args);

        // We can't easily get a modified count without calling something like changes().
        return processCount;
    }

    /**
     * Construct an update expression that will modify the parents of any records
     * that match.
     */
    int updateBookmarkParents(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        trace("Updating bookmark parents of " + selection + " (" + selectionArgs[0] + ")");
        String where = Bookmarks._ID + " IN (" +
                       " SELECT DISTINCT " + Bookmarks.PARENT +
                       " FROM " + TABLE_BOOKMARKS +
                       " WHERE " + selection + " )";
        return getWritableDatabase(uri).update(TABLE_BOOKMARKS, values, where, selectionArgs);
    }

    long insertBookmark(Uri uri, ContentValues values) {
        // Generate values if not specified. Don't overwrite
        // if specified by caller.
        long now = System.currentTimeMillis();
        if (!values.containsKey(Bookmarks.DATE_CREATED)) {
            values.put(Bookmarks.DATE_CREATED, now);
        }

        if (!values.containsKey(Bookmarks.DATE_MODIFIED)) {
            values.put(Bookmarks.DATE_MODIFIED, now);
        }

        if (!values.containsKey(Bookmarks.GUID)) {
            values.put(Bookmarks.GUID, Utils.generateGuid());
        }

        if (!values.containsKey(Bookmarks.POSITION)) {
            debug("Inserting bookmark with no position for URI");
            values.put(Bookmarks.POSITION,
                       Long.toString(BrowserContract.Bookmarks.DEFAULT_POSITION));
        }

        String url = values.getAsString(Bookmarks.URL);
        Integer type = values.getAsInteger(Bookmarks.TYPE);

        debug("Inserting bookmark in database with URL: " + url);
        final SQLiteDatabase db = getWritableDatabase(uri);
        return db.insertOrThrow(TABLE_BOOKMARKS, Bookmarks.TITLE, values);
    }


    int updateOrInsertBookmark(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        int updated = updateBookmarks(uri, values, selection, selectionArgs);
        if (updated > 0)
            return updated;

        if (0 <= insertBookmark(uri, values)) {
            // We 'updated' one row.
            return 1;
        }

        // If something went wrong, then we updated zero rows.
        return 0;
    }

    int updateBookmarks(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Updating bookmarks on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        final String[] bookmarksProjection = new String[] {
                Bookmarks._ID, // 0
                Bookmarks.URL, // 1
        };

        trace("Quering bookmarks to update on URI: " + uri);

        Cursor cursor = db.query(TABLE_BOOKMARKS, bookmarksProjection,
                selection, selectionArgs, null, null, null);

        try {
            if (!values.containsKey(Bookmarks.DATE_MODIFIED))
                values.put(Bookmarks.DATE_MODIFIED, System.currentTimeMillis());

            boolean updatingUrl = values.containsKey(Bookmarks.URL);
            String url = null;

            if (updatingUrl)
                url = values.getAsString(Bookmarks.URL);

            while (cursor.moveToNext()) {
                long id = cursor.getLong(0);

                trace("Updating bookmark with ID: " + id);

                updated += db.update(TABLE_BOOKMARKS, values, "_id = ?",
                        new String[] { Long.toString(id) });
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }

        return updated;
    }

    long insertHistory(Uri uri, ContentValues values) {
        final SQLiteDatabase db = getWritableDatabase(uri);

        long now = System.currentTimeMillis();
        values.put(History.DATE_CREATED, now);
        values.put(History.DATE_MODIFIED, now);

        // Generate GUID for new history entry. Don't override specified GUIDs.
        if (!values.containsKey(History.GUID)) {
          values.put(History.GUID, Utils.generateGuid());
        }

        String url = values.getAsString(History.URL);

        debug("Inserting history in database with URL: " + url);
        return db.insertOrThrow(TABLE_HISTORY, History.VISITS, values);
    }

    int updateOrInsertHistory(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        int updated = updateHistory(uri, values, selection, selectionArgs);
        if (updated > 0)
            return updated;

        // Insert a new entry if necessary
        if (!values.containsKey(History.VISITS))
            values.put(History.VISITS, 1);
        if (!values.containsKey(History.TITLE))
            values.put(History.TITLE, values.getAsString(History.URL));

        if (0 <= insertHistory(uri, values)) {
            return 1;
        }

        return 0;
    }

    int updateHistory(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        trace("Updating history on URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);
        int updated = 0;

        final String[] historyProjection = new String[] {
            History._ID,   // 0
            History.URL,   // 1
            History.VISITS // 2
        };

        Cursor cursor = db.query(TABLE_HISTORY, historyProjection, selection,
                selectionArgs, null, null, null);

        try {
            if (!values.containsKey(Bookmarks.DATE_MODIFIED)) {
                values.put(Bookmarks.DATE_MODIFIED,  System.currentTimeMillis());
            }

            boolean updatingUrl = values.containsKey(History.URL);
            String url = null;

            if (updatingUrl)
                url = values.getAsString(History.URL);

            while (cursor.moveToNext()) {
                long id = cursor.getLong(0);

                trace("Updating history entry with ID: " + id);

                if (shouldIncrementVisits(uri)) {
                    long existing = cursor.getLong(2);
                    Long additional = values.getAsLong(History.VISITS);

                    // Increment visit count by a specified amount, or default to increment by 1
                    values.put(History.VISITS, existing + ((additional != null) ? additional.longValue() : 1));
                }

                updated += db.update(TABLE_HISTORY, values, "_id = ?",
                        new String[] { Long.toString(id) });
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }

        return updated;
    }

    private void updateFaviconIdsForUrl(SQLiteDatabase db, String pageUrl, Long faviconId) {
        ContentValues updateValues = new ContentValues(1);
        updateValues.put(FaviconColumns.FAVICON_ID, faviconId);
        db.update(TABLE_HISTORY,
                  updateValues,
                  History.URL + " = ?",
                  new String[] { pageUrl });
        db.update(TABLE_BOOKMARKS,
                  updateValues,
                  Bookmarks.URL + " = ?",
                  new String[] { pageUrl });
    }

    long insertFavicon(Uri uri, ContentValues values) {
        return insertFavicon(getWritableDatabase(uri), values);
    }

    long insertFavicon(SQLiteDatabase db, ContentValues values) {
        String faviconUrl = values.getAsString(Favicons.URL);
        String pageUrl = null;
        long faviconId;

        trace("Inserting favicon for URL: " + faviconUrl);

        stripEmptyByteArray(values, Favicons.DATA);

        // Extract the page URL from the ContentValues
        if (values.containsKey(Favicons.PAGE_URL)) {
            pageUrl = values.getAsString(Favicons.PAGE_URL);
            values.remove(Favicons.PAGE_URL);
        }

        // If no URL is provided, insert using the default one.
        if (TextUtils.isEmpty(faviconUrl) && !TextUtils.isEmpty(pageUrl)) {
            values.put(Favicons.URL, org.mozilla.gecko.favicons.Favicons.guessDefaultFaviconURL(pageUrl));
        }

        long now = System.currentTimeMillis();
        values.put(Favicons.DATE_CREATED, now);
        values.put(Favicons.DATE_MODIFIED, now);
        faviconId = db.insertOrThrow(TABLE_FAVICONS, null, values);

        if (pageUrl != null) {
            updateFaviconIdsForUrl(db, pageUrl, faviconId);
        }

        return faviconId;
    }

    int updateOrInsertFavicon(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateFavicon(uri, values, selection, selectionArgs,
                true /* insert if needed */);
    }

    int updateExistingFavicon(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateFavicon(uri, values, selection, selectionArgs,
                false /* only update, no insert */);
    }

    int updateFavicon(Uri uri, ContentValues values, String selection,
            String[] selectionArgs, boolean insertIfNeeded) {
        String faviconUrl = values.getAsString(Favicons.URL);
        String pageUrl = null;
        int updated = 0;
        final SQLiteDatabase db = getWritableDatabase(uri);
        Cursor cursor = null;
        Long faviconId = null;
        long now = System.currentTimeMillis();

        trace("Updating favicon for URL: " + faviconUrl);

        stripEmptyByteArray(values, Favicons.DATA);

        // Extract the page URL from the ContentValues
        if (values.containsKey(Favicons.PAGE_URL)) {
            pageUrl = values.getAsString(Favicons.PAGE_URL);
            values.remove(Favicons.PAGE_URL);
        }

        values.put(Favicons.DATE_MODIFIED, now);

        // If there's no favicon URL given and we're inserting if needed, skip
        // the update and only do an insert (otherwise all rows would be
        // updated)
        if (!(insertIfNeeded && (faviconUrl == null))) {
            updated = db.update(TABLE_FAVICONS, values, selection, selectionArgs);
        }

        if (updated > 0) {
            if ((faviconUrl != null) && (pageUrl != null)) {
                try {
                    cursor = db.query(TABLE_FAVICONS,
                                      new String[] { Favicons._ID },
                                      Favicons.URL + " = ?",
                                      new String[] { faviconUrl },
                                      null, null, null);
                    if (cursor.moveToFirst()) {
                        faviconId = cursor.getLong(cursor.getColumnIndexOrThrow(Favicons._ID));
                    }
                } finally {
                    if (cursor != null)
                        cursor.close();
                }
            }
        } else if (insertIfNeeded) {
            values.put(Favicons.DATE_CREATED, now);

            trace("No update, inserting favicon for URL: " + faviconUrl);
            faviconId = db.insert(TABLE_FAVICONS, null, values);
            updated = 1;
        }

        if (pageUrl != null) {
            updateFaviconIdsForUrl(db, pageUrl, faviconId);
        }

        return updated;
    }

    long insertThumbnail(Uri uri, ContentValues values) {
        String url = values.getAsString(Thumbnails.URL);
        final SQLiteDatabase db = getWritableDatabase(uri);

        trace("Inserting thumbnail for URL: " + url);

        stripEmptyByteArray(values, Thumbnails.DATA);

        return db.insertOrThrow(TABLE_THUMBNAILS, null, values);
    }

    int updateOrInsertThumbnail(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateThumbnail(uri, values, selection, selectionArgs,
                true /* insert if needed */);
    }

    int updateExistingThumbnail(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return updateThumbnail(uri, values, selection, selectionArgs,
                false /* only update, no insert */);
    }

    int updateThumbnail(Uri uri, ContentValues values, String selection,
            String[] selectionArgs, boolean insertIfNeeded) {
        String url = values.getAsString(Thumbnails.URL);
        int updated = 0;
        final SQLiteDatabase db = getWritableDatabase(uri);

        stripEmptyByteArray(values, Thumbnails.DATA);

        trace("Updating thumbnail for URL: " + url);

        updated = db.update(TABLE_THUMBNAILS, values, selection, selectionArgs);

        if (updated == 0 && insertIfNeeded) {
            trace("No update, inserting thumbnail for URL: " + url);
            db.insert(TABLE_THUMBNAILS, null, values);
            updated = 1;
        }

        return updated;
    }

    /**
     * Verifies that 0-byte arrays aren't added as favicon or thumbnail data.
     * @param values        ContentValues of query
     * @param columnName    Name of data column to verify
     */
    private void stripEmptyByteArray(ContentValues values, String columnName) {
        if (values.containsKey(columnName)) {
            byte[] data = values.getAsByteArray(columnName);
            if (data == null || data.length == 0) {
                Log.w(LOGTAG, "Tried to insert an empty or non-byte-array image. Ignoring.");
                values.putNull(columnName);
            }
        }
    }

    int deleteHistory(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting history entry for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        if (isCallerSync(uri)) {
            return db.delete(TABLE_HISTORY, selection, selectionArgs);
        }

        debug("Marking history entry as deleted for URI: " + uri);

        ContentValues values = new ContentValues();
        values.put(History.IS_DELETED, 1);

        // Wipe sensitive data.
        values.putNull(History.TITLE);
        values.put(History.URL, "");          // Column is NOT NULL.
        values.put(History.DATE_CREATED, 0);
        values.put(History.DATE_LAST_VISITED, 0);
        values.put(History.VISITS, 0);
        values.put(History.DATE_MODIFIED, System.currentTimeMillis());

        cleanupSomeDeletedRecords(uri, History.CONTENT_URI, TABLE_HISTORY);
        return db.update(TABLE_HISTORY, values, selection, selectionArgs);
    }

    int deleteBookmarks(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting bookmarks for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        if (isCallerSync(uri)) {
            return db.delete(TABLE_BOOKMARKS, selection, selectionArgs);
        }

        debug("Marking bookmarks as deleted for URI: " + uri);

        ContentValues values = new ContentValues();
        values.put(Bookmarks.IS_DELETED, 1);

        cleanupSomeDeletedRecords(uri, Bookmarks.CONTENT_URI, TABLE_BOOKMARKS);
        return updateBookmarks(uri, values, selection, selectionArgs);
    }

    int deleteFavicons(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting favicons for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        return db.delete(TABLE_FAVICONS, selection, selectionArgs);
    }

    int deleteThumbnails(Uri uri, String selection, String[] selectionArgs) {
        debug("Deleting thumbnails for URI: " + uri);

        final SQLiteDatabase db = getWritableDatabase(uri);

        return db.delete(TABLE_THUMBNAILS, selection, selectionArgs);
    }

    int deleteUnusedImages(Uri uri) {
        debug("Deleting all unused favicons and thumbnails for URI: " + uri);

        String faviconSelection = Favicons._ID + " NOT IN "
                + "(SELECT " + History.FAVICON_ID
                + " FROM " + TABLE_HISTORY
                + " WHERE " + History.IS_DELETED + " = 0"
                + " AND " + History.FAVICON_ID + " IS NOT NULL"
                + " UNION ALL SELECT " + Bookmarks.FAVICON_ID
                + " FROM " + TABLE_BOOKMARKS
                + " WHERE " + Bookmarks.IS_DELETED + " = 0"
                + " AND " + Bookmarks.FAVICON_ID + " IS NOT NULL)";

        String thumbnailSelection = Thumbnails.URL + " NOT IN "
                + "(SELECT " + History.URL
                + " FROM " + TABLE_HISTORY
                + " WHERE " + History.IS_DELETED + " = 0"
                + " AND " + History.URL + " IS NOT NULL"
                + " UNION ALL SELECT " + Bookmarks.URL
                + " FROM " + TABLE_BOOKMARKS
                + " WHERE " + Bookmarks.IS_DELETED + " = 0"
                + " AND " + Bookmarks.URL + " IS NOT NULL)";

        return deleteFavicons(uri, faviconSelection, null) +
               deleteThumbnails(uri, thumbnailSelection, null);
    }

    @Override
    public ContentProviderResult[] applyBatch (ArrayList<ContentProviderOperation> operations)
        throws OperationApplicationException {
        final int numOperations = operations.size();
        final ContentProviderResult[] results = new ContentProviderResult[numOperations];
        boolean failures = false;
        SQLiteDatabase db = null;

        if (numOperations >= 1) {
            // We only have 1 database for all Uri's that we can get
            db = getWritableDatabase(operations.get(0).getUri());
        } else {
            // The original Android implementation returns a zero-length
            // array in this case, we do the same.
            return results;
        }

        // Note that the apply() call may cause us to generate
        // additional transactions for the invidual operations.
        // But Android's wrapper for SQLite supports nested transactions,
        // so this will do the right thing.
        db.beginTransaction();

        for (int i = 0; i < numOperations; i++) {
            try {
                results[i] = operations.get(i).apply(this, results, i);
            } catch (SQLException e) {
                Log.w(LOGTAG, "SQLite Exception during applyBatch: ", e);
                // The Android API makes it implementation-defined whether
                // the failure of a single operation makes all others abort
                // or not. For our use cases, best-effort operation makes
                // more sense. Rolling back and forcing the caller to retry
                // after it figures out what went wrong isn't very convenient
                // anyway.
                // Signal failed operation back, so the caller knows what
                // went through and what didn't.
                results[i] = new ContentProviderResult(0);
                failures = true;
                // http://www.sqlite.org/lang_conflict.html
                // Note that we need a new transaction, subsequent operations
                // on this one will fail (we're in ABORT by default, which
                // isn't IGNORE). We still need to set it as successful to let
                // everything before the failed op go through.
                // We can't set conflict resolution on API level < 8, and even
                // above 8 it requires splitting the call per operation
                // (insert/update/delete).
                db.setTransactionSuccessful();
                db.endTransaction();
                db.beginTransaction();
            } catch (OperationApplicationException e) {
                // Repeat of above.
                results[i] = new ContentProviderResult(0);
                failures = true;
                db.setTransactionSuccessful();
                db.endTransaction();
                db.beginTransaction();
            }
        }

        trace("Flushing DB applyBatch...");
        db.setTransactionSuccessful();
        db.endTransaction();

        if (failures) {
            throw new OperationApplicationException();
        }

        return results;
    }

    @Override
    public int bulkInsert(Uri uri, ContentValues[] values) {
        if (values == null)
            return 0;

        int numValues = values.length;
        int successes = 0;

        final SQLiteDatabase db = getWritableDatabase(uri);

        db.beginTransaction();

        try {
            for (int i = 0; i < numValues; i++) {
                insertInTransaction(uri, values[i]);
                successes++;
            }
            trace("Flushing DB bulkinsert...");
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }

        if (successes > 0)
            mContext.getContentResolver().notifyChange(uri, null);

        return successes;
    }
}

/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.db.BrowserDB;

import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.util.Log;

/**
 * Encapsulates the implementation of the search cursor loader.
 */
class SearchLoader {
    public static final String LOGTAG = "GeckoSearchLoader";

    private static final String KEY_SEARCH_TERM = "search_term";

    private SearchLoader() {
    }

    public static Loader<Cursor> createInstance(Context context, Bundle args) {
        if (args != null) {
            final String searchTerm = args.getString(KEY_SEARCH_TERM);
            return new SearchCursorLoader(context, searchTerm);
        } else {
            return new SearchCursorLoader(context, "");
        }
    }

    private static Bundle createArgs(String searchTerm) {
        Bundle args = new Bundle();
        args.putString(SearchLoader.KEY_SEARCH_TERM, searchTerm);

        return args;
    }

    public static void init(LoaderManager manager, int loaderId,
                               LoaderCallbacks<Cursor> callbacks, String searchTerm) {
        final Bundle args = createArgs(searchTerm);
        manager.initLoader(loaderId, args, callbacks);
    }

    public static void restart(LoaderManager manager, int loaderId,
                               LoaderCallbacks<Cursor> callbacks, String searchTerm) {
        final Bundle args = createArgs(searchTerm);
        manager.restartLoader(loaderId, args, callbacks);
    }

    public static class SearchCursorLoader extends SimpleCursorLoader {
        private static final String TELEMETRY_HISTOGRAM_LOAD_CURSOR = "FENNEC_SEARCH_LOADER_TIME_MS";

        // Max number of search results
        private static final int SEARCH_LIMIT = 100;

        // The target search term associated with the loader
        private final String mSearchTerm;

        public SearchCursorLoader(Context context, String searchTerm) {
            super(context);
            mSearchTerm = searchTerm;
        }

        @Override
        public Cursor loadCursor() {
            final long start = SystemClock.uptimeMillis();
            final Cursor cursor = BrowserDB.filter(getContext().getContentResolver(), mSearchTerm, SEARCH_LIMIT);
            final long end = SystemClock.uptimeMillis();
            final long took = end - start;
            Telemetry.HistogramAdd(TELEMETRY_HISTOGRAM_LOAD_CURSOR, (int) Math.min(took, Integer.MAX_VALUE));
            return cursor;
        }

        public String getSearchTerm() {
            return mSearchTerm;
        }
    }

}

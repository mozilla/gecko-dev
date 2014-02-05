/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.HomeItems;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.PanelLayout.DatasetHandler;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import java.util.EnumSet;

/**
 * Fragment that displays dynamic content specified by a {@code PanelConfig}.
 * The {@code DynamicPanel} UI is built based on the given {@code LayoutType}
 * and its associated list of {@code ViewConfig}.
 *
 * {@code DynamicPanel} manages all necessary Loaders to load panel datasets
 * from their respective content providers. Each panel dataset has its own
 * associated Loader. This is enforced by defining the Loader IDs based on
 * their associated dataset IDs.
 *
 * The {@code PanelLayout} can make load and reset requests on datasets via
 * the provided {@code DatasetHandler}. This way it doesn't need to know the
 * details of how datasets are loaded and reset. Each time a dataset is
 * requested, {@code DynamicPanel} restarts a Loader with the respective ID (see
 * {@code PanelDatasetHandler}).
 *
 * See {@code PanelLayout} for more details on how {@code DynamicPanel}
 * receives dataset requests and delivers them back to the {@code PanelLayout}.
 */
public class DynamicPanel extends HomeFragment {
    private static final String LOGTAG = "GeckoDynamicPanel";

    // Dataset ID to be used by the loader
    private static final String DATASET_ID = "dataset_id";

    // The panel layout associated with this panel
    private PanelLayout mLayout;

    // The configuration associated with this panel
    private PanelConfig mPanelConfig;

    // Callbacks used for the loader
    private PanelLoaderCallbacks mLoaderCallbacks;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnUrlOpenListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUrlOpenListener = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final Bundle args = getArguments();
        if (args != null) {
            mPanelConfig = (PanelConfig) args.getParcelable(HomePager.PANEL_CONFIG_ARG);
        }

        if (mPanelConfig == null) {
            throw new IllegalStateException("Can't create a DynamicPanel without a PanelConfig");
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        switch(mPanelConfig.getLayoutType()) {
            case FRAME:
                final PanelDatasetHandler datasetHandler = new PanelDatasetHandler();
                mLayout = new FramePanelLayout(getActivity(), mPanelConfig, datasetHandler, mUrlOpenListener);
                break;

            default:
                throw new IllegalStateException("Unrecognized layout type in DynamicPanel");
        }

        Log.d(LOGTAG, "Created layout of type: " + mPanelConfig.getLayoutType());

        return mLayout;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mLayout = null;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Detach and reattach the fragment as the layout changes.
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Create callbacks before the initial loader is started.
        mLoaderCallbacks = new PanelLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    protected void load() {
        Log.d(LOGTAG, "Loading layout");
        mLayout.load();
    }

    private static int generateLoaderId(String datasetId) {
        return datasetId.hashCode();
    }

    /**
     * Used by the PanelLayout to make load and reset requests to
     * the holding fragment.
     */
    private class PanelDatasetHandler implements DatasetHandler {
        @Override
        public void requestDataset(String datasetId) {
            Log.d(LOGTAG, "Requesting dataset: " + datasetId);

            // Ignore dataset requests while the fragment is not
            // allowed to load its content.
            if (!getCanLoadHint()) {
                return;
            }

            final Bundle bundle = new Bundle();
            bundle.putString(DATASET_ID, datasetId);

            // Ensure one loader per dataset
            final int loaderId = generateLoaderId(datasetId);
            getLoaderManager().restartLoader(loaderId, bundle, mLoaderCallbacks);
        }

        @Override
        public void resetDataset(String datasetId) {
            Log.d(LOGTAG, "Resetting dataset: " + datasetId);

            final LoaderManager lm = getLoaderManager();
            final int loaderId = generateLoaderId(datasetId);

            // Release any resources associated with the dataset if
            // it's currently loaded in memory.
            final Loader<?> datasetLoader = lm.getLoader(loaderId);
            if (datasetLoader != null) {
                datasetLoader.reset();
            }
        }
    }

    /**
     * Cursor loader for the panel datasets.
     */
    private static class PanelDatasetLoader extends SimpleCursorLoader {
        private final String mDatasetId;

        public PanelDatasetLoader(Context context, String datasetId) {
            super(context);
            mDatasetId = datasetId;
        }

        public String getDatasetId() {
            return mDatasetId;
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();

            final String selection = HomeItems.DATASET_ID + " = ?";
            final String[] selectionArgs = new String[] { mDatasetId };

            // XXX: You can use CONTENT_FAKE_URI for development to pull items from fake_home_items.json.
            return cr.query(HomeItems.CONTENT_URI, null, selection, selectionArgs, null);
        }
    }

    /**
     * LoaderCallbacks implementation that interacts with the LoaderManager.
     */
    private class PanelLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            final String datasetId = args.getString(DATASET_ID);

            Log.d(LOGTAG, "Creating loader for dataset: " + datasetId);
            return new PanelDatasetLoader(getActivity(), datasetId);
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor cursor) {
            final PanelDatasetLoader datasetLoader = (PanelDatasetLoader) loader;

            Log.d(LOGTAG, "Finished loader for dataset: " + datasetLoader.getDatasetId());
            mLayout.deliverDataset(datasetLoader.getDatasetId(), cursor);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            final PanelDatasetLoader datasetLoader = (PanelDatasetLoader) loader;
            Log.d(LOGTAG, "Resetting loader for dataset: " + datasetLoader.getDatasetId());
            if (mLayout != null) {
                mLayout.releaseDataset(datasetLoader.getDatasetId());
            }
        }
    }
}

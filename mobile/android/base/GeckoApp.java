/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.GeckoProfileDirectories.NoMozillaDirectoryException;
import org.mozilla.gecko.background.announcements.AnnouncementsBroadcastService;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.FullScreenState;
import org.mozilla.gecko.gfx.Layer;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PluginLayer;
import org.mozilla.gecko.health.HealthRecorder;
import org.mozilla.gecko.health.SessionInformation;
import org.mozilla.gecko.health.StubbedHealthRecorder;
import org.mozilla.gecko.menu.GeckoMenu;
import org.mozilla.gecko.menu.GeckoMenuInflater;
import org.mozilla.gecko.menu.MenuPanel;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.updater.UpdateService;
import org.mozilla.gecko.updater.UpdateServiceHelper;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.FileUtils;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;
import org.mozilla.gecko.webapp.EventListener;
import org.mozilla.gecko.webapp.UninstallListener;
import org.mozilla.gecko.widget.ButtonToast;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.location.Location;
import android.location.LocationListener;
import android.net.Uri;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.PowerManager;
import android.os.StrictMode;
import android.provider.ContactsContract;
import android.provider.MediaStore.Images.Media;
import android.telephony.CellLocation;
import android.telephony.NeighboringCellInfo;
import android.telephony.PhoneStateListener;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.telephony.gsm.GsmCellLocation;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Base64;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AbsoluteLayout;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.RelativeLayout;
import android.widget.SimpleAdapter;
import android.widget.TextView;
import android.widget.Toast;

public abstract class GeckoApp
    extends GeckoActivity
    implements
    ContextGetter,
    GeckoAppShell.GeckoInterface,
    GeckoEventListener,
    GeckoMenu.Callback,
    GeckoMenu.MenuPresenter,
    LocationListener,
    NativeEventListener,
    SensorEventListener,
    Tabs.OnTabsChangedListener
{
    private static final String LOGTAG = "GeckoApp";
    private static final int ONE_DAY_MS = 1000*60*60*24;

    private static enum StartupAction {
        NORMAL,     /* normal application start */
        URL,        /* launched with a passed URL */
        PREFETCH    /* launched with a passed URL that we prefetch */
    }

    public static final String ACTION_ALERT_CALLBACK       = "org.mozilla.gecko.ACTION_ALERT_CALLBACK";
    public static final String ACTION_BOOKMARK             = "org.mozilla.gecko.BOOKMARK";
    public static final String ACTION_DEBUG                = "org.mozilla.gecko.DEBUG";
    public static final String ACTION_LAUNCH_SETTINGS      = "org.mozilla.gecko.SETTINGS";
    public static final String ACTION_LOAD                 = "org.mozilla.gecko.LOAD";
    public static final String ACTION_INIT_PW              = "org.mozilla.gecko.INIT_PW";
    public static final String ACTION_WEBAPP_PREFIX        = "org.mozilla.gecko.WEBAPP";

    public static final String EXTRA_STATE_BUNDLE          = "stateBundle";

    public static final String PREFS_ALLOW_STATE_BUNDLE    = "allowStateBundle";
    public static final String PREFS_OOM_EXCEPTION         = "OOMException";
    public static final String PREFS_VERSION_CODE          = "versionCode";
    public static final String PREFS_WAS_STOPPED           = "wasStopped";
    public static final String PREFS_CRASHED               = "crashed";
    public static final String PREFS_CLEANUP_TEMP_FILES    = "cleanupTempFiles";

    public static final String SAVED_STATE_IN_BACKGROUND   = "inBackground";
    public static final String SAVED_STATE_PRIVATE_SESSION = "privateSession";

    static private final String LOCATION_URL = "https://location.services.mozilla.com/v1/submit";

    // Delay before running one-time "cleanup" tasks that may be needed
    // after a version upgrade.
    private static final int CLEANUP_DEFERRAL_SECONDS = 15;

    protected RelativeLayout mMainLayout;
    protected RelativeLayout mGeckoLayout;
    public View getView() { return mGeckoLayout; }
    private View mCameraView;
    private OrientationEventListener mCameraOrientationEventListener;
    public List<GeckoAppShell.AppStateListener> mAppStateListeners;
    protected MenuPanel mMenuPanel;
    protected Menu mMenu;
    protected GeckoProfile mProfile;
    protected boolean mIsRestoringActivity;

    private ContactService mContactService;
    private PromptService mPromptService;
    private TextSelection mTextSelection;

    protected DoorHangerPopup mDoorHangerPopup;
    protected FormAssistPopup mFormAssistPopup;
    protected ButtonToast mToast;

    protected LayerView mLayerView;
    private AbsoluteLayout mPluginContainer;

    private FullScreenHolder mFullScreenPluginContainer;
    private View mFullScreenPluginView;

    private HashMap<String, PowerManager.WakeLock> mWakeLocks = new HashMap<String, PowerManager.WakeLock>();

    protected boolean mShouldRestore;
    protected boolean mInitialized = false;
    private Telemetry.Timer mJavaUiStartupTimer;
    private Telemetry.Timer mGeckoReadyStartupTimer;

    private String mPrivateBrowsingSession;

    private volatile HealthRecorder mHealthRecorder = null;
    private volatile Locale mLastLocale = null;

    private int mSignalStrenth;
    private PhoneStateListener mPhoneStateListener = null;
    private boolean mShouldReportGeoData;
    private EventListener mWebappEventListener;

    abstract public int getLayout();
    abstract public boolean hasTabsSideBar();
    abstract protected String getDefaultProfileName() throws NoMozillaDirectoryException;

    private static final String RESTARTER_ACTION = "org.mozilla.gecko.restart";
    private static final String RESTARTER_CLASS = "org.mozilla.gecko.Restarter";

    @SuppressWarnings("serial")
    class SessionRestoreException extends Exception {
        public SessionRestoreException(Exception e) {
            super(e);
        }

        public SessionRestoreException(String message) {
            super(message);
        }
    }

    void toggleChrome(final boolean aShow) { }

    void focusChrome() { }

    @Override
    public Context getContext() {
        return this;
    }

    @Override
    public SharedPreferences getSharedPreferences() {
        return GeckoSharedPrefs.forApp(this);
    }

    public Activity getActivity() {
        return this;
    }

    public LocationListener getLocationListener() {
        if (mShouldReportGeoData && mPhoneStateListener == null) {
            mPhoneStateListener = new PhoneStateListener() {
                public void onSignalStrengthsChanged(SignalStrength signalStrength) {
                    setCurrentSignalStrenth(signalStrength);
                }
            };
            TelephonyManager tm = (TelephonyManager)getSystemService(Context.TELEPHONY_SERVICE);
            tm.listen(mPhoneStateListener, PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);
        }
        return this;
    }

    public SensorEventListener getSensorEventListener() {
        return this;
    }

    public View getCameraView() {
        return mCameraView;
    }

    public void addAppStateListener(GeckoAppShell.AppStateListener listener) {
        mAppStateListeners.add(listener);
    }

    public void removeAppStateListener(GeckoAppShell.AppStateListener listener) {
        mAppStateListeners.remove(listener);
    }

    public FormAssistPopup getFormAssistPopup() {
        return mFormAssistPopup;
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, Object data) {
        // When a tab is closed, it is always unselected first.
        // When a tab is unselected, another tab is always selected first.
        switch(msg) {
            case UNSELECTED:
                hidePlugins(tab);
                break;

            case LOCATION_CHANGE:
                // We only care about location change for the selected tab.
                if (!Tabs.getInstance().isSelectedTab(tab))
                    break;
                // Fall through...
            case SELECTED:
                invalidateOptionsMenu();
                if (mFormAssistPopup != null)
                    mFormAssistPopup.hide();
                break;

            case LOADED:
                // Sync up the layer view and the tab if the tab is
                // currently displayed.
                LayerView layerView = mLayerView;
                if (layerView != null && Tabs.getInstance().isSelectedTab(tab))
                    layerView.setBackgroundColor(tab.getBackgroundColor());
                break;

            case DESKTOP_MODE_CHANGE:
                if (Tabs.getInstance().isSelectedTab(tab))
                    invalidateOptionsMenu();
                break;
        }
    }

    public void refreshChrome() { }

    @Override
    public void invalidateOptionsMenu() {
        if (mMenu == null)
            return;

        onPrepareOptionsMenu(mMenu);

        if (Build.VERSION.SDK_INT >= 11)
            super.invalidateOptionsMenu();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        mMenu = menu;

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.gecko_app_menu, mMenu);
        return true;
    }

    @Override
    public MenuInflater getMenuInflater() {
        if (Build.VERSION.SDK_INT >= 11)
            return new GeckoMenuInflater(this);
        else
            return super.getMenuInflater();
    }

    public MenuPanel getMenuPanel() {
        if (mMenuPanel == null) {
            onCreatePanelMenu(Window.FEATURE_OPTIONS_PANEL, null);
            invalidateOptionsMenu();
        }
        return mMenuPanel;
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        return onOptionsItemSelected(item);
    }

    @Override
    public boolean onMenuItemLongClick(MenuItem item) {
        return false;
    }

    @Override
    public void openMenu() {
        openOptionsMenu();
    }

    @Override
    public void showMenu(final View menu) {
        // On devices using the custom menu, focus is cleared from the menu when its tapped.
        // Close and then reshow it to avoid these issues. See bug 794581 and bug 968182.
        closeMenu();

        // Post the reshow code back to the UI thread to avoid some optimizations Android
        // has put in place for menus that hide/show themselves quickly. See bug 985400.
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mMenuPanel.removeAllViews();
                mMenuPanel.addView(menu);
                openOptionsMenu();
            }
        });
    }

    @Override
    public void closeMenu() {
        closeOptionsMenu();
    }

    @Override
    public View onCreatePanelView(int featureId) {
        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenuPanel == null) {
                mMenuPanel = new MenuPanel(this, null);
            } else {
                // Prepare the panel everytime before showing the menu.
                onPreparePanel(featureId, mMenuPanel, mMenu);
            }

            return mMenuPanel; 
        }
  
        return super.onCreatePanelView(featureId);
    }

    @Override
    public boolean onCreatePanelMenu(int featureId, Menu menu) {
        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenuPanel == null) {
                mMenuPanel = (MenuPanel) onCreatePanelView(featureId);
            }

            GeckoMenu gMenu = new GeckoMenu(this, null);
            gMenu.setCallback(this);
            gMenu.setMenuPresenter(this);
            menu = gMenu;
            mMenuPanel.addView(gMenu);

            return onCreateOptionsMenu(menu);
        }

        return super.onCreatePanelMenu(featureId, menu);
    }

    @Override
    public boolean onPreparePanel(int featureId, View view, Menu menu) {
        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL)
            return onPrepareOptionsMenu(menu);

        return super.onPreparePanel(featureId, view, menu);
    }

    @Override
    public boolean onMenuOpened(int featureId, Menu menu) {
        // exit full-screen mode whenever the menu is opened
        if (mLayerView != null && mLayerView.isFullScreen()) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FullScreen:Exit", null));
        }

        if (Build.VERSION.SDK_INT >= 11 && featureId == Window.FEATURE_OPTIONS_PANEL) {
            if (mMenu == null) {
                // getMenuPanel() will force the creation of the menu as well
                MenuPanel panel = getMenuPanel();
                onPreparePanel(featureId, panel, mMenu);
            }

            // Scroll custom menu to the top
            if (mMenuPanel != null)
                mMenuPanel.scrollTo(0, 0);

            return true;
        }

        return super.onMenuOpened(featureId, menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.quit) {
            if (GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.GeckoRunning, GeckoThread.LaunchState.GeckoExiting)) {
                GeckoAppShell.notifyGeckoOfEvent(GeckoEvent.createBroadcastEvent("Browser:Quit", null));
            } else {
                GeckoAppShell.systemExit();
            }
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onOptionsMenuClosed(Menu menu) {
        if (Build.VERSION.SDK_INT >= 11) {
            mMenuPanel.removeAllViews();
            mMenuPanel.addView((GeckoMenu) mMenu);
        }
    }
 
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        // Handle hardware menu key presses separately so that we can show a custom menu in some cases.
        if (keyCode == KeyEvent.KEYCODE_MENU) {
            openOptionsMenu();
            return true;
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);

        outState.putBoolean(SAVED_STATE_IN_BACKGROUND, isApplicationInBackground());
        outState.putString(SAVED_STATE_PRIVATE_SESSION, mPrivateBrowsingSession);
    }

    void handleClearHistory() {
        BrowserDB.clearHistory(getContentResolver());
    }

    public void addTab() { }

    public void addPrivateTab() { }

    public void showNormalTabs() { }

    public void showPrivateTabs() { }

    public void hideTabs() { }

    /**
     * Close the tab UI indirectly (not as the result of a direct user
     * action).  This does not force the UI to close; for example in Firefox
     * tablet mode it will remain open unless the user explicitly closes it.
     *
     * @return True if the tab UI was hidden.
     */
    public boolean autoHideTabs() { return false; }

    public boolean areTabsShown() { return false; }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        if ("Accessibility:Ready".equals(event)) {
            GeckoAccessibility.updateAccessibilitySettings(this);

        } else if ("Bookmark:Insert".equals(event)) {
            final String url = message.getString("url");
            final String title = message.getString("title");
            final Context context = this;
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    BrowserDB.addBookmark(getContentResolver(), title, url);
                    ThreadUtils.postToUiThread(new Runnable() {
                        @Override
                        public void run() {
                            Toast.makeText(context, R.string.bookmark_added, Toast.LENGTH_SHORT).show();
                        }
                    });
                }
            });

        } else if ("Contact:Add".equals(event)) {
            final String email = message.optString("email", null);
            final String phone = message.optString("phone", null);
            if (email != null) {
                Uri contactUri = Uri.parse(email);
                Intent i = new Intent(ContactsContract.Intents.SHOW_OR_CREATE_CONTACT, contactUri);
                startActivity(i);
            } else if (phone != null) {
                Uri contactUri = Uri.parse(phone);
                Intent i = new Intent(ContactsContract.Intents.SHOW_OR_CREATE_CONTACT, contactUri);
                startActivity(i);
            } else {
                // something went wrong.
                Log.e(LOGTAG, "Received Contact:Add message with no email nor phone number");
            }

        } else if ("DOMFullScreen:Start".equals(event)) {
            // Local ref to layerView for thread safety
            LayerView layerView = mLayerView;
            if (layerView != null) {
                layerView.setFullScreenState(message.getBoolean("rootElement")
                        ? FullScreenState.ROOT_ELEMENT : FullScreenState.NON_ROOT_ELEMENT);
            }

        } else if ("DOMFullScreen:Stop".equals(event)) {
            // Local ref to layerView for thread safety
            LayerView layerView = mLayerView;
            if (layerView != null) {
                layerView.setFullScreenState(FullScreenState.NONE);
            }

        } else if ("Image:SetAs".equals(event)) {
            String src = message.getString("url");
            setImageAs(src);

        } else if ("Locale:Set".equals(event)) {
            setLocale(message.getString("locale"));

        } else if ("Permissions:Data".equals(event)) {
            String host = message.getString("host");
            final NativeJSObject[] permissions = message.getObjectArray("permissions");
            showSiteSettingsDialog(host, permissions);

        } else if ("PrivateBrowsing:Data".equals(event)) {
            mPrivateBrowsingSession = message.optString("session", null);

        } else if ("Sanitize:ClearHistory".equals(event)) {
            handleClearHistory();

        } else if ("Session:StatePurged".equals(event)) {
            onStatePurged();

        } else if ("Share:Text".equals(event)) {
            String text = message.getString("text");
            GeckoAppShell.openUriExternal(text, "text/plain", "", "", Intent.ACTION_SEND, "");

            // Context: Sharing via chrome list (no explicit session is active)
            Telemetry.sendUIEvent(TelemetryContract.Event.SHARE, TelemetryContract.Method.LIST);

        } else if ("Shortcut:Remove".equals(event)) {
            final String url = message.getString("url");
            final String origin = message.getString("origin");
            final String title = message.getString("title");
            final String type = message.getString("shortcutType");
            GeckoAppShell.removeShortcut(title, url, origin, type);

        } else if ("SystemUI:Visibility".equals(event)) {
            setSystemUiVisible(message.getBoolean("visible"));

        } else if ("Toast:Show".equals(event)) {
            final String msg = message.getString("message");
            final NativeJSObject button = message.optObject("button", null);
            if (button != null) {
                final String label = button.optString("label", "");
                final String icon = button.optString("icon", "");
                final String id = button.optString("id", "");
                showButtonToast(msg, label, icon, id);
            } else {
                final String duration = message.getString("duration");
                showNormalToast(msg, duration);
            }

        } else if ("ToggleChrome:Focus".equals(event)) {
            focusChrome();

        } else if ("ToggleChrome:Hide".equals(event)) {
            toggleChrome(false);

        } else if ("ToggleChrome:Show".equals(event)) {
            toggleChrome(true);

        } else if ("Update:Check".equals(event)) {
            startService(new Intent(
                    UpdateServiceHelper.ACTION_CHECK_FOR_UPDATE, null, this, UpdateService.class));

        } else if ("Update:Download".equals(event)) {
            startService(new Intent(
                    UpdateServiceHelper.ACTION_DOWNLOAD_UPDATE, null, this, UpdateService.class));

        } else if ("Update:Install".equals(event)) {
            startService(new Intent(
                    UpdateServiceHelper.ACTION_APPLY_UPDATE, null, this, UpdateService.class));
        }
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        try {
            if (event.equals("Gecko:DelayedStartup")) {
                ThreadUtils.postToBackgroundThread(new UninstallListener.DelayedStartupTask(this));
            } else if (event.equals("Gecko:Ready")) {
                mGeckoReadyStartupTimer.stop();
                geckoConnected();

                // This method is already running on the background thread, so we
                // know that mHealthRecorder will exist. That doesn't stop us being
                // paranoid.
                // This method is cheap, so don't spawn a new runnable.
                final HealthRecorder rec = mHealthRecorder;
                if (rec != null) {
                  rec.recordGeckoStartupTime(mGeckoReadyStartupTimer.getElapsed());
                }
            } else if ("NativeApp:IsDebuggable".equals(event)) {
                JSONObject ret = new JSONObject();
                ret.put("isDebuggable", getIsDebuggable());
                EventDispatcher.sendResponse(message, ret);
            } else if (event.equals("Accessibility:Event")) {
                GeckoAccessibility.sendAccessibilityEvent(message);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    void onStatePurged() { }

    /**
     * @param permissions
     *        Array of JSON objects to represent site permissions.
     *        Example: { type: "offline-app", setting: "Store Offline Data", value: "Allow" }
     */
    private void showSiteSettingsDialog(final String host, final NativeJSObject[] permissions) {
        final AlertDialog.Builder builder = new AlertDialog.Builder(this);

        View customTitleView = getLayoutInflater().inflate(R.layout.site_setting_title, null);
        ((TextView) customTitleView.findViewById(R.id.title)).setText(R.string.site_settings_title);
        ((TextView) customTitleView.findViewById(R.id.host)).setText(host);
        builder.setCustomTitle(customTitleView);

        // If there are no permissions to clear, show the user a message about that.
        // In the future, we want to disable the menu item if there are no permissions to clear.
        if (permissions.length == 0) {
            builder.setMessage(R.string.site_settings_no_settings);
        } else {

            final ArrayList<HashMap<String, String>> itemList =
                    new ArrayList<HashMap<String, String>>();
            for (final NativeJSObject permObj : permissions) {
                final HashMap<String, String> map = new HashMap<String, String>();
                map.put("setting", permObj.getString("setting"));
                map.put("value", permObj.getString("value"));
                itemList.add(map);
            }

            // setMultiChoiceItems doesn't support using an adapter, so we're creating a hack with
            // setSingleChoiceItems and changing the choiceMode below when we create the dialog
            builder.setSingleChoiceItems(new SimpleAdapter(
                GeckoApp.this,
                itemList,
                R.layout.site_setting_item,
                new String[] { "setting", "value" },
                new int[] { R.id.setting, R.id.value }
                ), -1, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) { }
                });

            builder.setPositiveButton(R.string.site_settings_clear, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int id) {
                    ListView listView = ((AlertDialog) dialog).getListView();
                    SparseBooleanArray checkedItemPositions = listView.getCheckedItemPositions();

                    // An array of the indices of the permissions we want to clear
                    JSONArray permissionsToClear = new JSONArray();
                    for (int i = 0; i < checkedItemPositions.size(); i++)
                        if (checkedItemPositions.get(i))
                            permissionsToClear.put(i);

                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(
                        "Permissions:Clear", permissionsToClear.toString()));
                }
            });
        }

        builder.setNegativeButton(R.string.site_settings_cancel, new DialogInterface.OnClickListener(){
            @Override
            public void onClick(DialogInterface dialog, int id) {
                dialog.cancel();
            }
        });

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Dialog dialog = builder.create();
                dialog.show();

                ListView listView = ((AlertDialog) dialog).getListView();
                if (listView != null) {
                    listView.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
                    int listSize = listView.getAdapter().getCount();
                    for (int i = 0; i < listSize; i++)
                        listView.setItemChecked(i, true);
                }
            }
        });
    }

    public void showToast(final int resId, final int duration) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(GeckoApp.this, resId, duration).show();
            }
        });
    }

    public void showNormalToast(final String message, final String duration) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Toast toast;
                if (duration.equals("long")) {
                    toast = Toast.makeText(GeckoApp.this, message, Toast.LENGTH_LONG);
                } else {
                    toast = Toast.makeText(GeckoApp.this, message, Toast.LENGTH_SHORT);
                }
                toast.show();
            }
        });
    }

    public ButtonToast getButtonToast() {
        if (mToast != null) {
            return mToast;
        }

        ViewStub toastStub = (ViewStub) findViewById(R.id.toast_stub);
        mToast = new ButtonToast(toastStub.inflate());

        return mToast;
    }

    void showButtonToast(final String message, final String buttonText,
                         final String buttonIcon, final String buttonId) {
        BitmapUtils.getDrawable(GeckoApp.this, buttonIcon, new BitmapUtils.BitmapLoader() {
            @Override
            public void onBitmapFound(final Drawable d) {
                getButtonToast().show(false, message, buttonText, d, new ButtonToast.ToastListener() {
                    @Override
                    public void onButtonClicked() {
                        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Toast:Click", buttonId));
                    }

                    @Override
                    public void onToastHidden(ButtonToast.ReasonHidden reason) {
                        if (reason == ButtonToast.ReasonHidden.TIMEOUT) {
                            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Toast:Hidden", buttonId));
                        }
                    }
                });
            }
        });
    }

    private void addFullScreenPluginView(View view) {
        if (mFullScreenPluginView != null) {
            Log.w(LOGTAG, "Already have a fullscreen plugin view");
            return;
        }

        setFullScreen(true);

        view.setWillNotDraw(false);
        if (view instanceof SurfaceView) {
            ((SurfaceView) view).setZOrderOnTop(true);
        }

        mFullScreenPluginContainer = new FullScreenHolder(this);

        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.FILL_PARENT,
                            ViewGroup.LayoutParams.FILL_PARENT,
                            Gravity.CENTER);
        mFullScreenPluginContainer.addView(view, layoutParams);


        FrameLayout decor = (FrameLayout)getWindow().getDecorView();
        decor.addView(mFullScreenPluginContainer, layoutParams);

        mFullScreenPluginView = view;
    }

    public void addPluginView(final View view, final RectF rect, final boolean isFullScreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getSelectedTab();

                if (isFullScreen) {
                    addFullScreenPluginView(view);
                    return;
                }

                PluginLayer layer = (PluginLayer) tab.getPluginLayer(view);
                if (layer == null) {
                    layer = new PluginLayer(view, rect, mLayerView.getRenderer().getMaxTextureSize());
                    tab.addPluginLayer(view, layer);
                } else {
                    layer.reset(rect);
                    layer.setVisible(true);
                }

                mLayerView.addLayer(layer);
            }
        });
    }

    private void removeFullScreenPluginView(View view) {
        if (mFullScreenPluginView == null) {
            Log.w(LOGTAG, "Don't have a fullscreen plugin view");
            return;
        }

        if (mFullScreenPluginView != view) {
            Log.w(LOGTAG, "Passed view is not the current full screen view");
            return;
        }

        mFullScreenPluginContainer.removeView(mFullScreenPluginView);

        // We need do do this on the next iteration in order to avoid
        // a deadlock, see comment below in FullScreenHolder
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mLayerView.showSurface();
            }
        });

        FrameLayout decor = (FrameLayout)getWindow().getDecorView();
        decor.removeView(mFullScreenPluginContainer);

        mFullScreenPluginView = null;

        GeckoScreenOrientation.getInstance().unlock();
        setFullScreen(false);
    }

    public void removePluginView(final View view, final boolean isFullScreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Tabs tabs = Tabs.getInstance();
                Tab tab = tabs.getSelectedTab();

                if (isFullScreen) {
                    removeFullScreenPluginView(view);
                    return;
                }

                PluginLayer layer = (PluginLayer) tab.removePluginLayer(view);
                if (layer != null) {
                    layer.destroy();
                }
            }
        });
    }

    // This method starts downloading an image synchronously and displays the Chooser activity to set the image as wallpaper.
    private void setImageAs(final String aSrc) {
        boolean isDataURI = aSrc.startsWith("data:");
        Bitmap image = null;
        InputStream is = null;
        ByteArrayOutputStream os = null;
        try {
            if (isDataURI) {
                int dataStart = aSrc.indexOf(",");
                byte[] buf = Base64.decode(aSrc.substring(dataStart+1), Base64.DEFAULT);
                image = BitmapUtils.decodeByteArray(buf);
            } else {
                int byteRead;
                byte[] buf = new byte[4192];
                os = new ByteArrayOutputStream();
                URL url = new URL(aSrc);
                is = url.openStream();

                // Cannot read from same stream twice. Also, InputStream from
                // URL does not support reset. So converting to byte array.

                while((byteRead = is.read(buf)) != -1) {
                    os.write(buf, 0, byteRead);
                }
                byte[] imgBuffer = os.toByteArray();
                image = BitmapUtils.decodeByteArray(imgBuffer);
            }
            if (image != null) {
                String path = Media.insertImage(getContentResolver(),image, null, null);
                final Intent intent = new Intent(Intent.ACTION_ATTACH_DATA);
                intent.addCategory(Intent.CATEGORY_DEFAULT);
                intent.setData(Uri.parse(path));

                // Removes the image from storage once the chooser activity ends.
                Intent chooser = Intent.createChooser(intent, getString(R.string.set_image_chooser_title));
                ActivityResultHandler handler = new ActivityResultHandler() {
                    @Override
                    public void onActivityResult (int resultCode, Intent data) {
                        getContentResolver().delete(intent.getData(), null, null);
                    }
                };
                ActivityHandlerHelper.startIntentForActivity(this, chooser, handler);
            } else {
                Toast.makeText((Context) this, R.string.set_image_fail, Toast.LENGTH_SHORT).show();
            }
        } catch(OutOfMemoryError ome) {
            Log.e(LOGTAG, "Out of Memory when converting to byte array", ome);
        } catch(IOException ioe) {
            Log.e(LOGTAG, "I/O Exception while setting wallpaper", ioe);
        } finally {
            if (is != null) {
                try {
                    is.close();
                } catch(IOException ioe) {
                    Log.w(LOGTAG, "I/O Exception while closing stream", ioe);
                }
            }
            if (os != null) {
                try {
                    os.close();
                } catch(IOException ioe) {
                    Log.w(LOGTAG, "I/O Exception while closing stream", ioe);
                }
            }
        }
    }

    private int getBitmapSampleSize(BitmapFactory.Options options, int idealWidth, int idealHeight) {
        int width = options.outWidth;
        int height = options.outHeight;
        int inSampleSize = 1;
        if (height > idealHeight || width > idealWidth) {
            if (width > height) {
                inSampleSize = Math.round((float)height / (float)idealHeight);
            } else {
                inSampleSize = Math.round((float)width / (float)idealWidth);
            }
        }
        return inSampleSize;
    }

    private void hidePluginLayer(Layer layer) {
        LayerView layerView = mLayerView;
        layerView.removeLayer(layer);
        layerView.requestRender();
    }

    private void showPluginLayer(Layer layer) {
        LayerView layerView = mLayerView;
        layerView.addLayer(layer);
        layerView.requestRender();
    }

    public void requestRender() {
        mLayerView.requestRender();
    }
    
    public void hidePlugins(Tab tab) {
        for (Layer layer : tab.getPluginLayers()) {
            if (layer instanceof PluginLayer) {
                ((PluginLayer) layer).setVisible(false);
            }

            hidePluginLayer(layer);
        }

        requestRender();
    }

    public void showPlugins() {
        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();

        showPlugins(tab);
    }

    public void showPlugins(Tab tab) {
        for (Layer layer : tab.getPluginLayers()) {
            showPluginLayer(layer);

            if (layer instanceof PluginLayer) {
                ((PluginLayer) layer).setVisible(true);
            }
        }

        requestRender();
    }

    public void setFullScreen(final boolean fullscreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                // Hide/show the system notification bar
                Window window = getWindow();
                window.setFlags(fullscreen ?
                                WindowManager.LayoutParams.FLAG_FULLSCREEN : 0,
                                WindowManager.LayoutParams.FLAG_FULLSCREEN);

                if (Build.VERSION.SDK_INT >= 11)
                    window.getDecorView().setSystemUiVisibility(fullscreen ? 1 : 0);
            }
        });
    }

    /**
     * Check and start the Java profiler if MOZ_PROFILER_STARTUP env var is specified
     **/
    protected void earlyStartJavaSampler(Intent intent)
    {
        String env = intent.getStringExtra("env0");
        for (int i = 1; env != null; i++) {
            if (env.startsWith("MOZ_PROFILER_STARTUP=")) {
                if (!env.endsWith("=")) {
                    GeckoJavaSampler.start(10, 1000);
                    Log.d(LOGTAG, "Profiling Java on startup");
                }
                break;
            }
            env = intent.getStringExtra("env" + i);
        }
    }

    /**
     * Called when the activity is first created.
     *
     * Here we initialize all of our profile settings, Firefox Health Report,
     * and other one-shot constructions.
     **/
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        GeckoAppShell.registerGlobalExceptionHandler();

        // Enable Android Strict Mode for developers' local builds (the "default" channel).
        if ("default".equals(AppConstants.MOZ_UPDATE_CHANNEL)) {
            enableStrictMode();
        }

        // The clock starts...now. Better hurry!
        mJavaUiStartupTimer = new Telemetry.UptimeTimer("FENNEC_STARTUP_TIME_JAVAUI");
        mGeckoReadyStartupTimer = new Telemetry.UptimeTimer("FENNEC_STARTUP_TIME_GECKOREADY");

        final Intent intent = getIntent();
        final String args = intent.getStringExtra("args");

        earlyStartJavaSampler(intent);

        // GeckoLoader wants to dig some environment variables out of the
        // incoming intent, so pass it in here. GeckoLoader will do its
        // business later and dispose of the reference.
        GeckoLoader.setLastIntent(intent);

        if (mProfile == null) {
            String profileName = null;
            String profilePath = null;
            if (args != null) {
                if (args.contains("-P")) {
                    Pattern p = Pattern.compile("(?:-P\\s*)(\\w*)(\\s*)");
                    Matcher m = p.matcher(args);
                    if (m.find()) {
                        profileName = m.group(1);
                    }
                }

                if (args.contains("-profile")) {
                    Pattern p = Pattern.compile("(?:-profile\\s*)(\\S*)(\\s*)");
                    Matcher m = p.matcher(args);
                    if (m.find()) {
                        profilePath =  m.group(1);
                    }
                    if (profileName == null) {
                        try {
                            profileName = getDefaultProfileName();
                        } catch (NoMozillaDirectoryException e) {
                            Log.wtf(LOGTAG, "Unable to fetch default profile name!", e);
                            // There's nothing at all we can do now. If the Mozilla directory
                            // didn't exist, then we're screwed.
                            // Crash here so we can fix the bug.
                            throw new RuntimeException(e);
                        }
                        if (profileName == null)
                            profileName = GeckoProfile.DEFAULT_PROFILE;
                    }
                    GeckoProfile.sIsUsingCustomProfile = true;
                }

                if (profileName != null || profilePath != null) {
                    mProfile = GeckoProfile.get(this, profileName, profilePath);
                }
            }
        }

        BrowserDB.initialize(getProfile().getName());

        // Workaround for <http://code.google.com/p/android/issues/detail?id=20915>.
        try {
            Class.forName("android.os.AsyncTask");
        } catch (ClassNotFoundException e) {}

        MemoryMonitor.getInstance().init(getApplicationContext());

        // GeckoAppShell is tightly coupled to us, rather than
        // the app context, because various parts of Fennec (e.g.,
        // GeckoScreenOrientation) use GAS to access the Activity in
        // the guise of fetching a Context.
        // When that's fixed, `this` can change to
        // `(GeckoApplication) getApplication()` here.
        GeckoAppShell.setContextGetter(this);
        GeckoAppShell.setGeckoInterface(this);

        ThreadUtils.setUiThread(Thread.currentThread(), new Handler());

        Tabs.getInstance().attachToContext(this);
        try {
            Favicons.attachToContext(this);
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception starting favicon cache. Corrupt resources?", e);
        }

        // Did the OS locale change while we were backgrounded? If so,
        // we need to die so that Gecko will re-init add-ons that touch
        // the UI.
        // This is using a sledgehammer to crack a nut, but it'll do for
        // now.
        if (BrowserLocaleManager.getInstance().systemLocaleDidChange()) {
            Log.i(LOGTAG, "System locale changed. Restarting.");
            doRestart();
            GeckoAppShell.systemExit();
            return;
        }

        if (GeckoThread.isCreated()) {
            // This happens when the GeckoApp activity is destroyed by Android
            // without killing the entire application (see Bug 769269).
            mIsRestoringActivity = true;
            Telemetry.HistogramAdd("FENNEC_RESTORING_ACTIVITY", 1);
        }

        // Fix for Bug 830557 on Tegra boards running Froyo.
        // This fix must be done before doing layout.
        // Assume the bug is fixed in Gingerbread and up.
        if (Build.VERSION.SDK_INT < 9) {
            try {
                Class<?> inputBindResultClass =
                    Class.forName("com.android.internal.view.InputBindResult");
                java.lang.reflect.Field creatorField =
                    inputBindResultClass.getField("CREATOR");
                Log.i(LOGTAG, "froyo startup fix: " + String.valueOf(creatorField.get(null)));
            } catch (Exception e) {
                Log.w(LOGTAG, "froyo startup fix failed", e);
            }
        }

        Bundle stateBundle = getIntent().getBundleExtra(EXTRA_STATE_BUNDLE);
        if (stateBundle != null) {
            // Use the state bundle if it was given as an intent extra. This is
            // only intended to be used internally via Robocop, so a boolean
            // is read from a private shared pref to prevent other apps from
            // injecting states.
            final SharedPreferences prefs = getSharedPreferences();
            if (prefs.getBoolean(PREFS_ALLOW_STATE_BUNDLE, false)) {
                Log.i(LOGTAG, "Restoring state from intent bundle");
                prefs.edit().remove(PREFS_ALLOW_STATE_BUNDLE).commit();
                savedInstanceState = stateBundle;
            }
        } else if (savedInstanceState != null) {
            // Bug 896992 - This intent has already been handled; reset the intent.
            setIntent(new Intent(Intent.ACTION_MAIN));
        }

        super.onCreate(savedInstanceState);

        GeckoScreenOrientation.getInstance().update(getResources().getConfiguration().orientation);

        setContentView(getLayout());

        // Set up Gecko layout.
        mGeckoLayout = (RelativeLayout) findViewById(R.id.gecko_layout);
        mMainLayout = (RelativeLayout) findViewById(R.id.main_layout);

        // Determine whether we should restore tabs.
        mShouldRestore = getSessionRestoreState(savedInstanceState);
        if (mShouldRestore && savedInstanceState != null) {
            boolean wasInBackground =
                savedInstanceState.getBoolean(SAVED_STATE_IN_BACKGROUND, false);

            // Don't log OOM-kills if only one activity was destroyed. (For example
            // from "Don't keep activities" on ICS)
            if (!wasInBackground && !mIsRestoringActivity) {
                Telemetry.HistogramAdd("FENNEC_WAS_KILLED", 1);
            }

            mPrivateBrowsingSession = savedInstanceState.getString(SAVED_STATE_PRIVATE_SESSION);
        }

        // Perform background initialization.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final SharedPreferences prefs = GeckoApp.this.getSharedPreferences();

                // Wait until now to set this, because we'd rather throw an exception than
                // have a caller of BrowserLocaleManager regress startup.
                final LocaleManager localeManager = BrowserLocaleManager.getInstance();
                localeManager.initialize(getApplicationContext());

                SessionInformation previousSession = SessionInformation.fromSharedPrefs(prefs);
                if (previousSession.wasKilled()) {
                    Telemetry.HistogramAdd("FENNEC_WAS_KILLED", 1);
                }

                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_OOM_EXCEPTION, false);

                // Put a flag to check if we got a normal `onSaveInstanceState`
                // on exit, or if we were suddenly killed (crash or native OOM).
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);

                editor.commit();

                // The lifecycle of mHealthRecorder is "shortly after onCreate"
                // through "onDestroy" -- essentially the same as the lifecycle
                // of the activity itself.
                final String profilePath = getProfile().getDir().getAbsolutePath();
                final EventDispatcher dispatcher = EventDispatcher.getInstance();
                Log.i(LOGTAG, "Creating HealthRecorder.");

                final String osLocale = Locale.getDefault().toString();
                String appLocale = localeManager.getAndApplyPersistedLocale(GeckoApp.this);
                Log.d(LOGTAG, "OS locale is " + osLocale + ", app locale is " + appLocale);

                if (appLocale == null) {
                    appLocale = osLocale;
                }

                mHealthRecorder = GeckoApp.this.createHealthRecorder(GeckoApp.this,
                                                                     profilePath,
                                                                     dispatcher,
                                                                     osLocale,
                                                                     appLocale,
                                                                     previousSession);

                final String uiLocale = appLocale;
                ThreadUtils.postToUiThread(new Runnable() {
                    @Override
                    public void run() {
                        GeckoApp.this.onLocaleReady(uiLocale);
                    }
                });
            }
        });

        GeckoAppShell.setNotificationClient(makeNotificationClient());
        NotificationHelper.init(getApplicationContext());
        IntentHelper.init(this);
    }

    /**
     * At this point, the resource system and the rest of the browser are
     * aware of the locale.
     *
     * Now we can display strings!
     *
     * You can think of this as being something like a second phase of onCreate,
     * where you can do string-related operations. Use this in place of embedding
     * strings in view XML.
     *
     * By contrast, onConfigurationChanged does some locale operations, but is in
     * response to device changes.
     */
    @Override
    public void onLocaleReady(final String locale) {
        if (!ThreadUtils.isOnUiThread()) {
            throw new RuntimeException("onLocaleReady must always be called from the UI thread.");
        }

        final Locale loc = BrowserLocaleManager.parseLocaleCode(locale);
        if (loc.equals(mLastLocale)) {
            Log.d(LOGTAG, "New locale same as old; onLocaleReady has nothing to do.");
        }

        // The URL bar hint needs to be populated.
        TextView urlBar = (TextView) findViewById(R.id.url_bar_title);
        if (urlBar != null) {
            final String hint = getResources().getString(R.string.url_bar_default_text);
            urlBar.setHint(hint);
        } else {
            Log.d(LOGTAG, "No URL bar in GeckoApp. Not loading localized hint string.");
        }

        mLastLocale = loc;

        // Allow onConfigurationChanged to take care of the rest.
        // We don't call this.onConfigurationChanged, because (a) that does
        // work that's unnecessary after this locale action, and (b) it can
        // cause a loop! See Bug 1011008, Comment 12.
        super.onConfigurationChanged(getResources().getConfiguration());
    }

    protected void initializeChrome() {
        mDoorHangerPopup = new DoorHangerPopup(this);
        mPluginContainer = (AbsoluteLayout) findViewById(R.id.plugin_container);
        mFormAssistPopup = (FormAssistPopup) findViewById(R.id.form_assist_popup);

        if (mCameraView == null) {
            // Pre-ICS devices need the camera surface in a visible layout.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.ICE_CREAM_SANDWICH) {
                mCameraView = new SurfaceView(this);
                ((SurfaceView)mCameraView).getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
            }
        }

        if (mLayerView == null) {
            LayerView layerView = (LayerView) findViewById(R.id.layer_view);
            layerView.initializeView(EventDispatcher.getInstance());
            mLayerView = layerView;
            GeckoAppShell.setLayerView(layerView);
            // bind the GeckoEditable instance to the new LayerView
            GeckoAppShell.notifyIMEContext(GeckoEditableListener.IME_STATE_DISABLED, "", "", "");
        }
    }

    /**
     * Loads the initial tab at Fennec startup.
     *
     * If Fennec was opened with an external URL, that URL will be loaded.
     * Otherwise, unless there was a session restore, the default URL
     * (about:home) be loaded.
     *
     * @param url External URL to load, or null to load the default URL
     */
    protected void loadStartupTab(String url) {
        if (url == null) {
            if (!mShouldRestore) {
                // Show about:home if we aren't restoring previous session and
                // there's no external URL.
                Tabs.getInstance().loadUrl(AboutPages.HOME, Tabs.LOADURL_NEW_TAB);
            }
        } else {
            // If given an external URL, load it
            int flags = Tabs.LOADURL_NEW_TAB | Tabs.LOADURL_USER_ENTERED | Tabs.LOADURL_EXTERNAL;
            Tabs.getInstance().loadUrl(url, flags);
        }
    }

    private void initialize() {
        mInitialized = true;

        Intent intent = getIntent();
        String action = intent.getAction();

        String passedUri = null;
        final String uri = getURIFromIntent(intent);
        if (!TextUtils.isEmpty(uri)) {
            passedUri = uri;
        }

        final boolean isExternalURL = passedUri != null &&
                                      !AboutPages.isAboutHome(passedUri);
        StartupAction startupAction;
        if (isExternalURL) {
            startupAction = StartupAction.URL;
        } else {
            startupAction = StartupAction.NORMAL;
        }

        // Start migrating as early as possible, can do this in
        // parallel with Gecko load.
        checkMigrateProfile();

        Uri data = intent.getData();
        if (data != null && "http".equals(data.getScheme())) {
            startupAction = StartupAction.PREFETCH;
            ThreadUtils.postToBackgroundThread(new PrefetchRunnable(data.toString()));
        }

        Tabs.registerOnTabsChangedListener(this);

        initializeChrome();

        // If we are doing a restore, read the session data and send it to Gecko
        if (!mIsRestoringActivity) {
            String restoreMessage = null;
            if (mShouldRestore) {
                try {
                    // restoreSessionTabs() will create simple tab stubs with the
                    // URL and title for each page, but we also need to restore
                    // session history. restoreSessionTabs() will inject the IDs
                    // of the tab stubs into the JSON data (which holds the session
                    // history). This JSON data is then sent to Gecko so session
                    // history can be restored for each tab.
                    restoreMessage = restoreSessionTabs(isExternalURL);
                } catch (SessionRestoreException e) {
                    // If restore failed, do a normal startup
                    Log.e(LOGTAG, "An error occurred during restore", e);
                    mShouldRestore = false;
                }
            }

            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Session:Restore", restoreMessage));
        }

        // External URLs should always be loaded regardless of whether Gecko is
        // already running.
        if (isExternalURL) {
            loadStartupTab(passedUri);
        } else if (!mIsRestoringActivity) {
            loadStartupTab(null);
        }

        // We now have tab stubs from the last session. Any future tabs should
        // be animated.
        Tabs.getInstance().notifyListeners(null, Tabs.TabEvents.RESTORED);

        // If we're not restoring, move the session file so it can be read for
        // the last tabs section.
        if (!mShouldRestore) {
            getProfile().moveSessionFile();
        }

        Telemetry.HistogramAdd("FENNEC_STARTUP_GECKOAPP_ACTION", startupAction.ordinal());

        if (!mIsRestoringActivity) {
            GeckoThread.setArgs(intent.getStringExtra("args"));
            GeckoThread.setAction(intent.getAction());
            GeckoThread.setUri(passedUri);
        }
        if (!ACTION_DEBUG.equals(action) &&
            GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.Launching, GeckoThread.LaunchState.Launched)) {
            GeckoThread.createAndStart();
        } else if (ACTION_DEBUG.equals(action) &&
            GeckoThread.checkAndSetLaunchState(GeckoThread.LaunchState.Launching, GeckoThread.LaunchState.WaitForDebugger)) {
            ThreadUtils.getUiHandler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    GeckoThread.setLaunchState(GeckoThread.LaunchState.Launching);
                    GeckoThread.createAndStart();
                }
            }, 1000 * 5 /* 5 seconds */);
        }

        // Check if launched from data reporting notification.
        if (ACTION_LAUNCH_SETTINGS.equals(action)) {
            Intent settingsIntent = new Intent(GeckoApp.this, GeckoPreferences.class);
            // Copy extras.
            settingsIntent.putExtras(intent);
            startActivity(settingsIntent);
        }

        //app state callbacks
        mAppStateListeners = new LinkedList<GeckoAppShell.AppStateListener>();

        //register for events
        EventDispatcher.getInstance().registerGeckoThreadListener((GeckoEventListener)this,
            "Gecko:Ready",
            "Gecko:DelayedStartup",
            "Accessibility:Event",
            "NativeApp:IsDebuggable");

        EventDispatcher.getInstance().registerGeckoThreadListener((NativeEventListener)this,
            "Accessibility:Ready",
            "Bookmark:Insert",
            "Contact:Add",
            "DOMFullScreen:Start",
            "DOMFullScreen:Stop",
            "Image:SetAs",
            "Locale:Set",
            "Permissions:Data",
            "PrivateBrowsing:Data",
            "Sanitize:ClearHistory",
            "Session:StatePurged",
            "Share:Text",
            "Shortcut:Remove",
            "SystemUI:Visibility",
            "Toast:Show",
            "ToggleChrome:Focus",
            "ToggleChrome:Hide",
            "ToggleChrome:Show",
            "Update:Check",
            "Update:Download",
            "Update:Install");

        if (mWebappEventListener == null) {
            mWebappEventListener = new EventListener();
            mWebappEventListener.registerEvents();
        }


        if (SmsManager.getInstance() != null) {
          SmsManager.getInstance().start();
        }

        mContactService = new ContactService(EventDispatcher.getInstance(), this);

        mPromptService = new PromptService(this);

        mTextSelection = new TextSelection((TextSelectionHandle) findViewById(R.id.start_handle),
                                           (TextSelectionHandle) findViewById(R.id.middle_handle),
                                           (TextSelectionHandle) findViewById(R.id.end_handle),
                                           EventDispatcher.getInstance(),
                                           this);

        PrefsHelper.getPref("app.update.autodownload", new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, String value) {
                UpdateServiceHelper.registerForUpdates(GeckoApp.this, value);
            }
        });

        PrefsHelper.getPref("app.geo.reportdata", new PrefsHelper.PrefHandlerBase() {
            @Override public void prefValue(String pref, int value) {
                if (value == 1)
                    mShouldReportGeoData = true;
                else
                    mShouldReportGeoData = false;
            }
        });

        // Trigger the completion of the telemetry timer that wraps activity startup,
        // then grab the duration to give to FHR.
        mJavaUiStartupTimer.stop();
        final long javaDuration = mJavaUiStartupTimer.getElapsed();

        ThreadUtils.getBackgroundHandler().postDelayed(new Runnable() {
            @Override
            public void run() {
                final HealthRecorder rec = mHealthRecorder;
                if (rec != null) {
                    rec.recordJavaStartupTime(javaDuration);
                }

                // Record our launch time for the announcements service
                // to use in assessing inactivity.
                final Context context = GeckoApp.this;
                AnnouncementsBroadcastService.recordLastLaunch(context);

                // Kick off our background services. We do this by invoking the broadcast
                // receiver, which uses the system alarm infrastructure to perform tasks at
                // intervals.
                GeckoPreferences.broadcastAnnouncementsPref(context);
                GeckoPreferences.broadcastHealthReportUploadPref(context);
                if (!GeckoThread.checkLaunchState(GeckoThread.LaunchState.Launched)) {
                    return;
                }
            }
        }, 50);

        if (mIsRestoringActivity) {
            GeckoThread.setLaunchState(GeckoThread.LaunchState.GeckoRunning);
            Tab selectedTab = Tabs.getInstance().getSelectedTab();
            if (selectedTab != null)
                Tabs.getInstance().notifyListeners(selectedTab, Tabs.TabEvents.SELECTED);
            geckoConnected();
            GeckoAppShell.setLayerClient(mLayerView.getLayerClientObject());
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Viewport:Flush", null));
        }

        if (ACTION_ALERT_CALLBACK.equals(action)) {
            processAlertCallback(intent);
        }
    }

    private String restoreSessionTabs(final boolean isExternalURL) throws SessionRestoreException {
        try {
            String sessionString = getProfile().readSessionFile(false);
            if (sessionString == null) {
                throw new SessionRestoreException("Could not read from session file");
            }

            // If we are doing an OOM restore, parse the session data and
            // stub the restored tabs immediately. This allows the UI to be
            // updated before Gecko has restored.
            if (mShouldRestore) {
                final JSONArray tabs = new JSONArray();
                SessionParser parser = new SessionParser() {
                    @Override
                    public void onTabRead(SessionTab sessionTab) {
                        JSONObject tabObject = sessionTab.getTabObject();

                        int flags = Tabs.LOADURL_NEW_TAB;
                        flags |= ((isExternalURL || !sessionTab.isSelected()) ? Tabs.LOADURL_DELAY_LOAD : 0);
                        flags |= (tabObject.optBoolean("desktopMode") ? Tabs.LOADURL_DESKTOP : 0);
                        flags |= (tabObject.optBoolean("isPrivate") ? Tabs.LOADURL_PRIVATE : 0);

                        Tab tab = Tabs.getInstance().loadUrl(sessionTab.getUrl(), flags);
                        tab.updateTitle(sessionTab.getTitle());

                        try {
                            tabObject.put("tabId", tab.getId());
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "JSON error", e);
                        }
                        tabs.put(tabObject);
                    }
                };

                if (mPrivateBrowsingSession == null) {
                    parser.parse(sessionString);
                } else {
                    parser.parse(sessionString, mPrivateBrowsingSession);
                }

                if (tabs.length() > 0) {
                    sessionString = new JSONObject().put("windows", new JSONArray().put(new JSONObject().put("tabs", tabs))).toString();
                } else {
                    throw new SessionRestoreException("No tabs could be read from session file");
                }
            }

            JSONObject restoreData = new JSONObject();
            restoreData.put("sessionString", sessionString);
            return restoreData.toString();

        } catch (JSONException e) {
            throw new SessionRestoreException(e);
        }
    }

    public GeckoProfile getProfile() {
        // fall back to default profile if we didn't load a specific one
        if (mProfile == null) {
            mProfile = GeckoProfile.get(this);
        }
        return mProfile;
    }

    /**
     * Determine whether the session should be restored.
     *
     * @param savedInstanceState Saved instance state given to the activity
     * @return                   Whether to restore
     */
    protected boolean getSessionRestoreState(Bundle savedInstanceState) {
        final SharedPreferences prefs = getSharedPreferences();
        boolean shouldRestore = false;

        final int versionCode = getVersionCode();
        if (prefs.getInt(PREFS_VERSION_CODE, 0) != versionCode) {
            // If the version has changed, the user has done an upgrade, so restore
            // previous tabs.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    prefs.edit()
                         .putInt(PREFS_VERSION_CODE, versionCode)
                         .commit();
                }
            });

            shouldRestore = true;
        } else if (savedInstanceState != null ||
                   getSessionRestorePreference().equals("always") ||
                   getRestartFromIntent()) {
            // We're coming back from a background kill by the OS, the user
            // has chosen to always restore, or we restarted.
            shouldRestore = true;
        } else if (prefs.getBoolean(GeckoApp.PREFS_CRASHED, false)) {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    prefs.edit().putBoolean(PREFS_CRASHED, false).commit();
                }
            });
            shouldRestore = true;
        }

        return shouldRestore;
    }

    private String getSessionRestorePreference() {
        return getSharedPreferences().getString(GeckoPreferences.PREFS_RESTORE_SESSION, "quit");
    }

    private boolean getRestartFromIntent() {
        return getIntent().getBooleanExtra("didRestart", false);
    }

    /**
     * Enable Android StrictMode checks (for supported OS versions).
     * http://developer.android.com/reference/android/os/StrictMode.html
     */
    private void enableStrictMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {
            return;
        }

        Log.d(LOGTAG, "Enabling Android StrictMode");

        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                  .detectAll()
                                  .penaltyLog()
                                  .build());

        StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                               .detectAll()
                               .penaltyLog()
                               .build());
    }

    public void enableCameraView() {
        // Start listening for orientation events
        mCameraOrientationEventListener = new OrientationEventListener(this) {
            @Override
            public void onOrientationChanged(int orientation) {
                if (mAppStateListeners != null) {
                    for (GeckoAppShell.AppStateListener listener: mAppStateListeners) {
                        listener.onOrientationChanged();
                    }
                }
            }
        };
        mCameraOrientationEventListener.enable();

        // Try to make it fully transparent.
        if (mCameraView != null && (mCameraView instanceof SurfaceView)) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
                mCameraView.setAlpha(0.0f);
            }
            ViewGroup mCameraLayout = (ViewGroup) findViewById(R.id.camera_layout);
            // Some phones (eg. nexus S) need at least a 8x16 preview size
            mCameraLayout.addView(mCameraView,
                                  new AbsoluteLayout.LayoutParams(8, 16, 0, 0));
        }
    }

    public void disableCameraView() {
        if (mCameraOrientationEventListener != null) {
            mCameraOrientationEventListener.disable();
            mCameraOrientationEventListener = null;
        }
        if (mCameraView != null) {
          ViewGroup mCameraLayout = (ViewGroup) findViewById(R.id.camera_layout);
          mCameraLayout.removeView(mCameraView);
        }
    }

    public String getDefaultUAString() {
        return HardwareUtils.isTablet() ? AppConstants.USER_AGENT_FENNEC_TABLET :
                                          AppConstants.USER_AGENT_FENNEC_MOBILE;
    }

    public String getUAStringForHost(String host) {
        // With our standard UA String, we get a 200 response code and
        // client-side redirect from t.co. This bot-like UA gives us a
        // 301 response code
        if ("t.co".equals(host)) {
            return AppConstants.USER_AGENT_BOT_LIKE;
        }
        return getDefaultUAString();
    }

    class PrefetchRunnable implements Runnable {
        private String mPrefetchUrl;

        PrefetchRunnable(String prefetchUrl) {
            mPrefetchUrl = prefetchUrl;
        }

        @Override
        public void run() {
            HttpURLConnection connection = null;
            try {
                URL url = new URL(mPrefetchUrl);
                // data url should have an http scheme
                connection = (HttpURLConnection) url.openConnection();
                connection.setRequestProperty("User-Agent", getUAStringForHost(url.getHost()));
                connection.setInstanceFollowRedirects(false);
                connection.setRequestMethod("GET");
                connection.connect();
            } catch (Exception e) {
                Log.e(LOGTAG, "Exception prefetching URL", e);
            } finally {
                if (connection != null)
                    connection.disconnect();
            }
        }
    }

    private void processAlertCallback(Intent intent) {
        String alertName = "";
        String alertCookie = "";
        Uri data = intent.getData();
        if (data != null) {
            alertName = data.getQueryParameter("name");
            if (alertName == null)
                alertName = "";
            alertCookie = data.getQueryParameter("cookie");
            if (alertCookie == null)
                alertCookie = "";
        }
        handleNotification(ACTION_ALERT_CALLBACK, alertName, alertCookie);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoExiting)) {
            // We're exiting and shouldn't try to do anything else. In the case
            // where we are hung while exiting, we should force the process to exit.
            GeckoAppShell.systemExit();
            return;
        }

        // if we were previously OOM killed, we can end up here when launching
        // from external shortcuts, so set this as the intent for initialization
        if (!mInitialized) {
            setIntent(intent);
            return;
        }

        final String action = intent.getAction();

        if (ACTION_LOAD.equals(action)) {
            String uri = intent.getDataString();
            Tabs.getInstance().loadUrl(uri);
        } else if (Intent.ACTION_VIEW.equals(action)) {
            String uri = intent.getDataString();
            Tabs.getInstance().loadUrl(uri, Tabs.LOADURL_NEW_TAB |
                                            Tabs.LOADURL_USER_ENTERED |
                                            Tabs.LOADURL_EXTERNAL);
        } else if (action != null && action.startsWith(ACTION_WEBAPP_PREFIX)) {
            // A lightweight mechanism for loading a web page as a webapp
            // without installing the app natively nor registering it in the DOM
            // application registry.
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createWebappLoadEvent(uri));
        } else if (ACTION_BOOKMARK.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBookmarkLoadEvent(uri));
        } else if (Intent.ACTION_SEARCH.equals(action)) {
            String uri = getURIFromIntent(intent);
            GeckoAppShell.sendEventToGecko(GeckoEvent.createURILoadEvent(uri));
        } else if (ACTION_ALERT_CALLBACK.equals(action)) {
            processAlertCallback(intent);
        } else if (ACTION_LAUNCH_SETTINGS.equals(action)) {
            // Check if launched from data reporting notification.
            Intent settingsIntent = new Intent(GeckoApp.this, GeckoPreferences.class);
            // Copy extras.
            settingsIntent.putExtras(intent);
            startActivity(settingsIntent);
        }
    }

    /*
     * Handles getting a uri from and intent in a way that is backwards
     * compatable with our previous implementations
     */
    protected String getURIFromIntent(Intent intent) {
        final String action = intent.getAction();
        if (ACTION_ALERT_CALLBACK.equals(action))
            return null;

        String uri = intent.getDataString();
        if (uri != null)
            return uri;

        if ((action != null && action.startsWith(ACTION_WEBAPP_PREFIX)) || ACTION_BOOKMARK.equals(action)) {
            uri = intent.getStringExtra("args");
            if (uri != null && uri.startsWith("--url=")) {
                uri.replace("--url=", "");
            }
        }
        return uri;
    }

    protected int getOrientation() {
        return GeckoScreenOrientation.getInstance().getAndroidOrientation();
    }

    @Override
    public void onResume()
    {
        // After an onPause, the activity is back in the foreground.
        // Undo whatever we did in onPause.
        super.onResume();

        int newOrientation = getResources().getConfiguration().orientation;
        if (GeckoScreenOrientation.getInstance().update(newOrientation)) {
            refreshChrome();
        }

        // User may have enabled/disabled accessibility.
        GeckoAccessibility.updateAccessibilitySettings(this);

        if (mAppStateListeners != null) {
            for (GeckoAppShell.AppStateListener listener: mAppStateListeners) {
                listener.onResume();
            }
        }

        // We use two times: a pseudo-unique wall-clock time to identify the
        // current session across power cycles, and the elapsed realtime to
        // track the duration of the session.
        final long now = System.currentTimeMillis();
        final long realTime = android.os.SystemClock.elapsedRealtime();

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // Now construct the new session on HealthRecorder's behalf. We do this here
                // so it can benefit from a single near-startup prefs commit.
                SessionInformation currentSession = new SessionInformation(now, realTime);

                SharedPreferences prefs = GeckoApp.this.getSharedPreferences();
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                currentSession.recordBegin(editor);
                editor.commit();

                final HealthRecorder rec = mHealthRecorder;
                if (rec != null) {
                    rec.setCurrentSession(currentSession);
                } else {
                    Log.w(LOGTAG, "Can't record session: rec is null.");
                }
            }
         });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (!mInitialized && hasFocus) {
            initialize();
            getWindow().setBackgroundDrawable(null);
        }
    }

    @Override
    public void onPause()
    {
        final HealthRecorder rec = mHealthRecorder;
        final Context context = this;

        // In some way it's sad that Android will trigger StrictMode warnings
        // here as the whole point is to save to disk while the activity is not
        // interacting with the user.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SharedPreferences prefs = GeckoApp.this.getSharedPreferences();
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, true);
                if (rec != null) {
                    rec.recordSessionEnd("P", editor);
                }

                // If we haven't done it before, cleanup any old files in our old temp dir
                if (prefs.getBoolean(GeckoApp.PREFS_CLEANUP_TEMP_FILES, true)) {
                    File tempDir = GeckoLoader.getGREDir(GeckoApp.this);
                    FileUtils.delTree(tempDir, new FileUtils.NameAndAgeFilter(null, ONE_DAY_MS), false);

                    editor.putBoolean(GeckoApp.PREFS_CLEANUP_TEMP_FILES, false);
                }

                editor.commit();

                // In theory, the first browser session will not run long enough that we need to
                // prune during it and we'd rather run it when the browser is inactive so we wait
                // until here to register the prune service.
                GeckoPreferences.broadcastHealthReportPrune(context);
            }
        });

        if (mAppStateListeners != null) {
            for(GeckoAppShell.AppStateListener listener: mAppStateListeners) {
                listener.onPause();
            }
        }

        super.onPause();
    }

    @Override
    public void onRestart()
    {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                SharedPreferences prefs = GeckoApp.this.getSharedPreferences();
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_WAS_STOPPED, false);
                editor.commit();
            }
        });

        super.onRestart();
    }

    @Override
    public void onDestroy()
    {
        EventDispatcher.getInstance().unregisterGeckoThreadListener((GeckoEventListener)this,
            "Gecko:Ready",
            "Gecko:DelayedStartup",
            "Accessibility:Event",
            "NativeApp:IsDebuggable");

        EventDispatcher.getInstance().unregisterGeckoThreadListener((NativeEventListener)this,
            "Accessibility:Ready",
            "Bookmark:Insert",
            "Contact:Add",
            "DOMFullScreen:Start",
            "DOMFullScreen:Stop",
            "Image:SetAs",
            "Locale:Set",
            "Permissions:Data",
            "PrivateBrowsing:Data",
            "Sanitize:ClearHistory",
            "Session:StatePurged",
            "Share:Text",
            "Shortcut:Remove",
            "SystemUI:Visibility",
            "Toast:Show",
            "ToggleChrome:Focus",
            "ToggleChrome:Hide",
            "ToggleChrome:Show",
            "Update:Check",
            "Update:Download",
            "Update:Install");

        if (mWebappEventListener != null) {
            mWebappEventListener.unregisterEvents();
            mWebappEventListener = null;
        }

        deleteTempFiles();

        if (mLayerView != null)
            mLayerView.destroy();
        if (mDoorHangerPopup != null)
            mDoorHangerPopup.destroy();
        if (mFormAssistPopup != null)
            mFormAssistPopup.destroy();
        if (mContactService != null)
            mContactService.destroy();
        if (mPromptService != null)
            mPromptService.destroy();
        if (mTextSelection != null)
            mTextSelection.destroy();
        NotificationHelper.destroy();
        IntentHelper.destroy();

        if (SmsManager.getInstance() != null) {
            SmsManager.getInstance().stop();
            if (isFinishing())
                SmsManager.getInstance().shutdown();
        }

        final HealthRecorder rec = mHealthRecorder;
        mHealthRecorder = null;
        if (rec != null && rec.isEnabled()) {
            // Closing a BrowserHealthRecorder could incur a write.
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    rec.close();
                }
            });
        }

        Favicons.close();

        super.onDestroy();

        Tabs.unregisterOnTabsChangedListener(this);
    }

    // Get a temporary directory, may return null
    public static File getTempDirectory() {
        File dir = GeckoApplication.get().getExternalFilesDir("temp");
        return dir;
    }

    // Delete any files in our temporary directory
    public static void deleteTempFiles() {
        File dir = getTempDirectory();
        if (dir == null)
            return;
        File[] files = dir.listFiles();
        if (files == null)
            return;
        for (File file : files) {
            file.delete();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        Log.d(LOGTAG, "onConfigurationChanged: " + newConfig.locale);

        final LocaleManager localeManager = BrowserLocaleManager.getInstance();
        final Locale changed = localeManager.onSystemConfigurationChanged(this, getResources(), newConfig, mLastLocale);
        if (changed != null) {
            onLocaleChanged(BrowserLocaleManager.getLanguageTag(changed));
        }

        // onConfigurationChanged is not called for 180 degree orientation changes,
        // we will miss such rotations and the screen orientation will not be
        // updated.
        if (GeckoScreenOrientation.getInstance().update(newConfig.orientation)) {
            if (mFormAssistPopup != null)
                mFormAssistPopup.hide();
            refreshChrome();
        }
        super.onConfigurationChanged(newConfig);
    }

    public String getContentProcessName() {
        return AppConstants.MOZ_CHILD_PROCESS_NAME;
    }

    public void addEnvToIntent(Intent intent) {
        Map<String,String> envMap = System.getenv();
        Set<Map.Entry<String,String>> envSet = envMap.entrySet();
        Iterator<Map.Entry<String,String>> envIter = envSet.iterator();
        int c = 0;
        while (envIter.hasNext()) {
            Map.Entry<String,String> entry = envIter.next();
            intent.putExtra("env" + c, entry.getKey() + "="
                            + entry.getValue());
            c++;
        }
    }

    public void doRestart() {
        doRestart(RESTARTER_ACTION, null);
    }

    public void doRestart(String args) {
        doRestart(RESTARTER_ACTION, args);
    }

    public void doRestart(String action, String args) {
        Log.d(LOGTAG, "doRestart(\"" + action + "\")");
        try {
            Intent intent = new Intent(action);
            intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, RESTARTER_CLASS);
            /* TODO: addEnvToIntent(intent); */
            if (args != null)
                intent.putExtra("args", args);
            intent.putExtra("didRestart", true);
            Log.d(LOGTAG, "Restart intent: " + intent.toString());
            GeckoAppShell.killAnyZombies();
            startActivity(intent);
        } catch (Exception e) {
            Log.e(LOGTAG, "Error effecting restart.", e);
        }

        finish();
        // Give the restart process time to start before we die
        GeckoAppShell.waitForAnotherGeckoProc();
    }

    public void handleNotification(String action, String alertName, String alertCookie) {
        // If Gecko isn't running yet, we ignore the notification. Note that
        // even if Gecko is running but it was restarted since the notification
        // was created, the notification won't be handled (bug 849653).
        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            GeckoAppShell.handleNotification(action, alertName, alertCookie);
        }
    }

    private void checkMigrateProfile() {
        final File profileDir = getProfile().getDir();

        if (profileDir != null) {
            ThreadUtils.postToBackgroundThread(new Runnable() {
                @Override
                public void run() {
                    Handler handler = new Handler();
                    handler.postDelayed(new DeferredCleanupTask(), CLEANUP_DEFERRAL_SECONDS * 1000);
                }
            });
        }
    }

    private class DeferredCleanupTask implements Runnable {
        // The cleanup-version setting is recorded to avoid repeating the same
        // tasks on subsequent startups; CURRENT_CLEANUP_VERSION may be updated
        // if we need to do additional cleanup for future Gecko versions.

        private static final String CLEANUP_VERSION = "cleanup-version";
        private static final int CURRENT_CLEANUP_VERSION = 1;

        @Override
        public void run() {
            long cleanupVersion = getSharedPreferences().getInt(CLEANUP_VERSION, 0);

            if (cleanupVersion < 1) {
                // Reduce device storage footprint by removing .ttf files from
                // the res/fonts directory: we no longer need to copy our
                // bundled fonts out of the APK in order to use them.
                // See https://bugzilla.mozilla.org/show_bug.cgi?id=878674.
                File dir = new File("res/fonts");
                if (dir.exists() && dir.isDirectory()) {
                    for (File file : dir.listFiles()) {
                        if (file.isFile() && file.getName().endsWith(".ttf")) {
                            Log.i(LOGTAG, "deleting " + file.toString());
                            file.delete();
                        }
                    }
                    if (!dir.delete()) {
                        Log.w(LOGTAG, "unable to delete res/fonts directory (not empty?)");
                    } else {
                        Log.i(LOGTAG, "res/fonts directory deleted");
                    }
                }
            }

            // Additional cleanup needed for future versions would go here

            if (cleanupVersion != CURRENT_CLEANUP_VERSION) {
                SharedPreferences.Editor editor = GeckoApp.this.getSharedPreferences().edit();
                editor.putInt(CLEANUP_VERSION, CURRENT_CLEANUP_VERSION);
                editor.commit();
            }
        }
    }

    public PromptService getPromptService() {
        return mPromptService;
    }

    @Override
    public void onBackPressed() {
        if (getSupportFragmentManager().getBackStackEntryCount() > 0) {
            super.onBackPressed();
            return;
        }

        if (autoHideTabs()) {
            return;
        }

        if (mDoorHangerPopup != null && mDoorHangerPopup.isShowing()) {
            mDoorHangerPopup.dismiss();
            return;
        }

        if (mFullScreenPluginView != null) {
            GeckoAppShell.onFullScreenPluginHidden(mFullScreenPluginView);
            removeFullScreenPluginView(mFullScreenPluginView);
            return;
        }

        if (mLayerView != null && mLayerView.isFullScreen()) {
            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("FullScreen:Exit", null));
            return;
        }

        Tabs tabs = Tabs.getInstance();
        Tab tab = tabs.getSelectedTab();
        if (tab == null) {
            moveTaskToBack(true);
            return;
        }

        if (tab.doBack())
            return;

        if (tab.isExternal()) {
            moveTaskToBack(true);
            tabs.closeTab(tab);
            return;
        }

        int parentId = tab.getParentId();
        Tab parent = tabs.getTab(parentId);
        if (parent != null) {
            // The back button should always return to the parent (not a sibling).
            tabs.closeTab(tab, parent);
            return;
        }

        moveTaskToBack(true);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (!ActivityHandlerHelper.handleActivityResult(requestCode, resultCode, data)) {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    public AbsoluteLayout getPluginContainer() { return mPluginContainer; }

    // Accelerometer.
    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createSensorEvent(event));
    }

    // Geolocation.
    @Override
    public void onLocationChanged(Location location) {
        // No logging here: user-identifying information.
        GeckoAppShell.sendEventToGecko(GeckoEvent.createLocationEvent(location));
        if (mShouldReportGeoData)
            collectAndReportLocInfo(location);
    }

    public void setCurrentSignalStrenth(SignalStrength ss) {
        if (ss.isGsm())
            mSignalStrenth = ss.getGsmSignalStrength();
    }

    private int getCellInfo(JSONArray cellInfo) {
        TelephonyManager tm = (TelephonyManager)getSystemService(Context.TELEPHONY_SERVICE);
        if (tm == null)
            return TelephonyManager.PHONE_TYPE_NONE;
        List<NeighboringCellInfo> cells = tm.getNeighboringCellInfo();
        CellLocation cl = tm.getCellLocation();
        String mcc = "", mnc = "";
        if (cl instanceof GsmCellLocation) {
            JSONObject obj = new JSONObject();
            GsmCellLocation gcl = (GsmCellLocation)cl;
            try {
                obj.put("lac", gcl.getLac());
                obj.put("cid", gcl.getCid());

                int psc = (Build.VERSION.SDK_INT >= 9) ? gcl.getPsc() : -1;
                obj.put("psc", psc);

                switch(tm.getNetworkType()) {
                case TelephonyManager.NETWORK_TYPE_GPRS:
                case TelephonyManager.NETWORK_TYPE_EDGE:
                    obj.put("radio", "gsm");
                    break;
                case TelephonyManager.NETWORK_TYPE_UMTS:
                case TelephonyManager.NETWORK_TYPE_HSDPA:
                case TelephonyManager.NETWORK_TYPE_HSUPA:
                case TelephonyManager.NETWORK_TYPE_HSPA:
                case TelephonyManager.NETWORK_TYPE_HSPAP:
                    obj.put("radio", "umts");
                    break;
                }
                String mcc_mnc = tm.getNetworkOperator();
                if (mcc_mnc.length() > 3) {
                    mcc = mcc_mnc.substring(0, 3);
                    mnc = mcc_mnc.substring(3);
                    obj.put("mcc", mcc);
                    obj.put("mnc", mnc);
                }
                obj.put("asu", mSignalStrenth);
            } catch(JSONException jsonex) {}
            cellInfo.put(obj);
        }
        if (cells != null) {
            for (NeighboringCellInfo nci : cells) {
                try {
                    JSONObject obj = new JSONObject();
                    obj.put("lac", nci.getLac());
                    obj.put("cid", nci.getCid());
                    obj.put("psc", nci.getPsc());
                    obj.put("mcc", mcc);
                    obj.put("mnc", mnc);

                    int dbm;
                    switch(nci.getNetworkType()) {
                    case TelephonyManager.NETWORK_TYPE_GPRS:
                    case TelephonyManager.NETWORK_TYPE_EDGE:
                        obj.put("radio", "gsm");
                        break;
                    case TelephonyManager.NETWORK_TYPE_UMTS:
                    case TelephonyManager.NETWORK_TYPE_HSDPA:
                    case TelephonyManager.NETWORK_TYPE_HSUPA:
                    case TelephonyManager.NETWORK_TYPE_HSPA:
                    case TelephonyManager.NETWORK_TYPE_HSPAP:
                        obj.put("radio", "umts");
                        break;
                    }

                    obj.put("asu", nci.getRssi());
                    cellInfo.put(obj);
                } catch(JSONException jsonex) {}
            }
        }
        return tm.getPhoneType();
    }

    private static boolean shouldLog(final ScanResult sr) {
        return sr.SSID == null || !sr.SSID.endsWith("_nomap");
    }

    private void collectAndReportLocInfo(Location location) {
        final JSONObject locInfo = new JSONObject();
        WifiManager wm = (WifiManager)getSystemService(Context.WIFI_SERVICE);
        wm.startScan();
        try {
            JSONArray cellInfo = new JSONArray();

            String radioType = getRadioTypeName(getCellInfo(cellInfo));
            if (radioType != null) {
                locInfo.put("radio", radioType);
            }

            locInfo.put("lon", location.getLongitude());
            locInfo.put("lat", location.getLatitude());

            // If we have an accuracy, round it up to the next meter.
            if (location.hasAccuracy()) {
                locInfo.put("accuracy", (int) Math.ceil(location.getAccuracy()));
            }

            // If we have an altitude, round it to the nearest meter.
            if (location.hasAltitude()) {
                locInfo.put("altitude", Math.round(location.getAltitude()));
            }

            // Reduce timestamp precision so as to expose less PII.
            DateFormat df = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
            locInfo.put("time", df.format(new Date(location.getTime())));
            locInfo.put("cell", cellInfo);

            JSONArray wifiInfo = new JSONArray();
            List<ScanResult> aps = wm.getScanResults();
            if (aps != null) {
                for (ScanResult ap : aps) {
                    if (!shouldLog(ap))
                        continue;

                    JSONObject obj = new JSONObject();
                    obj.put("key", ap.BSSID);
                    obj.put("frequency", ap.frequency);
                    obj.put("signal", ap.level);
                    wifiInfo.put(obj);
                }
            }
            locInfo.put("wifi", wifiInfo);
        } catch (JSONException jsonex) {
            Log.w(LOGTAG, "json exception", jsonex);
            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            public void run() {
                try {
                    URL url = new URL(LOCATION_URL);
                    HttpURLConnection urlConnection = (HttpURLConnection) url.openConnection();
                    try {
                        urlConnection.setDoOutput(true);

                        // Workaround for a bug in Android HttpURLConnection. When the library
                        // reuses a stale connection, the connection may fail with an EOFException.
                        if (Build.VERSION.SDK_INT >= 14 && Build.VERSION.SDK_INT <= 18) {
                            urlConnection.setRequestProperty("Connection", "Close");
                        }

                        JSONArray batch = new JSONArray();
                        batch.put(locInfo);
                        JSONObject wrapper = new JSONObject();
                        wrapper.put("items", batch);
                        byte[] bytes = wrapper.toString().getBytes();
                        urlConnection.setFixedLengthStreamingMode(bytes.length);
                        OutputStream out = new BufferedOutputStream(urlConnection.getOutputStream());
                        out.write(bytes);
                        out.flush();
                    } catch (JSONException jsonex) {
                        Log.e(LOGTAG, "error wrapping data as a batch", jsonex);
                    } catch (IOException ioex) {
                        Log.e(LOGTAG, "error submitting data", ioex);
                    } finally {
                        urlConnection.disconnect();
                    }
                } catch (IOException ioex) {
                    Log.e(LOGTAG, "error submitting data", ioex);
                }
            }
        });
    }

    private static String getRadioTypeName(int phoneType) {
        switch (phoneType) {
            case TelephonyManager.PHONE_TYPE_CDMA:
                return "cdma";

            case TelephonyManager.PHONE_TYPE_GSM:
                return "gsm";

            case TelephonyManager.PHONE_TYPE_NONE:
            case TelephonyManager.PHONE_TYPE_SIP:
                // These devices have no radio.
                return null;

            default:
                Log.e(LOGTAG, "", new IllegalArgumentException("Unexpected PHONE_TYPE: " + phoneType));
                return null;
        }
    }

    @Override
    public void onProviderDisabled(String provider)
    {
    }

    @Override
    public void onProviderEnabled(String provider)
    {
    }

    @Override
    public void onStatusChanged(String provider, int status, Bundle extras)
    {
    }

    // Called when a Gecko Hal WakeLock is changed
    public void notifyWakeLockChanged(String topic, String state) {
        PowerManager.WakeLock wl = mWakeLocks.get(topic);
        if (state.equals("locked-foreground") && wl == null) {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            wl = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, topic);
            wl.acquire();
            mWakeLocks.put(topic, wl);
        } else if (!state.equals("locked-foreground") && wl != null) {
            wl.release();
            mWakeLocks.remove(topic);
        }
    }

    public void notifyCheckUpdateResult(String result) {
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Update:CheckResult", result));
    }

    protected void geckoConnected() {
        mLayerView.geckoConnected();
        mLayerView.setOverScrollMode(View.OVER_SCROLL_NEVER);
    }

    public void setAccessibilityEnabled(boolean enabled) {
    }

    public static class MainLayout extends RelativeLayout {
        private TouchEventInterceptor mTouchEventInterceptor;
        private MotionEventInterceptor mMotionEventInterceptor;

        public MainLayout(Context context, AttributeSet attrs) {
            super(context, attrs);
        }

        public void setTouchEventInterceptor(TouchEventInterceptor interceptor) {
            mTouchEventInterceptor = interceptor;
        }

        public void setMotionEventInterceptor(MotionEventInterceptor interceptor) {
            mMotionEventInterceptor = interceptor;
        }

        @Override
        public boolean onInterceptTouchEvent(MotionEvent event) {
            if (mTouchEventInterceptor != null && mTouchEventInterceptor.onInterceptTouchEvent(this, event)) {
                return true;
            }
            return super.onInterceptTouchEvent(event);
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            if (mTouchEventInterceptor != null && mTouchEventInterceptor.onTouch(this, event)) {
                return true;
            }
            return super.onTouchEvent(event);
        }

        @Override
        public boolean onGenericMotionEvent(MotionEvent event) {
            if (mMotionEventInterceptor != null && mMotionEventInterceptor.onInterceptMotionEvent(this, event)) {
                return true;
            }
            return super.onGenericMotionEvent(event);
        }

        @Override
        public void setDrawingCacheEnabled(boolean enabled) {
            // Instead of setting drawing cache in the view itself, we simply
            // enable drawing caching on its children. This is mainly used in
            // animations (see PropertyAnimator)
            super.setChildrenDrawnWithCacheEnabled(enabled);
        }
    }

    private class FullScreenHolder extends FrameLayout {

        public FullScreenHolder(Context ctx) {
            super(ctx);
        }

        @Override
        public void addView(View view, int index) {
            /**
             * This normally gets called when Flash adds a separate SurfaceView
             * for the video. It is unhappy if we have the LayerView underneath
             * it for some reason so we need to hide that. Hiding the LayerView causes
             * its surface to be destroyed, which causes a pause composition
             * event to be sent to Gecko. We synchronously wait for that to be
             * processed. Simultaneously, however, Flash is waiting on a mutex so
             * the post() below is an attempt to avoid a deadlock.
             */
            super.addView(view, index);

            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    mLayerView.hideSurface();
                }
            });
        }

        /**
         * The methods below are simply copied from what Android WebKit does.
         * It wasn't ever called in my testing, but might as well
         * keep it in case it is for some reason. The methods
         * all return true because we don't want any events
         * leaking out from the fullscreen view.
         */
        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (event.isSystem()) {
                return super.onKeyDown(keyCode, event);
            }
            mFullScreenPluginView.onKeyDown(keyCode, event);
            return true;
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            if (event.isSystem()) {
                return super.onKeyUp(keyCode, event);
            }
            mFullScreenPluginView.onKeyUp(keyCode, event);
            return true;
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            return true;
        }

        @Override
        public boolean onTrackballEvent(MotionEvent event) {
            mFullScreenPluginView.onTrackballEvent(event);
            return true;
        }
    }

    protected NotificationClient makeNotificationClient() {
        // Don't use a notification service; we may be killed in the background
        // during downloads.
        return new AppNotificationClient(getApplicationContext());
    }

    private int getVersionCode() {
        int versionCode = 0;
        try {
            versionCode = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
        } catch (NameNotFoundException e) {
            Log.wtf(LOGTAG, getPackageName() + " not found", e);
        }
        return versionCode;
    }

    protected boolean getIsDebuggable() {
        // Return false so Fennec doesn't appear to be debuggable.  WebappImpl
        // then overrides this and returns the value of android:debuggable for
        // the webapp APK, so webapps get the behavior supported by this method
        // (i.e. automatic configuration and enabling of the remote debugger).
        return false;

        // If we ever want to expose this for Fennec, here's how we would do it:
        // int flags = 0;
        // try {
        //     flags = getPackageManager().getPackageInfo(getPackageName(), 0).applicationInfo.flags;
        // } catch (NameNotFoundException e) {
        //     Log.wtf(LOGTAG, getPackageName() + " not found", e);
        // }
        // return (flags & android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    }

    // FHR reason code for a session end prior to a restart for a
    // locale change.
    private static final String SESSION_END_LOCALE_CHANGED = "L";

    /**
     * This exists so that a locale can be applied in two places: when saved
     * in a nested activity, and then again when we get back up to GeckoApp.
     *
     * GeckoApp needs to do a bunch more stuff than, say, GeckoPreferences.
     */
    protected void onLocaleChanged(final String locale) {
        final boolean startNewSession = true;
        final boolean shouldRestart = false;

        // If the HealthRecorder is not yet initialized (unlikely), the locale change won't
        // trigger a session transition and subsequent events will be recorded in an environment
        // with the wrong locale.
        final HealthRecorder rec = mHealthRecorder;
        if (rec != null) {
            rec.onAppLocaleChanged(locale);
            rec.onEnvironmentChanged(startNewSession, SESSION_END_LOCALE_CHANGED);
        }

        if (!shouldRestart) {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    GeckoApp.this.onLocaleReady(locale);
                }
            });
            return;
        }

        // Do this in the background so that the health recorder has its
        // time to finish.
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                GeckoApp.this.doRestart();
                GeckoApp.this.finish();
            }
        });
    }

    /**
     * Use BrowserLocaleManager to change our persisted and current locales,
     * and poke HealthRecorder to tell it of our changed state.
     */
    protected void setLocale(final String locale) {
        Log.d(LOGTAG, "setLocale: " + locale);
        if (locale == null) {
            return;
        }

        final String resultant = BrowserLocaleManager.getInstance().setSelectedLocale(this, locale);
        if (resultant == null) {
            return;
        }

        onLocaleChanged(resultant);
    }

    private void setSystemUiVisible(final boolean visible) {
        if (Build.VERSION.SDK_INT < 14) {
            return;
        }

        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                if (visible) {
                    mMainLayout.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
                } else {
                    mMainLayout.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LOW_PROFILE);
                }
            }
        });
    }

    protected HealthRecorder createHealthRecorder(final Context context,
                                                  final String profilePath,
                                                  final EventDispatcher dispatcher,
                                                  final String osLocale,
                                                  final String appLocale,
                                                  final SessionInformation previousSession) {
        // GeckoApp does not need to record any health information - return a stub.
        return new StubbedHealthRecorder();
    }
}

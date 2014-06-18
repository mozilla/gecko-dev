package org.mozilla.gecko.tests;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Actions;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.distribution.Distribution;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.SharedPreferences;

/**
 * Tests distribution customization.
 * mock-package.zip should contain the following directory structure:
 *
 *   distribution/
 *     preferences.json
 *     bookmarks.json
 *     searchplugins/
 *       common/
 *         engine.xml
 */
public class testDistribution extends ContentProviderTest {
    private static final String MOCK_PACKAGE = "mock-package.zip";
    private static final int PREF_REQUEST_ID = 0x7357;

    private Activity mActivity;

    /**
     * This is a hack.
     *
     * Startup results in us writing prefs -- we fetch the Distribution, which
     * caches its state. Our tests try to wipe those prefs, but apparently
     * sometimes race with startup, which leads to us not getting one of our
     * expected messages. The test fails.
     *
     * This hack waits for any existing background tasks -- such as the one that
     * writes prefs -- to finish before we begin the test.
     */
    private void waitForBackgroundHappiness() {
        final Object signal = new Object();
        final Runnable done = new Runnable() {
            @Override
            public void run() {
                synchronized (signal) {
                    signal.notify();
                }
            }
        };
        synchronized (signal) {
            ThreadUtils.postToBackgroundThread(done);
            try {
                signal.wait();
            } catch (InterruptedException e) {
                mAsserter.ok(false, "InterruptedException waiting on background thread.", e.toString());
            }
        }
        mAsserter.dumpLog("Background task completed. Proceeding.");
    }

    public void testDistribution() {
        mActivity = getActivity();

        String mockPackagePath = getMockPackagePath();

        // Wait for any startup-related background distribution shenanigans to
        // finish. This reduces the chance of us racing with startup pref writes.
        waitForBackgroundHappiness();

        // Pre-clear distribution pref, run basic preferences and en-US localized preferences Tests
        clearDistributionPref();
        setTestLocale("en-US");
        initDistribution(mockPackagePath);
        checkPreferences();
        checkLocalizedPreferences("en-US");
        checkSearchPlugin();

        // Pre-clear distribution pref, and run es-MX localized preferences Test
        clearDistributionPref();
        setTestLocale("es-MX");
        initDistribution(mockPackagePath);
        checkLocalizedPreferences("es-MX");
    }

    // Initialize the distribution from the mock package.
    private void initDistribution(String aPackagePath) {
        // Call Distribution.init with the mock package.
        Actions.EventExpecter distributionSetExpecter = mActions.expectGeckoEvent("Distribution:Set:OK");
        Distribution.init(mActivity, aPackagePath, "prefs-" + System.currentTimeMillis());
        distributionSetExpecter.blockForEvent();
        distributionSetExpecter.unregisterListener();
    }

    // Test distribution and preferences values stored in preferences.json
    private void checkPreferences() {
        String prefID = "distribution.id";
        String prefAbout = "distribution.about";
        String prefVersion = "distribution.version";
        String prefTestBoolean = "distribution.test.boolean";
        String prefTestString = "distribution.test.string";
        String prefTestInt = "distribution.test.int";

        try {
            final String[] prefNames = { prefID,
                                         prefAbout,
                                         prefVersion,
                                         prefTestBoolean,
                                         prefTestString,
                                         prefTestInt };

            Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
            mActions.sendPreferencesGetEvent(PREF_REQUEST_ID, prefNames);

            JSONObject data = null;
            int requestId = -1;

            // Wait until we get the correct "Preferences:Data" event
            while (requestId != PREF_REQUEST_ID) {
                data = new JSONObject(eventExpecter.blockForEventData());
                requestId = data.getInt("requestId");
            }
            eventExpecter.unregisterListener();

            JSONArray preferences = data.getJSONArray("preferences");
            for (int i = 0; i < preferences.length(); i++) {
                JSONObject pref = (JSONObject) preferences.get(i);
                String name = pref.getString("name");

                if (name.equals(prefID)) {
                    mAsserter.is(pref.getString("value"), "test-partner", "check " + prefID);
                } else if (name.equals(prefAbout)) {
                    mAsserter.is(pref.getString("value"), "Test Partner", "check " + prefAbout);
                } else if (name.equals(prefVersion)) {
                    mAsserter.is(pref.getInt("value"), 1, "check " + prefVersion);
                } else if (name.equals(prefTestBoolean)) {
                    mAsserter.is(pref.getBoolean("value"), true, "check " + prefTestBoolean);
                } else if (name.equals(prefTestString)) {
                    mAsserter.is(pref.getString("value"), "test", "check " + prefTestString);
                } else if (name.equals(prefTestInt)) {
                    mAsserter.is(pref.getInt("value"), 5, "check " + prefTestInt);
                }
            }

        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting preferences", e.toString());
        }
    }

    private void checkSearchPlugin() {
        Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("SearchEngines:Data");
        mActions.sendGeckoEvent("SearchEngines:GetVisible", null);

        try {
            JSONObject data = new JSONObject(eventExpecter.blockForEventData());
            eventExpecter.unregisterListener();
            JSONArray searchEngines = data.getJSONArray("searchEngines");
            boolean foundEngine = false;
            for (int i = 0; i < searchEngines.length(); i++) {
                JSONObject engine = (JSONObject) searchEngines.get(i);
                String name = engine.getString("name");
                if (name.equals("Test search engine")) {
                    foundEngine = true;
                    break;
                }
            }
            mAsserter.ok(foundEngine, "check search plugin", "found test search plugin");
        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting search plugins", e.toString());
        }
    }

    // Sets the distribution locale preference for the test
    private void setTestLocale(String aLocale) {
        String prefUseragentLocale = "general.useragent.locale";

        JSONObject jsonPref = new JSONObject();
        try {
            // Request the pref change to the locale.
            jsonPref.put("name", prefUseragentLocale);
            jsonPref.put("type", "string");
            jsonPref.put("value", aLocale);
            mActions.sendGeckoEvent("Preferences:Set", jsonPref.toString());

            // Wait for confirmation of the pref change.
            final String[] prefNames = { prefUseragentLocale };

            Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
            mActions.sendPreferencesGetEvent(PREF_REQUEST_ID, prefNames);

            JSONObject data = null;
            int requestId = -1;

            // Wait until we get the correct "Preferences:Data" event
            while (requestId != PREF_REQUEST_ID) {
                data = new JSONObject(eventExpecter.blockForEventData());
                requestId = data.getInt("requestId");
            }
            eventExpecter.unregisterListener();

        } catch (Exception e) {
            mAsserter.ok(false, "exception setting test locale", e.toString());
        }
    }

    // Test localized distribution and preferences values stored in preferences.json
    private void checkLocalizedPreferences(String aLocale) {
        String prefAbout = "distribution.about";
        String prefLocalizeable = "distribution.test.localizeable";
        String prefLocalizeableOverride = "distribution.test.localizeable-override";

        try {
            final String[] prefNames = { prefAbout, prefLocalizeable, prefLocalizeableOverride };

            Actions.RepeatedEventExpecter eventExpecter = mActions.expectGeckoEvent("Preferences:Data");
            mActions.sendPreferencesGetEvent(PREF_REQUEST_ID, prefNames);

            JSONObject data = null;
            int requestId = -1;

            // Wait until we get the correct "Preferences:Data" event
            while (requestId != PREF_REQUEST_ID) {
                data = new JSONObject(eventExpecter.blockForEventData());
                requestId = data.getInt("requestId");
            }
            eventExpecter.unregisterListener();

            JSONArray preferences = data.getJSONArray("preferences");
            for (int i = 0; i < preferences.length(); i++) {
                JSONObject pref = (JSONObject) preferences.get(i);
                String name = pref.getString("name");

                if (name.equals(prefAbout)) {
                    if (aLocale.equals("en-US")) {
                        mAsserter.is(pref.getString("value"), "Test Partner", "check " + prefAbout);
                    } else if (aLocale.equals("es-MX")) {
                        mAsserter.is(pref.getString("value"), "Afiliado de Prueba", "check " + prefAbout);
                    }
                } else if (name.equals(prefLocalizeable)) {
                    if (aLocale.equals("en-US")) {
                        mAsserter.is(pref.getString("value"), "http://test.org/en-US/en-US/", "check " + prefLocalizeable);
                    } else if (aLocale.equals("es-MX")) {
                        mAsserter.is(pref.getString("value"), "http://test.org/es-MX/es-MX/", "check " + prefLocalizeable);
                    }
                } else if (name.equals(prefLocalizeableOverride)) {
                    if (aLocale.equals("en-US")) {
                        mAsserter.is(pref.getString("value"), "http://cheese.com", "check " + prefLocalizeableOverride);
                    } else if (aLocale.equals("es-MX")) {
                        mAsserter.is(pref.getString("value"), "http://test.org/es-MX/", "check " + prefLocalizeableOverride);
                    }
                }
            }

        } catch (JSONException e) {
            mAsserter.ok(false, "exception getting preferences", e.toString());
        }
    }

    // Copies the mock package to the data directory and returns the file path to it.
    private String getMockPackagePath() {
        String mockPackagePath = "";

        try {
            InputStream inStream = getAsset(MOCK_PACKAGE);
            File dataDir = new File(mActivity.getApplicationInfo().dataDir);
            File outFile = new File(dataDir, MOCK_PACKAGE);

            OutputStream outStream = new FileOutputStream(outFile);
            int b;
            while ((b = inStream.read()) != -1) {
                outStream.write(b);
            }
            inStream.close();
            outStream.close();

            mockPackagePath = outFile.getPath();

        } catch (Exception e) {
            mAsserter.ok(false, "exception copying mock distribution package to data directory", e.toString());
        }

        return mockPackagePath;
    }

    // Clears the distribution pref to return distribution state to STATE_UNKNOWN
    private void clearDistributionPref() {
        mAsserter.dumpLog("Clearing distribution pref.");
        SharedPreferences settings = mActivity.getSharedPreferences("GeckoApp", Activity.MODE_PRIVATE);
        String keyName = mActivity.getPackageName() + ".distribution_state";
        settings.edit().remove(keyName).commit();
    }

    @Override
    public void setUp() throws Exception {
        // TODO: Set up the content provider after setting the distribution.
        super.setUp(sBrowserProviderCallable, BrowserContract.AUTHORITY, "browser.db");
    }

    private void delete(File file) throws Exception {
      if (file.isDirectory()) {
        File[] files = file.listFiles();
        for (File f : files) {
          delete(f);
        }
      }
      mAsserter.ok(file.delete(), "clean up distribution files", "deleted " + file.getPath());
    }

    @Override
    public void tearDown() throws Exception {
        File dataDir = new File(mActivity.getApplicationInfo().dataDir);

        // Delete mock package from data directory.
        File mockPackage = new File(dataDir, MOCK_PACKAGE);
        mAsserter.ok(mockPackage.delete(), "clean up mock package", "deleted " + mockPackage.getPath());

        // Recursively delete distribution files that Distribution.init copied to data directory.
        File distDir = new File(dataDir, "distribution");
        delete(distDir);

        clearDistributionPref();

        super.tearDown();
    }
}

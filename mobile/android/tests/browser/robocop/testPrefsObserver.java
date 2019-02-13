/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Actions;

/**
 * Basic test to check bounce-back from overscroll.
 * - Load the page and verify it draws
 * - Drag page downwards by 100 pixels into overscroll, verify it snaps back.
 * - Drag page rightwards by 100 pixels into overscroll, verify it snaps back.
 */
public class testPrefsObserver extends BaseTest {
    private static final String PREF_TEST_PREF = "robocop.tests.dummy";
    private static final int PREF_OBSERVE_REQUEST_ID = 0x7357;
    private static final long PREF_TIMEOUT = 10000;

    private Actions.RepeatedEventExpecter mExpecter;

    public void setPref(boolean value) throws JSONException {
        mAsserter.dumpLog("Setting pref");

        JSONObject jsonPref = new JSONObject();
        jsonPref.put("name", PREF_TEST_PREF);
        jsonPref.put("type", "bool");
        jsonPref.put("value", value);
        mActions.sendGeckoEvent("Preferences:Set", jsonPref.toString());
    }

    public void waitAndCheckPref(boolean value) throws JSONException {
        mAsserter.dumpLog("Waiting to check pref");

        JSONObject data = null;
        int requestId = -1;

        while (requestId != PREF_OBSERVE_REQUEST_ID) {
            data = new JSONObject(mExpecter.blockForEventData());
            if (!mExpecter.eventReceived()) {
                mAsserter.ok(false, "Checking pref is correct value", "Didn't receive pref");
                return;
            }
            requestId = data.getInt("requestId");
        }

        JSONObject pref = data.getJSONArray("preferences").getJSONObject(0);
        mAsserter.is(pref.getString("name"), PREF_TEST_PREF, "Pref name is correct");
        mAsserter.is(pref.getString("type"), "bool", "Pref type is correct");
        mAsserter.is(pref.getBoolean("value"), value, "Pref value is correct");
    }

    public void verifyDisconnect() throws JSONException {
        mAsserter.dumpLog("Checking pref observer is removed");

        JSONObject pref = null;
        int requestId = -1;

        while (requestId != PREF_OBSERVE_REQUEST_ID) {
            String data = mExpecter.blockForEventDataWithTimeout(PREF_TIMEOUT);
            if (data == null) {
                mAsserter.ok(true, "Verifying pref is unobserved", "Didn't get unobserved pref");
                return;
            }
            pref = new JSONObject(data);
            requestId = pref.getInt("requestId");
        }

        mAsserter.ok(false, "Received unobserved pref change", "");
    }

    public void observePref() throws JSONException {
        mAsserter.dumpLog("Setting up pref observer");

        // Setup the pref observer
        mExpecter = mActions.expectGeckoEvent("Preferences:Data");
        mActions.sendPreferencesObserveEvent(PREF_OBSERVE_REQUEST_ID, new String[] { PREF_TEST_PREF });
    }

    public void removePrefObserver() {
        mAsserter.dumpLog("Removing pref observer");

        mActions.sendPreferencesRemoveObserversEvent(PREF_OBSERVE_REQUEST_ID);
    }

    public void testPrefsObserver() {
        blockForGeckoReady();

        try {
            setPref(false);
            observePref();
            waitAndCheckPref(false);

            setPref(true);
            waitAndCheckPref(true);

            removePrefObserver();
            setPref(false);
            verifyDisconnect();
        } catch (Exception ex) {
            mAsserter.ok(false, "exception in testPrefsObserver", ex.toString());
        } finally {
            // Make sure we remove the observer - if it's already removed, this
            // will do nothing.
            removePrefObserver();
        }
        mExpecter.unregisterListener();
    }
}


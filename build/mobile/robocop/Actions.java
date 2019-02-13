/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;
import android.database.Cursor;

public interface Actions {

    /** Special keys supported by sendSpecialKey() */
    public enum SpecialKey {
        DOWN,
        UP,
        LEFT,
        RIGHT,
        ENTER,
        MENU,
        /**
         * @deprecated Use Solo.goBack() in Robocop instead.
         */
        @Deprecated
        BACK
    }

    public interface EventExpecter {
        /** Blocks until the event has been received. Subsequent calls will return immediately. */
        public void blockForEvent();
        public void blockForEvent(long millis, boolean failOnTimeout);

        /** Blocks until the event has been received and returns data associated with the event. */
        public String blockForEventData();

        /**
         * Blocks until the event has been received, or until the timeout has been exceeded.
         * Returns the data associated with the event, if applicable.
         */
        public String blockForEventDataWithTimeout(long millis);

        /** Polls to see if the event has been received. Once this returns true, subsequent calls will also return true. */
        public boolean eventReceived();

        /** Stop listening for events. */
        public void unregisterListener();
    }

    public interface RepeatedEventExpecter extends EventExpecter {
        /** Blocks until at least one event has been received, and no events have been received in the last <code>millis</code> milliseconds. */
        public void blockUntilClear(long millis);
    }

    /**
     * Sends an event to Gecko.
     *
     * @param geckoEvent The geckoEvent JSONObject's type
     */
    void sendGeckoEvent(String geckoEvent, String data);

    /**
     * Sends a preferences get event to Gecko.
     *
     * @param requestId The id of this request.
     * @param prefNames The preferences being requested.
     */
    void sendPreferencesGetEvent(int requestId, String[] prefNames);

    /**
     * Sends a preferences observe event to Gecko.
     *
     * @param requestId The id of this request.
     * @param prefNames The preferences being requested.
     */
    void sendPreferencesObserveEvent(int requestId, String[] prefNames);

    /**
     * Sends a preferences remove observers event to Gecko.
     *
     * @param requestId The id of this request.
     */
    void sendPreferencesRemoveObserversEvent(int requestid);

    /**
     * Listens for a gecko event to be sent from the Gecko instance.
     * The returned object can be used to test if the event has been
     * received. Note that only one event is listened for.
     *
     * @param geckoEvent The geckoEvent JSONObject's type
     */
    RepeatedEventExpecter expectGeckoEvent(String geckoEvent);

    /**
     * Listens for a paint event. Note that calling expectPaint() will
     * invalidate the event expecters returned from any previous calls
     * to expectPaint(); calling any methods on those invalidated objects
     * will result in undefined behaviour.
     */
    RepeatedEventExpecter expectPaint();

    /**
     * Send a string to the application
     *
     * @param keysToSend The string to send
     */
    void sendKeys(String keysToSend);

    /**
     * Send a special keycode to the element
     *
     * @param key The special key to send
     */
    void sendSpecialKey(SpecialKey key);
    void sendKeyCode(int keyCode);

    void drag(int startingX, int endingX, int startingY, int endingY);

    /**
     * Run a sql query on the specified database
     */
    public Cursor querySql(String dbPath, String sql);
}

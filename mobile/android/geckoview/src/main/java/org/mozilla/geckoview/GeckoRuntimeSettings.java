/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.Map;

import android.app.Service;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.util.ArrayMap;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.geckoview.GeckoSession.TrackingProtectionDelegate;

public final class GeckoRuntimeSettings implements Parcelable {

    /**
     * Settings builder used to construct the settings object.
     */
    public static final class Builder {
        private final GeckoRuntimeSettings mSettings;

        public Builder() {
            mSettings = new GeckoRuntimeSettings();
        }

        public Builder(final GeckoRuntimeSettings settings) {
            mSettings = new GeckoRuntimeSettings(settings);
        }

        /**
         * Finalize and return the settings.
         *
         * @return The constructed settings.
         */
        public @NonNull GeckoRuntimeSettings build() {
            return new GeckoRuntimeSettings(mSettings);
        }

        /**
         * Set the content process hint flag.
         *
         * @param use If true, this will reload the content process for future use.
         *            Default is false.
         * @return This Builder instance.

         */
        public @NonNull Builder useContentProcessHint(final boolean use) {
            mSettings.mUseContentProcess = use;
            return this;
        }

        /**
         * Set the custom Gecko process arguments.
         *
         * @param args The Gecko process arguments.
         * @return This Builder instance.
         */
        public @NonNull Builder arguments(final @NonNull String[] args) {
            if (args == null) {
                throw new IllegalArgumentException("Arguments must not  be null");
            }
            mSettings.mArgs = args;
            return this;
        }

        /**
         * Set the custom Gecko intent extras.
         *
         * @param extras The Gecko intent extras.
         * @return This Builder instance.
         */
        public @NonNull Builder extras(final @NonNull Bundle extras) {
            if (extras == null) {
                throw new IllegalArgumentException("Extras must not  be null");
            }
            mSettings.mExtras = extras;
            return this;
        }

        /**
         * Set whether JavaScript support should be enabled.
         *
         * @param flag A flag determining whether JavaScript should be enabled.
         *             Default is true.
         * @return This Builder instance.
         */
        public @NonNull Builder javaScriptEnabled(final boolean flag) {
            mSettings.mJavaScript.set(flag);
            return this;
        }

        /**
         * Set whether remote debugging support should be enabled.
         *
         * @param enabled True if remote debugging should be enabled.
         * @return This Builder instance.
         */
        public @NonNull Builder remoteDebuggingEnabled(final boolean enabled) {
            mSettings.mRemoteDebugging.set(enabled);
            return this;
        }

        /**
         * Set whether support for web fonts should be enabled.
         *
         * @param flag A flag determining whether web fonts should be enabled.
         *             Default is true.
         * @return This Builder instance.
         */
        public @NonNull Builder webFontsEnabled(final boolean flag) {
            mSettings.mWebFonts.set(flag ? 1 : 0);
            return this;
        }

        /**
         * Set whether there should be a pause during startup. This is useful if you need to
         * wait for a debugger to attach.
         *
         * @param enabled A flag determining whether there will be a pause early in startup.
         *                Defaults to false.
         * @return This Builder.
         */
        public @NonNull Builder pauseForDebugger(boolean enabled) {
            mSettings.mDebugPause = enabled;
            return this;
        }
        /**
         * Set whether the to report the full bit depth of the device.
         *
         * By default, 24 bits are reported for high memory devices and 16 bits
         * for low memory devices. If set to true, the device's maximum bit depth is
         * reported. On most modern devices this will be 32 bit screen depth.
         *
         * @param enable A flag determining whether maximum screen depth should be used.
         * @return This Builder.
         */
        public @NonNull Builder useMaxScreenDepth(boolean enable) {
            mSettings.mUseMaxScreenDepth = enable;
            return this;
        }

        /**
         * Set cookie storage behavior.
         *
         * @param behavior The storage behavior that should be applied.
         *                 Use one of the {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
         * @return The Builder instance.
         */
        public @NonNull Builder cookieBehavior(@CookieBehavior int behavior) {
            mSettings.mCookieBehavior.set(behavior);
            return this;
        }

        /**
         * Set the cookie lifetime.
         *
         * @param lifetime The enforced cookie lifetime.
         *                 Use one of the {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
         * @return The Builder instance.
         */
        public @NonNull Builder cookieLifetime(@CookieLifetime int lifetime) {
            mSettings.mCookieLifetime.set(lifetime);
            return this;
        }

        /**
         * Set tracking protection blocking categories.
         *
         * @param categories The categories of trackers that should be blocked.
         *                   Use one or more of the
         *                   {@link TrackingProtectionDelegate#CATEGORY_AD TrackingProtectionDelegate.CATEGORY_*} flags.
         * @return This Builder instance.
         **/
        public @NonNull Builder trackingProtectionCategories(
                @TrackingProtectionDelegate.Category int categories) {
            mSettings.mTrackingProtection
                     .set(TrackingProtection.buildPrefValue(categories));
            return this;
        }

        /**
         * Set whether or not web console messages should go to logcat.
         *
         * Note: If enabled, Gecko performance may be negatively impacted if
         * content makes heavy use of the console API.
         *
         * @param enabled A flag determining whether or not web console messages should be
         *                printed to logcat.
         * @return The builder instance.
         */
        public @NonNull Builder consoleOutput(boolean enabled) {
            mSettings.mConsoleOutput.set(enabled);
            return this;
        }

        /**
         * Set the display density override.
         *
         * @param density The display density value to use for overriding the system default.
         * @return The builder instance.
         */
        public @NonNull Builder displayDensityOverride(float density) {
            mSettings.mDisplayDensityOverride = density;
            return this;
        }

        /** Set whether or not known malware sites should be blocked.
         *
         * Note: For each blocked site, {@link GeckoSession.NavigationDelegate#onLoadError}
         * with error category {@link WebRequestError#ERROR_CATEGORY_SAFEBROWSING}
         * is called.
         *
         * @param enabled A flag determining whether or not to block malware
         *                sites.
         * @return The builder instance.
         */
        public @NonNull Builder blockMalware(boolean enabled) {
            mSettings.mSafebrowsingMalware.set(enabled);
            return this;
        }

        /**
         * Set whether or not known phishing sites should be blocked.
         *
         * Note: For each blocked site, {@link GeckoSession.NavigationDelegate#onLoadError}
         * with error category {@link WebRequestError#ERROR_CATEGORY_SAFEBROWSING}
         * is called.
         *
         * @param enabled A flag determining whether or not to block phishing
         *                sites.
         * @return The builder instance.
         */
        public @NonNull Builder blockPhishing(boolean enabled) {
            mSettings.mSafebrowsingPhishing.set(enabled);
            return this;
        }

        /**
         * Set the display DPI override.
         *
         * @param dpi The display DPI value to use for overriding the system default.
         * @return The builder instance.
         */
        public @NonNull Builder displayDpiOverride(int dpi) {
            mSettings.mDisplayDpiOverride = dpi;
            return this;
        }

        /**
         * Set the screen size override.
         *
         * @param width The screen width value to use for overriding the system default.
         * @param height The screen height value to use for overriding the system default.
         * @return The builder instance.
         */
        public @NonNull Builder screenSizeOverride(int width, int height) {
            mSettings.mScreenWidthOverride = width;
            mSettings.mScreenHeightOverride = height;
            return this;
        }

        /**
         * When set, the specified {@link android.app.Service} will be started by
         * an {@link android.content.Intent} with action {@link GeckoRuntime#ACTION_CRASHED} when
         * a crash is encountered. Crash details can be found in the Intent extras, such as
         * {@link GeckoRuntime#EXTRA_MINIDUMP_PATH}.
         * <br><br>
         * The crash handler Service must be declared to run in a different process from
         * the {@link GeckoRuntime}. Additionally, the handler will be run as a foreground service,
         * so the normal rules about activating a foreground service apply.
         * <br><br>
         * In practice, you have one of three
         * options once the crash handler is started:
         * <ul>
         * <li>Call {@link android.app.Service#startForeground(int, android.app.Notification)}. You can then
         * take as much time as necessary to report the crash.</li>
         * <li>Start an activity. Unless you also call {@link android.app.Service#startForeground(int, android.app.Notification)}
         * this should be in a different process from the crash handler, since Android will
         * kill the crash handler process as part of the background execution limitations.</li>
         * <li>Schedule work via {@link android.app.job.JobScheduler}. This will allow you to
         * do substantial work in the background without execution limits.</li>
         * </ul><br>
         * You can use {@link CrashReporter} to send the report to Mozilla, which provides Mozilla
         * with data needed to fix the crash. Be aware that the minidump may contain
         * personally identifiable information (PII). Consult Mozilla's
         * <a href="https://www.mozilla.org/en-US/privacy/">privacy policy</a> for information
         * on how this data will be handled.
         *
         * @param handler The class for the crash handler Service.
         * @return This builder instance.
         *
         * @see <a href="https://developer.android.com/about/versions/oreo/background">Android Background Execution Limits</a>
         * @see GeckoRuntime#ACTION_CRASHED
         */
        public @NonNull Builder crashHandler(final Class<? extends Service> handler) {
            mSettings.mCrashHandler = handler;
            return this;
        }

        /**
         * Set the locale.
         *
         * @param requestedLocales List of locale codes in Gecko format ("en" or "en-US").
         * @return The builder instance.
         */
        public @NonNull Builder locales(String[] requestedLocales) {
            mSettings.mRequestedLocales = requestedLocales;
            return this;
        }
    }

    /* package */ GeckoRuntime runtime;
    /* package */ boolean mUseContentProcess;
    /* package */ String[] mArgs;
    /* package */ Bundle mExtras;
    /* package */ int prefCount;

    private class Pref<T> {
        public final String name;
        public final T defaultValue;
        private T mValue;
        private boolean mIsSet;

        public Pref(final String name, final T defaultValue) {
            GeckoRuntimeSettings.this.prefCount++;

            this.name = name;
            this.defaultValue = defaultValue;
            mValue = defaultValue;
        }

        public void set(T newValue) {
            mValue = newValue;
            mIsSet = true;

            // There is a flush() in GeckoRuntimeSettings, so be explicit.
            this.flush();
        }

        public T get() {
            return mValue;
        }

        private void flush() {
            final GeckoRuntime runtime = GeckoRuntimeSettings.this.runtime;
            if (runtime != null) {
                final GeckoBundle prefs = new GeckoBundle(1);
                intoBundle(prefs);
                runtime.setDefaultPrefs(prefs);
            }
        }

        public void intoBundle(final GeckoBundle bundle) {
            final T value = mIsSet ? mValue : defaultValue;
            if (value instanceof String) {
                bundle.putString(name, (String)value);
            } else if (value instanceof Integer) {
                bundle.putInt(name, (Integer)value);
            } else if (value instanceof Boolean) {
                bundle.putBoolean(name, (Boolean)value);
            } else {
                throw new UnsupportedOperationException("Unhandled pref type for " + name);
            }
        }
    }

    /* package */ Pref<Boolean> mJavaScript = new Pref<Boolean>(
        "javascript.enabled", true);
    /* package */ Pref<Boolean> mRemoteDebugging = new Pref<Boolean>(
        "devtools.debugger.remote-enabled", false);
    /* package */ Pref<Integer> mWebFonts = new Pref<Integer>(
        "browser.display.use_document_fonts", 1);
    /* package */ Pref<Integer> mCookieBehavior = new Pref<Integer>(
        "network.cookie.cookieBehavior", COOKIE_ACCEPT_ALL);
    /* package */ Pref<Integer> mCookieLifetime = new Pref<Integer>(
        "network.cookie.lifetimePolicy", COOKIE_LIFETIME_NORMAL);
    /* package */ Pref<String> mTrackingProtection = new Pref<String>(
        "urlclassifier.trackingTable",
        TrackingProtection.buildPrefValue(
            TrackingProtectionDelegate.CATEGORY_TEST |
            TrackingProtectionDelegate.CATEGORY_ANALYTIC |
            TrackingProtectionDelegate.CATEGORY_SOCIAL |
            TrackingProtectionDelegate.CATEGORY_AD));
    /* package */ Pref<Boolean> mConsoleOutput = new Pref<Boolean>(
        "geckoview.console.enabled", false);
    /* package */ Pref<Boolean> mSafebrowsingMalware = new Pref<Boolean>(
        "browser.safebrowsing.malware.enabled", true);
    /* package */ Pref<Boolean> mSafebrowsingPhishing = new Pref<Boolean>(
        "browser.safebrowsing.phishing.enabled", true);

    /* package */ boolean mDebugPause;
    /* package */ boolean mUseMaxScreenDepth;
    /* package */ float mDisplayDensityOverride = -1.0f;
    /* package */ int mDisplayDpiOverride;
    /* package */ int mScreenWidthOverride;
    /* package */ int mScreenHeightOverride;
    /* package */ Class<? extends Service> mCrashHandler;
    /* package */ String[] mRequestedLocales;

    private final Pref<?>[] mPrefs = new Pref<?>[] {
        mCookieBehavior, mCookieLifetime, mConsoleOutput,
        mJavaScript, mRemoteDebugging, mSafebrowsingMalware,
        mSafebrowsingPhishing, mTrackingProtection, mWebFonts,
    };

    /* package */ GeckoRuntimeSettings() {
        this(null);
    }

    /* package */ GeckoRuntimeSettings(final @Nullable GeckoRuntimeSettings settings) {
        if (BuildConfig.DEBUG && prefCount != mPrefs.length) {
            throw new AssertionError("Add new pref to prefs list");
        }

        if (settings == null) {
            mArgs = new String[0];
            mExtras = new Bundle();
            return;
        }

        mUseContentProcess = settings.getUseContentProcessHint();
        mArgs = settings.getArguments().clone();
        mExtras = new Bundle(settings.getExtras());

        for (int i = 0; i < mPrefs.length; i++) {
            if (!settings.mPrefs[i].mIsSet) {
                continue;
            }
            // We know this is safe.
            @SuppressWarnings("unchecked")
            final Pref<Object> uncheckedPref = (Pref<Object>) mPrefs[i];
            uncheckedPref.set(settings.mPrefs[i].get());
        }

        mDebugPause = settings.mDebugPause;
        mUseMaxScreenDepth = settings.mUseMaxScreenDepth;
        mDisplayDensityOverride = settings.mDisplayDensityOverride;
        mDisplayDpiOverride = settings.mDisplayDpiOverride;
        mScreenWidthOverride = settings.mScreenWidthOverride;
        mScreenHeightOverride = settings.mScreenHeightOverride;
        mCrashHandler = settings.mCrashHandler;
        mRequestedLocales = settings.mRequestedLocales;
    }

    /* package */ Map<String, Object> getPrefsMap() {
        final ArrayMap<String, Object> prefs = new ArrayMap<>(mPrefs.length);
        for (final Pref<?> pref : mPrefs) {
            prefs.put(pref.name, pref.get());
        }

        return Collections.unmodifiableMap(prefs);
    }

    /* package */ void flush() {
        flushLocales();

        // Prefs are flushed individually when they are set, and
        // initial values are handled by GeckoRuntime itself.
        // We may have user prefs due to previous versions of
        // this class operating differently, though, so we'll
        // send a message to clear any user prefs that may have
        // been set on the prefs we manage.
        final String[] names = new String[mPrefs.length];
        for (int i = 0; i < mPrefs.length; i++) {
            names[i] = mPrefs[i].name;
        }

        final GeckoBundle data = new GeckoBundle(1);
        data.putStringArray("names", names);
        EventDispatcher.getInstance().dispatch("GeckoView:ResetUserPrefs", data);
    }

    /**
     * Get the content process hint flag.
     *
     * @return The content process hint flag.
     */
    public boolean getUseContentProcessHint() {
        return mUseContentProcess;
    }

    /**
     * Get the custom Gecko process arguments.
     *
     * @return The Gecko process arguments.
     */
    public String[] getArguments() {
        return mArgs;
    }

    /**
     * Get the custom Gecko intent extras.
     *
     * @return The Gecko intent extras.
     */
    public Bundle getExtras() {
        return mExtras;
    }

    /**
     * Get whether JavaScript support is enabled.
     *
     * @return Whether JavaScript support is enabled.
     */
    public boolean getJavaScriptEnabled() {
        return mJavaScript.get();
    }

    /**
     * Set whether JavaScript support should be enabled.
     *
     * @param flag A flag determining whether JavaScript should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setJavaScriptEnabled(final boolean flag) {
        mJavaScript.set(flag);
        return this;
    }

    /**
     * Get whether remote debugging support is enabled.
     *
     * @return True if remote debugging support is enabled.
     */
    public boolean getRemoteDebuggingEnabled() {
        return mRemoteDebugging.get();
    }

    /**
     * Set whether remote debugging support should be enabled.
     *
     * @param enabled True if remote debugging should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setRemoteDebuggingEnabled(final boolean enabled) {
        mRemoteDebugging.set(enabled);
        return this;
    }

    /**
     * Get whether web fonts support is enabled.
     *
     * @return Whether web fonts support is enabled.
     */
    public boolean getWebFontsEnabled() {
        return mWebFonts.get() != 0 ? true : false;
    }

    /**
     * Set whether support for web fonts should be enabled.
     *
     * @param flag A flag determining whether web fonts should be enabled.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setWebFontsEnabled(final boolean flag) {
        mWebFonts.set(flag ? 1 : 0);
        return this;
    }

    /**
     * Gets whether the pause-for-debugger is enabled or not.
     *
     * @return True if the pause is enabled.
     */
    public boolean getPauseForDebuggerEnabled() { return mDebugPause; }

    /**
     * Gets whether the compositor should use the maximum screen depth when rendering.
     *
     * @return True if the maximum screen depth should be used.
     */
    public boolean getUseMaxScreenDepth() { return mUseMaxScreenDepth; }

    /**
     * Gets the display density override value.
     *
     * @return Returns a positive number. Will return null if not set.
     */
    public Float getDisplayDensityOverride() {
        if (mDisplayDensityOverride > 0.0f) {
            return mDisplayDensityOverride;
        }
        return null;
    }

    /**
     * Gets the display DPI override value.
     *
     * @return Returns a positive number. Will return null if not set.
     */
    public Integer getDisplayDpiOverride() {
        if (mDisplayDpiOverride > 0) {
            return mDisplayDpiOverride;
        }
        return null;
    }

    public Class<? extends Service> getCrashHandler() {
        return mCrashHandler;
    }

    /**
     * Gets the screen size  override value.
     *
     * @return Returns a Rect containing the dimensions to use for the window size.
     * Will return null if not set.
     */
    public Rect getScreenSizeOverride() {
        if ((mScreenWidthOverride > 0) && (mScreenHeightOverride > 0)) {
            return new Rect(0, 0, mScreenWidthOverride, mScreenHeightOverride);
        }
        return null;
    }

    /**
     * Gets the list of requested locales.
     *
     * @return A list of locale codes in Gecko format ("en" or "en-US").
     */
    public String[] getLocales() {
        return mRequestedLocales;
    }

    /**
     * Set the locale.
     *
     * @param requestedLocales An ordered list of locales in Gecko format ("en-US").
     */
    public void setLocales(String[] requestedLocales) {
        mRequestedLocales = requestedLocales;
        flushLocales();
    }

    private void flushLocales() {
        if (mRequestedLocales == null) {
            return;
        }
        final GeckoBundle data = new GeckoBundle(1);
        data.putStringArray("requestedLocales", mRequestedLocales);
        EventDispatcher.getInstance().dispatch("GeckoView:SetLocale", data);
    }

    // Sync values with nsICookieService.idl.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ COOKIE_ACCEPT_ALL, COOKIE_ACCEPT_FIRST_PARTY,
              COOKIE_ACCEPT_NONE, COOKIE_ACCEPT_VISITED,
              COOKIE_ACCEPT_NON_TRACKERS })
    /* package */ @interface CookieBehavior {}

    /**
     * Accept first-party and third-party cookies and site data.
     */
    public static final int COOKIE_ACCEPT_ALL = 0;
    /**
     * Accept only first-party cookies and site data to block cookies which are
     * not associated with the domain of the visited site.
     */
    public static final int COOKIE_ACCEPT_FIRST_PARTY = 1;
    /**
     * Do not store any cookies and site data.
     */
    public static final int COOKIE_ACCEPT_NONE = 2;
    /**
     * Accept first-party and third-party cookies and site data only from
     * sites previously visited in a first-party context.
     */
    public static final int COOKIE_ACCEPT_VISITED = 3;
    /**
     * Accept only first-party and non-tracking third-party cookies and site data
     * to block cookies which are not associated with the domain of the visited
     * site set by known trackers.
     */
    public static final int COOKIE_ACCEPT_NON_TRACKERS = 4;

    /**
     * Get the assigned cookie storage behavior.
     *
     * @return The assigned behavior, as one of {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
     */
    public @CookieBehavior int getCookieBehavior() {
        return mCookieBehavior.get();
    }

    /**
     * Set cookie storage behavior.
     *
     * @param behavior The storage behavior that should be applied.
     *                 Use one of the {@link #COOKIE_ACCEPT_ALL COOKIE_ACCEPT_*} flags.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setCookieBehavior(
            @CookieBehavior int behavior) {
        mCookieBehavior.set(behavior);
        return this;
    }

    // Sync values with nsICookieService.idl.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ COOKIE_LIFETIME_NORMAL, COOKIE_LIFETIME_RUNTIME,
              COOKIE_LIFETIME_DAYS })
    /* package */ @interface CookieLifetime {}

    /**
     * Accept default cookie lifetime.
     */
    public static final int COOKIE_LIFETIME_NORMAL = 0;
    /**
     * Downgrade cookie lifetime to this runtime's lifetime.
     */
    public static final int COOKIE_LIFETIME_RUNTIME = 2;
    /**
     * Limit cookie lifetime to N days.
     * Defaults to 90 days.
     */
    public static final int COOKIE_LIFETIME_DAYS = 3;

    /**
     * Get the assigned cookie lifetime.
     *
     * @return The assigned lifetime, as one of {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
     */
    public @CookieBehavior int getCookieLifetime() {
        return mCookieLifetime.get();
    }

    /**
     * Set the cookie lifetime.
     *
     * @param lifetime The enforced cookie lifetime.
     *                 Use one of the {@link #COOKIE_LIFETIME_NORMAL COOKIE_LIFETIME_*} flags.
     * @return This GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setCookieLifetime(
            @CookieLifetime int lifetime) {
        mCookieLifetime.set(lifetime);
        return this;
    }

    /**
     * Get the set tracking protection blocking categories.
     *
     * @return categories The categories of trackers that are set to be blocked.
     *                    Use one or more of the
     *                    {@link TrackingProtectionDelegate#CATEGORY_AD TrackingProtectionDelegate.CATEGORY_*} flags.
     **/
    public @TrackingProtectionDelegate.Category int getTrackingProtectionCategories() {
        return TrackingProtection.listToCategory(mTrackingProtection.get());
    }

    /**
     * Set tracking protection blocking categories.
     *
     * @param categories The categories of trackers that should be blocked.
     *                   Use one or more of the
     *                   {@link TrackingProtectionDelegate#CATEGORY_AD TrackingProtectionDelegate.CATEGORY_*} flags.
     * @return This GeckoRuntimeSettings instance.
     **/
    public @NonNull GeckoRuntimeSettings setTrackingProtectionCategories(
            @TrackingProtectionDelegate.Category int categories) {
        mTrackingProtection.set(TrackingProtection.buildPrefValue(categories));
        return this;
    }

    /**
     * Set whether or not web console messages should go to logcat.
     *
     * Note: If enabled, Gecko performance may be negatively impacted if
     * content makes heavy use of the console API.
     *
     * @param enabled A flag determining whether or not web console messages should be
     *                printed to logcat.
     * @return This GeckoRuntimeSettings instance.
     */

    public @NonNull GeckoRuntimeSettings setConsoleOutputEnabled(boolean enabled) {
        mConsoleOutput.set(enabled);
        return this;
    }

    /**
     * Get whether or not web console messages are sent to logcat.
     *
     * @return True if console output is enabled.
     */
    public boolean getConsoleOutputEnabled() {
        return mConsoleOutput.get();
    }

    /**
     * Set whether or not known malware sites should be blocked.
     *
     * Note: For each blocked site, {@link GeckoSession.NavigationDelegate#onLoadError}
     * with error category {@link WebRequestError#ERROR_CATEGORY_SAFEBROWSING}
     * is called.
     *
     * @param enabled A flag determining whether or not to block malware sites.
     * @return The GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setBlockMalware(boolean enabled) {
        mSafebrowsingMalware.set(enabled);
        return this;
    }

    /**
     * Get whether or not known malware sites are blocked.
     *
     * @return True if malware site blocking is enabled.
     */
    public boolean getBlockMalware() {
        return mSafebrowsingMalware.get();
    }

    /**
     * Set whether or not known phishing sites should be blocked.
     *
     * Note: For each blocked site, {@link GeckoSession.NavigationDelegate#onLoadError}
     * with error category {@link WebRequestError#ERROR_CATEGORY_SAFEBROWSING}
     * is called.
     *
     * @param enabled A flag determining whether or not to block phishing sites.
     * @return The GeckoRuntimeSettings instance.
     */
    public @NonNull GeckoRuntimeSettings setBlockPhishing(boolean enabled) {
        mSafebrowsingPhishing.set(enabled);
        return this;
    }

    /**
     * Get whether or not known phishing sites are blocked.
     *
     * @return True if phishing site blocking is enabled.
     */
    public boolean getBlockPhishing() {
        return mSafebrowsingPhishing.get();
    }

    @Override // Parcelable
    public int describeContents() {
        return 0;
    }

    @Override // Parcelable
    public void writeToParcel(Parcel out, int flags) {
        ParcelableUtils.writeBoolean(out, mUseContentProcess);
        out.writeStringArray(mArgs);
        mExtras.writeToParcel(out, flags);

        for (final Pref<?> pref : mPrefs) {
            out.writeValue(pref.get());
        }

        ParcelableUtils.writeBoolean(out, mDebugPause);
        ParcelableUtils.writeBoolean(out, mUseMaxScreenDepth);
        out.writeFloat(mDisplayDensityOverride);
        out.writeInt(mDisplayDpiOverride);
        out.writeInt(mScreenWidthOverride);
        out.writeInt(mScreenHeightOverride);
        out.writeString(mCrashHandler != null ? mCrashHandler.getName() : null);
        out.writeStringArray(mRequestedLocales);
    }

    // AIDL code may call readFromParcel even though it's not part of Parcelable.
    public void readFromParcel(final Parcel source) {
        mUseContentProcess = ParcelableUtils.readBoolean(source);
        mArgs = source.createStringArray();
        mExtras.readFromParcel(source);

        for (final Pref<?> pref : mPrefs) {
            // We know this is safe.
            @SuppressWarnings("unchecked")
            final Pref<Object> uncheckedPref = (Pref<Object>) pref;
            uncheckedPref.set(source.readValue(getClass().getClassLoader()));
        }

        mDebugPause = ParcelableUtils.readBoolean(source);
        mUseMaxScreenDepth = ParcelableUtils.readBoolean(source);
        mDisplayDensityOverride = source.readFloat();
        mDisplayDpiOverride = source.readInt();
        mScreenWidthOverride = source.readInt();
        mScreenHeightOverride = source.readInt();

        final String crashHandlerName = source.readString();
        if (crashHandlerName != null) {
            try {
                @SuppressWarnings("unchecked")
                final Class<? extends Service> handler =
                        (Class<? extends Service>) Class.forName(crashHandlerName);

                mCrashHandler = handler;
            } catch (ClassNotFoundException e) {
            }
        }

        mRequestedLocales = source.createStringArray();
    }

    public static final Parcelable.Creator<GeckoRuntimeSettings> CREATOR
        = new Parcelable.Creator<GeckoRuntimeSettings>() {
        @Override
        public GeckoRuntimeSettings createFromParcel(final Parcel in) {
            final GeckoRuntimeSettings settings = new GeckoRuntimeSettings();
            settings.readFromParcel(in);
            return settings;
        }

        @Override
        public GeckoRuntimeSettings[] newArray(final int size) {
            return new GeckoRuntimeSettings[size];
        }
    };
}

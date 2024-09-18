/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.helpers

import android.content.Intent
import android.util.Log
import android.view.ViewConfiguration.getLongPressTimeout
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.espresso.intent.rule.IntentsTestRule
import androidx.test.rule.ActivityTestRule
import androidx.test.uiautomator.UiSelector
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.junit.rules.TestRule
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.FeatureSettingsHelper.Companion.settings
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.onboarding.FenixOnboarding
import org.mozilla.fenix.settings.PhoneFeature

typealias HomeActivityComposeTestRule = AndroidComposeTestRule<out TestRule, HomeActivity>

/**
 * A [org.junit.Rule] to handle shared test set up for tests on [HomeActivity].
 *
 * @param initialTouchMode See [ActivityTestRule]
 * @param launchActivity See [ActivityTestRule]
 */

class HomeActivityTestRule(
    initialTouchMode: Boolean = false,
    launchActivity: Boolean = true,
    private val skipOnboarding: Boolean = false,
) : ActivityTestRule<HomeActivity>(HomeActivity::class.java, initialTouchMode, launchActivity),
    FeatureSettingsHelper by FeatureSettingsHelperDelegate() {

    // Using a secondary constructor allows us to easily delegate the settings to FeatureSettingsHelperDelegate.
    // Otherwise if wanting to use the same names we would have to override these settings in the primary
    // constructor and in that elide the FeatureSettingsHelperDelegate.
    constructor(
        initialTouchMode: Boolean = false,
        launchActivity: Boolean = true,
        skipOnboarding: Boolean = false,
        isHomeOnboardingDialogEnabled: Boolean = settings.showHomeOnboardingDialog &&
            FenixOnboarding(appContext).userHasBeenOnboarded(),
        isPocketEnabled: Boolean = settings.showPocketRecommendationsFeature,
        isJumpBackInCFREnabled: Boolean = settings.shouldShowJumpBackInCFR,
        isNavigationBarCFREnabled: Boolean = settings.shouldShowNavigationBarCFR,
        isRecentTabsFeatureEnabled: Boolean = settings.showRecentTabsFeature,
        isRecentlyVisitedFeatureEnabled: Boolean = settings.historyMetadataUIFeature,
        isPWAsPromptEnabled: Boolean = !settings.userKnowsAboutPwas,
        isTCPCFREnabled: Boolean = settings.shouldShowTotalCookieProtectionCFR,
        isWallpaperOnboardingEnabled: Boolean = settings.showWallpaperOnboarding,
        isDeleteSitePermissionsEnabled: Boolean = settings.deleteSitePermissions,
        isOpenInAppBannerEnabled: Boolean = settings.shouldShowOpenInAppBanner,
        etpPolicy: ETPPolicy = getETPPolicy(settings),
        composeTopSitesEnabled: Boolean = false,
        isLocationPermissionEnabled: SitePermissionsRules.Action = getFeaturePermission(PhoneFeature.LOCATION, settings),
        isNavigationToolbarEnabled: Boolean = false,
        isMicrosurveyEnabled: Boolean = settings.microsurveyFeatureEnabled,
        isSetAsDefaultBrowserPromptEnabled: Boolean = settings.setAsDefaultBrowserPromptForExistingUsersEnabled,
        shouldUseBottomToolbar: Boolean = settings.shouldUseBottomToolbar,
    ) : this(initialTouchMode, launchActivity, skipOnboarding) {
        this.isHomeOnboardingDialogEnabled = isHomeOnboardingDialogEnabled
        this.isPocketEnabled = isPocketEnabled
        this.isJumpBackInCFREnabled = isJumpBackInCFREnabled
        this.isNavigationBarCFREnabled = isNavigationBarCFREnabled
        this.isRecentTabsFeatureEnabled = isRecentTabsFeatureEnabled
        this.isRecentlyVisitedFeatureEnabled = isRecentlyVisitedFeatureEnabled
        this.isPWAsPromptEnabled = isPWAsPromptEnabled
        this.isTCPCFREnabled = isTCPCFREnabled
        this.isWallpaperOnboardingEnabled = isWallpaperOnboardingEnabled
        this.isDeleteSitePermissionsEnabled = isDeleteSitePermissionsEnabled
        this.isOpenInAppBannerEnabled = isOpenInAppBannerEnabled
        this.etpPolicy = etpPolicy
        this.composeTopSitesEnabled = composeTopSitesEnabled
        this.isLocationPermissionEnabled = isLocationPermissionEnabled
        this.isNavigationToolbarEnabled = isNavigationToolbarEnabled
        this.isMicrosurveyEnabled = isMicrosurveyEnabled
        this.isSetAsDefaultBrowserPromptEnabled = isSetAsDefaultBrowserPromptEnabled
        this.shouldUseBottomToolbar = shouldUseBottomToolbar
    }

    /**
     * Update settings after the activity was created.
     */
    fun applySettingsExceptions(settings: (FeatureSettingsHelper) -> Unit) {
        Log.i(TAG, "applySettingsExceptions: Trying to update the settings after the activity was created")
        FeatureSettingsHelperDelegate().also {
            settings(this)
            applyFlagUpdates()
        }
        Log.i(TAG, "applySettingsExceptions: Updated the settings after the activity was created")
    }

    private val longTapUserPreference = getLongPressTimeout()

    override fun beforeActivityLaunched() {
        super.beforeActivityLaunched()
        setLongTapTimeout(3000)
        Log.i(TAG, "beforeActivityLaunched: Trying to apply the feature flags updates")
        applyFlagUpdates()
        Log.i(TAG, "beforeActivityLaunched: Successfully applied the feature flag updates")
        if (skipOnboarding) { skipOnboardingBeforeLaunch() }
    }

    override fun afterActivityFinished() {
        super.afterActivityFinished()
        setLongTapTimeout(longTapUserPreference)
        Log.i(TAG, "afterActivityFinished: Trying to reset all feature flags")
        resetAllFeatureFlags()
        Log.i(TAG, "afterActivityFinished: Successfully performed the reset of all feature flags")
        closeNotificationShade()
    }

    companion object {
        /**
         * Create a new instance of [HomeActivityTestRule] which by default will disable specific
         * app features that would otherwise negatively impact most tests.
         *
         * The disabled features are:
         *  - the Jump back in CFR,
         *  - the Total Cookie Protection CFR,
         *  - the PWA prompt dialog,
         *  - the wallpaper onboarding.
         */
        fun withDefaultSettingsOverrides(
            initialTouchMode: Boolean = false,
            launchActivity: Boolean = true,
            skipOnboarding: Boolean = false,
            composeTopSitesEnabled: Boolean = false,
        ) = HomeActivityTestRule(
            initialTouchMode = initialTouchMode,
            launchActivity = launchActivity,
            skipOnboarding = skipOnboarding,
            isJumpBackInCFREnabled = false,
            isPWAsPromptEnabled = false,
            isTCPCFREnabled = false,
            isWallpaperOnboardingEnabled = false,
            isOpenInAppBannerEnabled = false,
            composeTopSitesEnabled = composeTopSitesEnabled,
            isMicrosurveyEnabled = false,
            isSetAsDefaultBrowserPromptEnabled = false,
            // workaround for toolbar at top position by default
            // remove with https://bugzilla.mozilla.org/show_bug.cgi?id=1917640
            shouldUseBottomToolbar = true,
        )
    }
}

/**
 * A [org.junit.Rule] to handle shared test set up for tests on [HomeActivity]. This adds
 * functionality for using the Espresso-intents api, and extends from ActivityTestRule.
 *
 * @param initialTouchMode See [IntentsTestRule]
 * @param launchActivity See [IntentsTestRule]
 */

class HomeActivityIntentTestRule internal constructor(
    initialTouchMode: Boolean = false,
    launchActivity: Boolean = true,
    private val skipOnboarding: Boolean = false,
) : IntentsTestRule<HomeActivity>(HomeActivity::class.java, initialTouchMode, launchActivity),
    FeatureSettingsHelper by FeatureSettingsHelperDelegate() {
    // Using a secondary constructor allows us to easily delegate the settings to FeatureSettingsHelperDelegate.
    // Otherwise if wanting to use the same names we would have to override these settings in the primary
    // constructor and in that elide the FeatureSettingsHelperDelegate.
    constructor(
        initialTouchMode: Boolean = false,
        launchActivity: Boolean = true,
        skipOnboarding: Boolean = false,
        isHomeOnboardingDialogEnabled: Boolean = settings.showHomeOnboardingDialog &&
            FenixOnboarding(appContext).userHasBeenOnboarded(),
        isPocketEnabled: Boolean = settings.showPocketRecommendationsFeature,
        isJumpBackInCFREnabled: Boolean = settings.shouldShowJumpBackInCFR,
        isNavigationBarCFREnabled: Boolean = settings.shouldShowNavigationBarCFR,
        isRecentTabsFeatureEnabled: Boolean = settings.showRecentTabsFeature,
        isRecentlyVisitedFeatureEnabled: Boolean = settings.historyMetadataUIFeature,
        isPWAsPromptEnabled: Boolean = !settings.userKnowsAboutPwas,
        isTCPCFREnabled: Boolean = settings.shouldShowTotalCookieProtectionCFR,
        isWallpaperOnboardingEnabled: Boolean = settings.showWallpaperOnboarding,
        isDeleteSitePermissionsEnabled: Boolean = settings.deleteSitePermissions,
        isOpenInAppBannerEnabled: Boolean = settings.shouldShowOpenInAppBanner,
        etpPolicy: ETPPolicy = getETPPolicy(settings),
        composeTopSitesEnabled: Boolean = false,
        isLocationPermissionEnabled: SitePermissionsRules.Action = getFeaturePermission(PhoneFeature.LOCATION, settings),
        isNavigationToolbarEnabled: Boolean = false,
        isMicrosurveyEnabled: Boolean = settings.microsurveyFeatureEnabled,
        isSetAsDefaultBrowserPromptEnabled: Boolean = settings.setAsDefaultBrowserPromptForExistingUsersEnabled,
        shouldUseBottomToolbar: Boolean = settings.shouldUseBottomToolbar,
    ) : this(initialTouchMode, launchActivity, skipOnboarding) {
        this.isHomeOnboardingDialogEnabled = isHomeOnboardingDialogEnabled
        this.isPocketEnabled = isPocketEnabled
        this.isJumpBackInCFREnabled = isJumpBackInCFREnabled
        this.isNavigationBarCFREnabled = isNavigationBarCFREnabled
        this.isRecentTabsFeatureEnabled = isRecentTabsFeatureEnabled
        this.isRecentlyVisitedFeatureEnabled = isRecentlyVisitedFeatureEnabled
        this.isPWAsPromptEnabled = isPWAsPromptEnabled
        this.isTCPCFREnabled = isTCPCFREnabled
        this.isWallpaperOnboardingEnabled = isWallpaperOnboardingEnabled
        this.isDeleteSitePermissionsEnabled = isDeleteSitePermissionsEnabled
        this.isOpenInAppBannerEnabled = isOpenInAppBannerEnabled
        this.etpPolicy = etpPolicy
        this.composeTopSitesEnabled = composeTopSitesEnabled
        this.isLocationPermissionEnabled = isLocationPermissionEnabled
        this.isNavigationToolbarEnabled = isNavigationToolbarEnabled
        this.isMicrosurveyEnabled = isMicrosurveyEnabled
        this.isSetAsDefaultBrowserPromptEnabled = isSetAsDefaultBrowserPromptEnabled
        this.shouldUseBottomToolbar = shouldUseBottomToolbar
    }

    private val longTapUserPreference = getLongPressTimeout()

    private lateinit var intent: Intent

    /**
     * Update settings after the activity was created.
     */
    fun applySettingsExceptions(settings: (FeatureSettingsHelper) -> Unit) {
        Log.i(TAG, "applySettingsExceptions: Trying to update the settings after the activity was created")
        FeatureSettingsHelperDelegate().apply {
            settings(this)
            applyFlagUpdates()
        }
        Log.i(TAG, "applySettingsExceptions: Updated the settings after the activity was created")
    }

    override fun getActivityIntent(): Intent? {
        return if (this::intent.isInitialized) {
            this.intent
        } else {
            super.getActivityIntent()
        }
    }

    fun withIntent(intent: Intent): HomeActivityIntentTestRule {
        this.intent = intent
        return this
    }

    override fun beforeActivityLaunched() {
        super.beforeActivityLaunched()
        setLongTapTimeout(3000)
        Log.i(TAG, "beforeActivityLaunched: Trying to apply the feature flag updates")
        applyFlagUpdates()
        Log.i(TAG, "beforeActivityLaunched: Successfully applied the feature flag updates")
        if (skipOnboarding) { skipOnboardingBeforeLaunch() }
    }

    override fun afterActivityFinished() {
        super.afterActivityFinished()
        setLongTapTimeout(longTapUserPreference)
        closeNotificationShade()
        Log.i(TAG, "afterActivityFinished: Trying to reset all feature flags")
        resetAllFeatureFlags()
        Log.i(TAG, "afterActivityFinished: Successfully performed the reset of all feature flags")
    }

    /**
     * Update the settings values from when this rule was first instantiated to account for any changes
     * done while running the tests.
     * Useful in the scenario about the activity being restarted which would otherwise set the initial
     * settings and override any changes made in the meantime.
     */
    fun updateCachedSettings() {
        isHomeOnboardingDialogEnabled =
            settings.showHomeOnboardingDialog && FenixOnboarding(appContext).userHasBeenOnboarded()
        isPocketEnabled = settings.showPocketRecommendationsFeature
        isJumpBackInCFREnabled = settings.shouldShowJumpBackInCFR
        isNavigationBarCFREnabled = settings.shouldShowNavigationBarCFR
        isRecentTabsFeatureEnabled = settings.showRecentTabsFeature
        isRecentlyVisitedFeatureEnabled = settings.historyMetadataUIFeature
        isPWAsPromptEnabled = !settings.userKnowsAboutPwas
        isTCPCFREnabled = settings.shouldShowTotalCookieProtectionCFR
        isWallpaperOnboardingEnabled = settings.showWallpaperOnboarding
        isDeleteSitePermissionsEnabled = settings.deleteSitePermissions
        isOpenInAppBannerEnabled = settings.shouldShowOpenInAppBanner
        etpPolicy = getETPPolicy(settings)
        isLocationPermissionEnabled = getFeaturePermission(PhoneFeature.LOCATION, settings)
        isNavigationToolbarEnabled = settings.navigationToolbarEnabled
        isMicrosurveyEnabled = settings.microsurveyFeatureEnabled
        isSetAsDefaultBrowserPromptEnabled = settings.setAsDefaultBrowserPromptForExistingUsersEnabled
        shouldUseBottomToolbar = settings.shouldUseBottomToolbar
    }

    companion object {
        /**
         * Create a new instance of [HomeActivityIntentTestRule] which by default will disable specific
         * app features that would otherwise negatively impact most tests.
         *
         * The disabled features are:
         *  - the Jump back in CFR,
         *  - the Total Cookie Protection CFR,
         *  - the PWA prompt dialog,
         *  - the wallpaper onboarding.
         */
        fun withDefaultSettingsOverrides(
            initialTouchMode: Boolean = false,
            launchActivity: Boolean = true,
            skipOnboarding: Boolean = false,
            composeTopSitesEnabled: Boolean = false,
        ) = HomeActivityIntentTestRule(
            initialTouchMode = initialTouchMode,
            launchActivity = launchActivity,
            skipOnboarding = skipOnboarding,
            isJumpBackInCFREnabled = false,
            isPWAsPromptEnabled = false,
            isTCPCFREnabled = false,
            isWallpaperOnboardingEnabled = false,
            isOpenInAppBannerEnabled = false,
            composeTopSitesEnabled = composeTopSitesEnabled,
            isMicrosurveyEnabled = false,
            isSetAsDefaultBrowserPromptEnabled = false,
            // workaround for toolbar at top position by default
            // remove with https://bugzilla.mozilla.org/show_bug.cgi?id=1917640
            shouldUseBottomToolbar = true,
        )
    }
}

// changing the device preference for Touch and Hold delay, to avoid long-clicks instead of a single-click
fun setLongTapTimeout(delay: Int) {
    // Issue: https://github.com/mozilla-mobile/fenix/issues/25132
    var attempts = 0
    while (attempts++ < 3) {
        try {
            Log.i(TAG, "setLongTapTimeout: Trying to set the \"Touch and hold delay\" to: $delay ms")
            mDevice.executeShellCommand("settings put secure long_press_timeout $delay")
            Log.i(TAG, "setLongTapTimeout: Executed command \"settings put secure long_press_timeout $delay\"")
            break
        } catch (e: RuntimeException) {
            Log.i(TAG, "setLongTapTimeout: RuntimeException caught, executing fallback methods")
            e.printStackTrace()
        }
    }
}

private fun skipOnboardingBeforeLaunch() {
    // The production code isn't aware that we're using
    // this API so it can be fragile.
    Log.i(TAG, "skipOnboardingBeforeLaunch: Trying to skip the onboarding before launching the app")
    FenixOnboarding(appContext).finish()
    Log.i(TAG, "skipOnboardingBeforeLaunch: Successfully skipped the onboarding before launching the app")
}

private fun closeNotificationShade() {
    if (mDevice.findObject(
            UiSelector().resourceId("com.android.systemui:id/notification_stack_scroller"),
        ).exists()
    ) {
        Log.i(TAG, "closeNotificationShade: Trying to press device home button")
        mDevice.pressHome()
        Log.i(TAG, "closeNotificationShade: Pressed the device home button")
    }
}

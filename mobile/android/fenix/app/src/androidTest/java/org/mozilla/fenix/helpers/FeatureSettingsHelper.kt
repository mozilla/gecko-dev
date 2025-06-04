/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import androidx.test.platform.app.InstrumentationRegistry
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import org.mozilla.fenix.ext.settings

/**
 * Helper for querying the status and modifying various features and settings in the application.
 */
interface FeatureSettingsHelper {
    /**
     * Whether the Pocket stories feature is enabled or not.
     */
    var isPocketEnabled: Boolean

    /**
     * Whether the onboarding dialog for choosing wallpapers should be shown or not.
     */
    var isWallpaperOnboardingEnabled: Boolean

    /**
     * Whether the "Jump back in" homescreen section is enabled or not.
     * It shows the last visited tab on this device and on other synced devices.
     */
    var isRecentTabsFeatureEnabled: Boolean

    /**
     * Whether the "Recently visited" homescreen section is enabled or not.
     * It can show up to 9 history highlights and history groups.
     */
    var isRecentlyVisitedFeatureEnabled: Boolean

    /**
     * Whether the onboarding dialog for PWAs should be shown or not.
     * It can show the first time a website that can be installed as a PWA is accessed.
     */
    var isPWAsPromptEnabled: Boolean

    /**
     * Whether the "Site permissions" option is checked in the "Delete browsing data" screen or not.
     */
    var isDeleteSitePermissionsEnabled: Boolean

    /**
     * The current "Enhanced Tracking Protection" policy.
     * @see ETPPolicy
     */
    var etpPolicy: ETPPolicy

    /**
     * Enable or disable open in app banner.
     */
    var isOpenInAppBannerEnabled: Boolean

    /**
     * Enable or disable all location permission requests.
     */
    var isLocationPermissionEnabled: SitePermissionsRules.Action

    /**
     * Enable or disable the new main menu.
     */
    var isMenuRedesignEnabled: Boolean

    /**
     * Enable or disable the new main menu CFR.
     */
    var isMenuRedesignCFREnabled: Boolean

    /**
     * Enable or disable the new microsurvey feature.
     */
    var isMicrosurveyEnabled: Boolean

    /**
     * Enable or disable bottom toolbar position.
     */
    var shouldUseBottomToolbar: Boolean

    /**
     * Enable or disable the onboarding feature.
     */
    var onboardingFeatureEnabled: Boolean

    /**
     * Enable or disable the compose home screen feature.
     */
    var isComposeHomepageEnabled: Boolean

    /**
     * Enable or disable the translations prompt after a page that can be translated is loaded.
     */
    fun enableOrDisablePageLoadTranslationsPrompt(enableTranslationsPrompt: Boolean) {
        if (enableTranslationsPrompt) {
            FxNimbusHelper.enablePageLoadTranslationsPrompt()
        } else {
            FxNimbusHelper.disablePageLoadTranslationsPrompt()
        }
    }

    fun applyFlagUpdates()

    fun resetAllFeatureFlags()

    companion object {
        val settings = InstrumentationRegistry.getInstrumentation().targetContext.settings()
    }
}

/**
 * All "Enhanced Tracking Protection" modes.
 */
enum class ETPPolicy {
    STANDARD,
    STRICT,
    CUSTOM,
}

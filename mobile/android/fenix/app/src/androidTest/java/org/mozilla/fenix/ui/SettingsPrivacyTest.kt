/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.test.filters.SdkSuppress
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.settings.DataChoicesFragment
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.notificationShade

/**
 *  Tests for verifying the the privacy and security section of the Settings menu
 *
 */

class SettingsPrivacyTest : TestSetup() {
    @get:Rule
    val activityTestRule = HomeActivityTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2092698
    @Test
    fun settingsPrivacyItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsToolbar()
            verifyPrivacyHeading()
            verifyPrivateBrowsingButton()
            verifyHTTPSOnlyModeButton()
            verifySettingsOptionSummary("HTTPS-Only Mode", "Off")
            verifySettingsOptionSummary("Cookie Banner Blocker in private browsing", "")
            verifyEnhancedTrackingProtectionButton()
            verifySettingsOptionSummary("Enhanced Tracking Protection", "Standard")
            verifySitePermissionsButton()
            verifyDeleteBrowsingDataButton()
            verifyDeleteBrowsingDataOnQuitButton()
            verifySettingsOptionSummary("Delete browsing data on quit", "Off")
            verifyNotificationsButton()
            verifySettingsOptionSummary("Notifications", "Allowed")
            verifyDataCollectionButton()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/243362
    @Test
    fun verifyDataCollectionSettingsTest() {
        DataChoicesFragment.SHOULD_EXIT_APP_AFTER_TURNING_OFF_STUDIES = false
        homeScreen {}.openThreeDotMenu {}.openSettings {}.openSettingsSubMenuDataCollection {
            // Studies depends on the telemetry switch,  if telemetry is off studies will be
            // turned off as well, and will require the app to be restarted.
            verifyDataCollectionView(
                true,
                "On",
            )
            clickUsageAndTechnicalDataToggle()
            verifyUsageAndTechnicalDataToggle(false)
            // Automatically turned off as telemetry was turned off.
            verifyDataCollectionView(
                false,
                "Off",
            )
            clickUsageAndTechnicalDataToggle()
            verifyUsageAndTechnicalDataToggle(true)

            clickStudiesOption()
            verifyStudiesToggle(false)
            // Turning to true
            clickStudiesToggle()
            verifyStudiesDialog()
            clickStudiesDialogCancelButton()
            verifyStudiesToggle(false)
        }
        DataChoicesFragment.SHOULD_EXIT_APP_AFTER_TURNING_OFF_STUDIES = true
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1024594
    @SdkSuppress(maxSdkVersion = 30)
    @Test
    fun verifyNotificationsSettingsTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        // Clear all existing notifications
        notificationShade {
            mDevice.openNotification()
            clearNotifications()
        }

        homeScreen {
        }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.openNotificationShade {
            verifySystemNotificationExists("Close private tabs")
        }.closeNotificationTray {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsOptionSummary("Notifications", "Allowed")
        }.openSettingsSubMenuNotifications {
            verifyAllSystemNotificationsToggleState(true)
            verifyPrivateBrowsingSystemNotificationsToggleState(true)
            clickPrivateBrowsingSystemNotificationsToggle()
            verifyPrivateBrowsingSystemNotificationsToggleState(false)
            clickAllSystemNotificationsToggle()
            verifyAllSystemNotificationsToggleState(false)
        }.goBack {
            verifySettingsOptionSummary("Notifications", "Not allowed")
        }.goBackToBrowser {
        }.openNotificationShade {
            verifySystemNotificationDoesNotExist("Close private tabs")
        }
    }
}

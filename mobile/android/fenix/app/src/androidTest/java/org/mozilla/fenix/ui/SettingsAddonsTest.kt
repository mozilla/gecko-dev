/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.AppAndSystemHelper.registerAndCleanupIdlingResources
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RecyclerViewIdlingResource
import org.mozilla.fenix.helpers.TestAssetHelper.getEnhancedTrackingProtectionAsset
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestHelper.waitUntilSnackbarGone
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.helpers.perf.DetectMemoryLeaksRule
import org.mozilla.fenix.ui.robots.addonsMenu
import org.mozilla.fenix.ui.robots.homeScreen

/**
 *  Tests for verifying the functionality of installing or removing addons
 *
 */
class SettingsAddonsTest : TestSetup() {
    @get:Rule
    val activityTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(),
        ) { it.activity }

    @get:Rule
    val memoryLeaksRule = DetectMemoryLeaksRule()

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/875780
    // Walks through settings add-ons menu to ensure all items are present
    @Test
    fun verifyAddonsListItemsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
            verifyAdvancedHeading()
            verifyAddons()
        }.openAddonsManagerMenu {
            registerAndCleanupIdlingResources(
                RecyclerViewIdlingResource(activityTestRule.activity.findViewById(R.id.add_ons_list), 1),
            ) {
                verifyAddonsItems()
            }
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/875781
    // Installs an add-on from the Add-ons menu and verifies the prompts
    @Test
    fun installAddonFromMainMenuTest() {
        val addonName = "AdGuard AdBlocker"

        homeScreen {}
            .openThreeDotMenu {}
            .openAddonsManagerMenu {
                registerAndCleanupIdlingResources(
                    RecyclerViewIdlingResource(
                        activityTestRule.activity.findViewById(R.id.add_ons_list),
                        1,
                    ),
                ) {
                    waitForAddonsListProgressBarToBeGone()
                    clickInstallAddon(addonName)
                }
                verifyAddonDownloadOverlay()
                verifyAddonPermissionPrompt(addonName)
                cancelInstallAddon()
                clickInstallAddon(addonName)
                acceptPermissionToInstallAddon()
                verifyAddonInstallCompletedPrompt(addonName, activityTestRule.activityRule)
                closeAddonInstallCompletePrompt()
                verifyAddonIsInstalled(addonName)
                verifyEnabledTitleDisplayed()
            }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/561597
    // Installs an addon, then uninstalls it
    @Test
    fun verifyAddonsCanBeUninstalledTest() {
        val addonName = "uBlock Origin"

        addonsMenu {
            installAddon(addonName, activityTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }.openDetailedMenuForAddon(addonName) {
        }.removeAddon(activityTestRule.activityRule) {
            verifySnackBarText("Successfully uninstalled $addonName")
            waitUntilSnackbarGone()
        }.goBack {
        }.openThreeDotMenu {
        }.openAddonsManagerMenu {
            verifyAddonCanBeInstalled(addonName)
        }
    }

    // TODO: Harden to dynamically install addons from position
    //   in list of detected addons on screen instead of hard-coded values.
    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/561600
    // Installs 2 add-on and checks that the app doesn't crash while navigating the app
    @SmokeTest
    @Test
    fun noCrashWithAddonInstalledTest() {
        // setting ETP to Strict mode to test it works with add-ons
        activityTestRule.activity.settings().setStrictETP()

        val uBlockAddon = "uBlock Origin"
        val darkReaderAddon = "Dark Reader"
        val trackingProtectionPage = getEnhancedTrackingProtectionAsset(mockWebServer)

        addonsMenu {
            installAddon(uBlockAddon, activityTestRule.activityRule)
            closeAddonInstallCompletePrompt()
            installAddon(darkReaderAddon, activityTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }.goBack {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(trackingProtectionPage.url) {
            verifyUrl(trackingProtectionPage.url.toString())
        }.goToHomescreen(activityTestRule) {
        }.openTopSiteTabWithTitle(activityTestRule, "Top Articles") {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsView()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/561594
    @SmokeTest
    @Test
    fun verifyUBlockWorksInPrivateModeTest() {
        TestHelper.appContext.settings().shouldShowCookieBannersCFR = false
        val addonName = "uBlock Origin"

        addonsMenu {
            installAddonInPrivateMode(addonName, activityTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }.goBack {
        }.openContextMenuOnTopSitesWithTitle(activityTestRule, "Top Articles") {
        }.openTopSiteInPrivateTab(activityTestRule) {
            verifyPocketPageContent()
        }.openThreeDotMenu {
            openAddonsSubList()
            verifyAddonAvailableInMainMenu(addonName)
            verifyTrackersBlockedByUblock()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/875785
    @Test
    fun verifyUBlockWorksInNormalModeTest() {
        val addonName = "uBlock Origin"

        addonsMenu {
            installAddon(addonName, activityTestRule.activityRule)
            closeAddonInstallCompletePrompt()
        }.goBack {
        }.openTopSiteTabWithTitle(activityTestRule, "Top Articles") {
            verifyUrl("getpocket.com/explore")
        }.openThreeDotMenu {
            openAddonsSubList()
            verifyTrackersBlockedByUblock()
        }
    }
}

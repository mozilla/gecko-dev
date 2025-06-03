/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.openAppFromExternalLink
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper.getGenericAsset
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.helpers.perf.DetectMemoryLeaksRule
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

/**
 *  Tests for verifying the Homepage settings menu
 *
 */
class SettingsHomepageTest : TestSetup() {
    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true),
        ) { it.activity }

    @get:Rule
    val memoryLeaksRule = DetectMemoryLeaksRule()

    @Rule
    @JvmField
    val retryTestRule = RetryTestRule(3)

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1564843
    @Test
    fun verifyHomepageSettingsTest() {
        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHomepageSubMenu {
            verifyHomePageView()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1564859
    @Test
    fun verifyShortcutOptionTest() {
        // en-US defaults
        val defaultTopSites = arrayOf(
            "Wikipedia",
            "Google",
        )
        val genericURL = getGenericAsset(mockWebServer, 1)

        homeScreen {
            defaultTopSites.forEach { item ->
                verifyExistingTopSitesTabs(composeTestRule, item)
            }
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickShortcutsButton()
        }.goBackToHomeScreen {
            defaultTopSites.forEach { item ->
                verifyNotExistingTopSiteItem(composeTestRule, item)
            }
        }
        // Disabling the "Shortcuts" homepage setting option should remove the "Add to shortcuts" from main menu option
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu {
            expandMenu()
            verifyAddToShortcutsButton(shouldExist = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1565003
    @Test
    fun verifyRecentlyVisitedOptionTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isRecentTabsFeatureEnabled = false
        }
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.goToHomescreen(composeTestRule) {
            verifyRecentlyVisitedSectionIsDisplayed(true)
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickRecentlyVisited()
        }.goBackToHomeScreen {
            verifyRecentlyVisitedSectionIsDisplayed(false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1564999
    @SmokeTest
    @Test
    fun jumpBackInOptionTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.goToHomescreen(composeTestRule) {
            verifyJumpBackInSectionIsDisplayed()
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickJumpBackInButton()
        }.goBackToHomeScreen {
            verifyJumpBackInSectionIsNotDisplayed(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1565000
    @SmokeTest
    @Test
    fun recentBookmarksOptionTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu {
        }.bookmarkPage {
        }.goToHomescreen(composeTestRule) {
            verifyBookmarksSectionIsDisplayed(exists = true)
        }.openThreeDotMenu {
        }.openCustomizeHome {
            clickRecentBookmarksButton()
        }.goBackToHomeScreen {
            verifyBookmarksSectionIsDisplayed(exists = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1569831
    @SmokeTest
    @Test
    fun verifyOpeningScreenOptionsTest() {
        val genericURL = getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsOptionSummary("Homepage", "Open on homepage after four hours")
        }.openHomepageSubMenu {
            verifySelectedOpeningScreenOption("Homepage after four hours of inactivity")
            clickOpeningScreenOption("Homepage")
            verifySelectedOpeningScreenOption("Homepage")
        }

        restartApp(composeTestRule.activityRule)

        homeScreen {
            verifyHomeScreen()
        }.openThreeDotMenu {
        }.openSettings {
            verifySettingsOptionSummary("Homepage", "Open on homepage")
        }.openHomepageSubMenu {
            clickOpeningScreenOption("Last tab")
            verifySelectedOpeningScreenOption("Last tab")
        }.goBack {
            verifySettingsOptionSummary("Homepage", "Open on last tab")
        }

        restartApp(composeTestRule.activityRule)

        browserScreen {
            verifyUrl(genericURL.url.toString())
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1569843
    @Test
    fun verifyOpeningScreenAfterLaunchingExternalLinkTest() {
        val genericPage = getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openThreeDotMenu {
        }.openSettings {
        }.openHomepageSubMenu {
            clickOpeningScreenOption("Homepage")
        }.goBackToHomeScreen {}

        with(composeTestRule.activityRule) {
            finishActivity()
            mDevice.waitForIdle()
            openAppFromExternalLink(genericPage.url.toString())
        }

        browserScreen {
            verifyPageContent(genericPage.content)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1676359
    @Ignore("Intermittent test: https://github.com/mozilla-mobile/fenix/issues/26559")
    @Test
    fun verifyWallpaperChangeTest() {
        val wallpapers = listOf(
            "Wallpaper Item: amethyst",
            "Wallpaper Item: cerulean",
            "Wallpaper Item: sunrise",
        )

        for (wallpaper in wallpapers) {
            homeScreen {
            }.openThreeDotMenu {
            }.openCustomizeHome {
                openWallpapersMenu()
                selectWallpaper(wallpaper)
                verifySnackBarText("Wallpaper updated!")
            }.clickSnackBarViewButton {
                verifyWallpaperImageApplied(true)
            }
        }
    }
}

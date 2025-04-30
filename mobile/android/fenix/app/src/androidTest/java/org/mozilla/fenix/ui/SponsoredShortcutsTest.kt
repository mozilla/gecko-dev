/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.Constants.defaultTopSitesList
import org.mozilla.fenix.helpers.DataGenerationHelper.getSponsoredShortcutTitle
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MockBrowserDataHelper
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.helpers.perf.DetectMemoryLeaksRule
import org.mozilla.fenix.ui.robots.homeScreen

/**
 * Tests Sponsored shortcuts functionality
 */

class SponsoredShortcutsTest : TestSetup() {
    private lateinit var sponsoredShortcutTitle: String
    private lateinit var sponsoredShortcutTitle2: String

    @get:Rule
    val activityIntentTestRule = AndroidComposeTestRule(
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true),
    ) { it.activity }

    @get:Rule
    val memoryLeaksRule = DetectMemoryLeaksRule()

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729331
    // Expected for en-us defaults
    @SmokeTest
    @Test
    fun verifySponsoredShortcutsListTest() {
        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)
            defaultTopSitesList.values.forEach { value ->
                verifyExistingTopSitesTabs(activityIntentTestRule, value)
            }
        }.openThreeDotMenu {
        }.openCustomizeHome {
            verifySponsoredShortcutsCheckBox(true)
            clickSponsoredShortcuts()
            verifySponsoredShortcutsCheckBox(false)
        }.goBackToHomeScreen {
            verifyNotExistingSponsoredTopSitesList()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729338
    @Test
    fun openSponsoredShortcutTest() {
        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)
            sponsoredShortcutTitle = getSponsoredShortcutTitle(2)
        }.openTopSiteTabWithTitle(activityIntentTestRule, sponsoredShortcutTitle) {
            verifyUrl(sponsoredShortcutTitle)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729334
    @Test
    fun openSponsoredShortcutInPrivateTabTest() {
        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)
            sponsoredShortcutTitle = getSponsoredShortcutTitle(2)
        }.openContextMenuOnTopSitesWithTitle(activityIntentTestRule, sponsoredShortcutTitle) {
        }.openTopSiteInPrivateTab(activityIntentTestRule) {
            verifyUrl(sponsoredShortcutTitle)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729335
    @Test
    fun openSponsorsAndYourPrivacyOptionTest() {
        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)
            sponsoredShortcutTitle = getSponsoredShortcutTitle(2)
        }.openContextMenuOnTopSitesWithTitle(activityIntentTestRule, sponsoredShortcutTitle) {
        }.clickSponsorsAndPrivacyButton(activityIntentTestRule) {
            verifySponsoredShortcutsLearnMoreURL()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729336
    @Test
    fun openSponsoredShortcutsSettingsOptionTest() {
        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)
            sponsoredShortcutTitle = getSponsoredShortcutTitle(2)
        }.openContextMenuOnTopSitesWithTitle(activityIntentTestRule, sponsoredShortcutTitle) {
        }.clickSponsoredShortcutsSettingsButton(activityIntentTestRule) {
            verifyHomePageView()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729337
    @Test
    fun verifySponsoredShortcutsDetailsTest() {
        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)
            sponsoredShortcutTitle = getSponsoredShortcutTitle(2)
            sponsoredShortcutTitle2 = getSponsoredShortcutTitle(3)

            verifySponsoredShortcutDetails(sponsoredShortcutTitle, 2)
            verifySponsoredShortcutDetails(sponsoredShortcutTitle2, 3)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729328
    // 1 sponsored shortcut should be displayed if there are 7 pinned top sites
    @Test
    fun verifySponsoredShortcutsListWithSevenPinnedSitesTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)
        val thirdWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 3)
        val fourthWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 4)

        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)
            sponsoredShortcutTitle = getSponsoredShortcutTitle(2)
            sponsoredShortcutTitle2 = getSponsoredShortcutTitle(3)

            verifySponsoredShortcutDetails(sponsoredShortcutTitle, 2)
            verifySponsoredShortcutDetails(sponsoredShortcutTitle2, 3)
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            verifyPageContent(firstWebPage.content)
        }.openThreeDotMenu {
            expandMenuFully()
        }.addToFirefoxHome {
        }.goToHomescreen(activityIntentTestRule) {
            verifyExistingTopSitesTabs(activityIntentTestRule, firstWebPage.title)
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(secondWebPage.url) {
            verifyPageContent(secondWebPage.content)
        }.openThreeDotMenu {
            expandMenuFully()
        }.addToFirefoxHome {
        }.goToHomescreen(activityIntentTestRule) {
            verifyExistingTopSitesTabs(activityIntentTestRule, secondWebPage.title)
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(thirdWebPage.url) {
            verifyPageContent(thirdWebPage.content)
        }.openThreeDotMenu {
            expandMenuFully()
        }.addToFirefoxHome {
        }.goToHomescreen(activityIntentTestRule) {
            verifyExistingTopSitesTabs(activityIntentTestRule, thirdWebPage.title)
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(fourthWebPage.url) {
            verifyPageContent(fourthWebPage.content)
        }.openThreeDotMenu {
            expandMenuFully()
        }.addToFirefoxHome {
        }.goToHomescreen(activityIntentTestRule) {
            verifySponsoredShortcutDetails(sponsoredShortcutTitle, 2)
            verifySponsoredShortcutDoesNotExist(sponsoredShortcutTitle2, 3)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1729329
    // No sponsored shortcuts should be displayed if there are 8 pinned top sites
    @Test
    fun verifySponsoredShortcutsListWithEightPinnedSitesTest() {
        val pagesList = listOf(
            TestAssetHelper.getGenericAsset(mockWebServer, 1),
            TestAssetHelper.getGenericAsset(mockWebServer, 2),
            TestAssetHelper.getGenericAsset(mockWebServer, 3),
            TestAssetHelper.getGenericAsset(mockWebServer, 4),
            TestAssetHelper.getLoremIpsumAsset(mockWebServer),
        )

        homeScreen {
            verifyExistingTopSitesList(activityIntentTestRule)

            sponsoredShortcutTitle = getSponsoredShortcutTitle(2)
            sponsoredShortcutTitle2 = getSponsoredShortcutTitle(3)

            verifySponsoredShortcutDetails(sponsoredShortcutTitle, 2)
            verifySponsoredShortcutDetails(sponsoredShortcutTitle2, 3)

            MockBrowserDataHelper.addPinnedSite(
                Pair(pagesList[0].title, pagesList[0].url.toString()),
                Pair(pagesList[1].title, pagesList[1].url.toString()),
                Pair(pagesList[2].title, pagesList[2].url.toString()),
                Pair(pagesList[3].title, pagesList[3].url.toString()),
                Pair(pagesList[4].title, pagesList[4].url.toString()),
                activityTestRule = activityIntentTestRule.activityRule,
            )

            verifySponsoredShortcutDoesNotExist(sponsoredShortcutTitle, 2)
            verifySponsoredShortcutDoesNotExist(sponsoredShortcutTitle2, 3)
        }
    }
}

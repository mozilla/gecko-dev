/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.test.rule.ActivityTestRule
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.DataGenerationHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.homeScreen

class RedesignedMenuTest : TestSetup() {
    @get:Rule
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                skipOnboarding = true,
                isNavigationToolbarEnabled = true,
                isNavigationBarCFREnabled = false,
                isSetAsDefaultBrowserPromptEnabled = false,
                isMenuRedesignEnabled = true,
                isMenuRedesignCFREnabled = false,
            ),
        ) { it.activity }

    @get: Rule
    val intentReceiverActivityTestRule = ActivityTestRule(
        IntentReceiverActivity::class.java,
        true,
        false,
    )

    @SmokeTest
    @Test
    fun homepageRedesignedMenuItemsTest() {
        homeScreen {
        }.openThreeDotMenuFromRedesignedToolbar(composeTestRule) {
            expandRedesignedMenu()
            verifyHomeRedesignedMainMenuItems()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2763386
    // Verifies the main menu of a custom tab with a custom menu item
    @SmokeTest
    @Test
    fun verifyCustomTabMenuItemsTest() {
        val customMenuItem = "TestMenuItem"
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
                customMenuItem,
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenuFromRedesignedToolbar {
            verifyRedesignedCustomTabsMainMenuItems(customMenuItem)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769619
    // The test opens a link in a custom tab then sends it to the browser
    @SmokeTest
    @Test
    fun openCustomTabInFirefoxTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
            verifyCustomTabCloseButton()
        }.openMainMenuFromRedesignedToolbar {
        }.clickOpenInBrowserButtonFromRedesignedToolbar {
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2769620
    @SmokeTest
    @Test
    fun shareCustomTabUsingMainMenuButtonTest() {
        val customTabPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        intentReceiverActivityTestRule.launchActivity(
            createCustomTabIntent(
                customTabPage.url.toString(),
            ),
        )

        customTabScreen {
        }.openMainMenuFromRedesignedToolbar {
        }.clickShareButtonFromRedesignedMenu {
            verifyShareTabLayout()
            verifySharingWithSelectedApp(
                appName = "Gmail",
                content = customTabPage.url.toString(),
                subject = customTabPage.title,
            )
        }
    }
}

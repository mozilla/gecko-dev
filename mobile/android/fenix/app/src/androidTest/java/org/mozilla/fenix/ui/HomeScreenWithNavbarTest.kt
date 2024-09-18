package org.mozilla.fenix.ui

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.GleanMetrics.NavigationBar
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.RetryTestRule
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestHelper.restartApp
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.screenshots.tapOnTabCounter
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.tabDrawer

/**
 * Tests verifying the behavior of navbar in the home screen.
 */
class HomeScreenWithNavbarTest : TestSetup() {
    @get:Rule(order = 0)
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(),
        ) { it.activity }

    @Rule(order = 1)
    @JvmField
    val retryTestRule = RetryTestRule(3)

    @Test
    fun verifyTabCounterUpdateInNavbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyTabCounter("1")
        }
    }

    @Test
    fun verifyTabCounterClickFromNavbarInNormalModeTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        restartApp(composeTestRule.activityRule)

        navigationToolbar {
            tapOnTabCounter()
        }
        tabDrawer(composeTestRule) {
            verifyNormalBrowsingButtonIsSelected()
            verifyPrivateBrowsingButtonIsSelected(isSelected = false)
            verifySyncedTabsButtonIsSelected(isSelected = false)
        }
    }

    @Test
    fun verifyTabCounterClickFromNavbarInPrivateModeTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        restartApp(composeTestRule.activityRule)
        homeScreen { }.togglePrivateBrowsingMode()

        navigationToolbar {
            tapOnTabCounter()
        }
        tabDrawer(composeTestRule) {
            verifyNormalBrowsingButtonIsSelected(isSelected = false)
            verifyPrivateBrowsingButtonIsSelected()
            verifySyncedTabsButtonIsSelected(isSelected = false)
        }
    }

    @Test
    fun verifyTabsCounterShortcutMenuOptionFromNavbarInNormalModeTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        restartApp(composeTestRule.activityRule)

        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItemsForNormalHomescreen()
        }
    }

    @Test
    fun verifyTabsCounterShortcutMenuOptionFromNavbarInPrivateModeTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        restartApp(composeTestRule.activityRule)
        homeScreen { }.togglePrivateBrowsingMode()

        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItemsForPrivateHomescreen()
        }
    }

    @Test
    fun verifyTabsCounterShortcutMenuFromNavbarRecordsTelemetry() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        restartApp(composeTestRule.activityRule)

        assertNull(NavigationBar.homeTabTrayLongTapped.testGetValue())
        navigationToolbar {
        }.openTabButtonShortcutsMenu { }
        assertNotNull(NavigationBar.homeTabTrayLongTapped.testGetValue())
    }
}

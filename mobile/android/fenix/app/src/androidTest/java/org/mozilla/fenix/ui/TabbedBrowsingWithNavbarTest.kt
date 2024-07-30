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
import org.mozilla.fenix.helpers.TestHelper.clickSnackbarButton
import org.mozilla.fenix.helpers.TestHelper.verifySnackBarText
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.browserScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar

class TabbedBrowsingWithNavbarTest : TestSetup() {
    @get:Rule(order = 0)
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityIntentTestRule.withDefaultSettingsOverrides(
                skipOnboarding = true,
            ),
        ) { it.activity }

    @Rule(order = 1)
    @JvmField
    val retryTestRule = RetryTestRule(3)

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/903591
    @Test
    fun closingPrivateTabsFromNavbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen { }.togglePrivateBrowsingMode(switchPBModeOn = true)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openTabDrawer(composeTestRule) {
            verifyExistingOpenTabs("Test_Page_1")
            closeTab()
            verifySnackBarText("Private tab closed")
            clickSnackbarButton("UNDO")
        }
        browserScreen {
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/903587
    @Test
    fun verifyPrivateTabsTrayWithOpenTabFromNavbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
        }
        val website = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openTabDrawer(composeTestRule) {
        }.toggleToPrivateTabs {
        }.openNewTab {
        }.submitQuery(website.url.toString()) {
        }.openTabDrawer(composeTestRule) {
            verifyNormalBrowsingButtonIsSelected(false)
            verifyPrivateBrowsingButtonIsSelected(true)
            verifySyncedTabsButtonIsSelected(false)
            verifyThreeDotButton()
            verifyNormalTabCounter()
            verifyPrivateTabsList()
            verifyExistingOpenTabs(website.title)
            verifyTabCloseButton()
            verifyTabThumbnail()
            verifyFab()
        }
    }

    @Test
    fun tabsCounterShortcutMenuOptionFromNavbarInNormalModeTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }
    }

    @Test
    fun tabsCounterShortcutMenuOptionFromNavbarInPrivateModeTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {
        }.openTabDrawer(composeTestRule) {
        }.toggleToPrivateTabs {
        }.openNewTab {
        }.submitQuery(defaultWebPage.url.toString()) {
            waitForPageToLoad()
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
            verifyTabButtonShortcutMenuItems()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2343663
    @Test
    fun tabsCounterShortcutMenuNewPrivateTabFromNavbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifySearchBarPlaceholder("Search or enter address")
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2343662
    @Test
    fun tabsCounterShortcutMenuNewTabFromNavbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {}
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewTabFromShortcutsMenu {
            verifySearchBarPlaceholder("Search or enter address")
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2344199
    @Test
    fun privateTabsCounterShortcutMenuNewPrivateTabFromNavbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {}.togglePrivateBrowsingMode(switchPBModeOn = true)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewPrivateTabFromShortcutsMenu {
            verifySearchBarPlaceholder("Search or enter address")
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2344198
    @Test
    fun privateTabsCounterShortcutMenuNewTabFromNavbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
            it.isNavigationBarCFREnabled = false
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        homeScreen {}.togglePrivateBrowsingMode(switchPBModeOn = true)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyPageContent(defaultWebPage.content)
        }
        navigationToolbar {
        }.openTabButtonShortcutsMenu {
        }.openNewTabFromShortcutsMenu {
            verifySearchToolbar(isDisplayed = true)
        }.dismissSearchBar {
            verifyIfInPrivateOrNormalMode(privateBrowsingEnabled = false)
        }
    }

    @Test
    fun verifyTabsCounterShortcutMenuFromNavbarRecordsTelemetry() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.isNavigationToolbarEnabled = true
        }
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) { }

        assertNull(NavigationBar.browserTabTrayLongTapped.testGetValue())
        navigationToolbar {
        }.openTabButtonShortcutsMenu { }
        assertNotNull(NavigationBar.browserTabTrayLongTapped.testGetValue())
    }
}

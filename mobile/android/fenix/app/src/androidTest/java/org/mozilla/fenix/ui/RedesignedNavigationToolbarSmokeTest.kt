/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
@file:Suppress("DEPRECATION")

package org.mozilla.fenix.ui

import android.content.Context
import android.hardware.camera2.CameraManager
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.core.net.toUri
import androidx.test.rule.ActivityTestRule
import org.junit.Assume
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.IntentReceiverActivity
import org.mozilla.fenix.customannotations.SmokeTest
import org.mozilla.fenix.helpers.AppAndSystemHelper.assertAppWithPackageNameOpens
import org.mozilla.fenix.helpers.AppAndSystemHelper.enableOrDisableBackGestureNavigationOnDevice
import org.mozilla.fenix.helpers.AppAndSystemHelper.grantSystemPermission
import org.mozilla.fenix.helpers.DataGenerationHelper.createCustomTabIntent
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.exitMenu
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestSetup
import org.mozilla.fenix.ui.robots.clickPageObject
import org.mozilla.fenix.ui.robots.customTabScreen
import org.mozilla.fenix.ui.robots.homeScreen
import org.mozilla.fenix.ui.robots.navigationToolbar
import org.mozilla.fenix.ui.robots.shareOverlay

class RedesignedNavigationToolbarSmokeTest : TestSetup() {
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

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/987326
    // Swipes the nav bar left/right to switch between tabs
    @SmokeTest
    @Test
    fun swipeToSwitchTabTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        // Disable the back gesture from the edge of the screen on the device.
        enableOrDisableBackGestureNavigationOnDevice(backGestureNavigationEnabled = false)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
        }.openTabDrawerFromRedesignedToolbar(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            swipeNavBarRight(secondWebPage.url.toString())
            verifyUrl(firstWebPage.url.toString())
            swipeNavBarLeft(firstWebPage.url.toString())
            verifyUrl(secondWebPage.url.toString())
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2265279
    @SmokeTest
    @Test
    fun verifySecurePageSecuritySubMenuTest() {
        val defaultWebPage = "https://mozilla-mobile.github.io/testapp/loginForm"
        val defaultWebPageTitle = "Login_form"

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.toUri()) {
            waitForPageToLoad(pageLoadWaitingTime = waitingTimeLong)
        }.openSiteSecuritySheet {
            verifyQuickActionSheet(defaultWebPage, true)
            openSecureConnectionSubMenu(true)
            verifySecureConnectionSubMenu(defaultWebPageTitle, defaultWebPage, true)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2265280
    @SmokeTest
    @Test
    fun verifyInsecurePageSecuritySubMenuTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            waitForPageToLoad()
        }.openSiteSecuritySheet {
            verifyQuickActionSheet(defaultWebPage.url.toString(), false)
            openSecureConnectionSubMenu(false)
            verifySecureConnectionSubMenu(defaultWebPage.title, defaultWebPage.url.toString(), false)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1661318
    @SmokeTest
    @Test
    fun verifyClearCookiesFromQuickSettingsTest() {
        val helpPageUrl = "mozilla.org"

        homeScreen {
        }.openThreeDotMenuFromRedesignedToolbar() {
        }.openHelp(composeTestRule) {
        }.openSiteSecuritySheet {
            clickQuickActionSheetClearSiteData()
            verifyClearSiteDataPrompt(helpPageUrl)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/1681928
    @SmokeTest
    @Test
    fun useAppWhileTabIsCrashedTest() {
        val firstWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val secondWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        homeScreen {
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(firstWebPage.url) {
            mDevice.waitForIdle()
        }.openTabDrawerFromRedesignedToolbar(composeTestRule) {
        }.openNewTab {
        }.submitQuery(secondWebPage.url.toString()) {
            waitForPageToLoad()
        }

        navigationToolbar {
        }.openTabCrashReporter {
            verifyTabCrashReporterView()
        }.openTabDrawerFromRedesignedToolbar(composeTestRule) {
            verifyExistingOpenTabs(firstWebPage.title)
            verifyExistingOpenTabs(secondWebPage.title)
        }.closeTabDrawer {
        }.goToHomescreenWithRedesignedToolbar {
            verifyExistingTopSitesList()
        }.openThreeDotMenuFromRedesignedToolbar() {
            verifySettingsButton(composeTestRule)
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2728833
    @SmokeTest
    @Test
    fun refreshPageButtonTest() {
        val refreshWebPage = TestAssetHelper.getRefreshAsset(mockWebServer)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(refreshWebPage.url) {
            refreshPageFromRedesignedToolbar()
            verifyPageContent("REFRESHED")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2728832
    @SmokeTest
    @Test
    fun toolbarShareButtonTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
        }.clickShareButtonFromRedesignedToolbar {
            verifyShareTabLayout()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2048448
    // Save edited PDF file from the share overlay
    @SmokeTest
    @Test
    fun saveAsPdfFunctionalityTest() {
        val genericURL =
            TestAssetHelper.getGenericAsset(mockWebServer, 3)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
            clickPageObject(itemWithText("PDF form file"))
            waitForPageToLoad()
            clickPageObject(itemWithResIdAndText("android:id/button2", "CANCEL"))
            fillPdfForm("Firefox")
        }.clickShareButtonFromRedesignedToolbar {
        }.clickSaveAsPDF {
            verifyDownloadPrompt("pdfForm.pdf")
        }.clickDownload {
        }.clickOpen("application/pdf") {
            assertAppWithPackageNameOpens(packageName)
            verifyUrl("content://media/external_primary/downloads/")
            verifyTabCounter("2")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/235397
    @SmokeTest
    @Test
    fun scanQRCodeToOpenAWebpageTest() {
        val cameraManager = TestHelper.appContext.getSystemService(Context.CAMERA_SERVICE) as CameraManager
        Assume.assumeTrue(cameraManager.cameraIdList.isNotEmpty())

        homeScreen {
        }.openSearch {
            clickScanButton()
            grantSystemPermission()
            verifyScannerOpen()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/243845
    @SmokeTest
    @Test
    fun verifyShareUrlBarTextSelectionOptionTest() {
        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.openNavigationToolbar {
            longClickEditModeToolbar()
            clickContextMenuItem("Share")
        }
        shareOverlay {
            verifyAndroidShareLayout()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2767071
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
        }.clickOpenInBrowserButtonFromRedesignedToolbar {
            verifyTabCounter("1")
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2728825
    @SmokeTest
    @Test
    fun verifyToolbarWithAddressBarAtTheTopTest() {
        homeScreen {
        }.openThreeDotMenuFromRedesignedToolbar() {
        }.openSettings(composeTestRule) {
        }.openCustomizeSubMenu {
            verifyAddressBarPositionPreference("Top")
        }

        exitMenu()

        homeScreen {
            verifyAddressBarPosition(bottomPosition = false)
            verifyNavigationToolbarIsSetToTheBottomOfTheHomeScreen()
        }
        navigationToolbar {
            verifyRedesignedNavigationToolbarItems()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2767049
    @SmokeTest
    @Test
    fun verifyToolbarWithAddressBarAtTheBottomTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.shouldUseBottomToolbar = true
        }

        homeScreen {
        }.openThreeDotMenuFromRedesignedToolbar() {
        }.openSettings(composeTestRule) {
        }.openCustomizeSubMenu {
            verifyAddressBarPositionPreference("Bottom")
        }

        exitMenu()

        homeScreen {
            verifyAddressBarPosition(bottomPosition = true)
            verifyNavigationToolbarIsSetToTheBottomOfTheHomeScreen()
        }
        navigationToolbar {
            verifyRedesignedNavigationToolbarItems()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2728834
    @SmokeTest
    @Test
    fun returnToHomescreenWithRedesignedToolbarTest() {
        composeTestRule.activityRule.applySettingsExceptions {
            it.shouldUseBottomToolbar = false
        }

        val genericURL = TestAssetHelper.getGenericAsset(mockWebServer, 1)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.goToHomescreenWithRedesignedToolbar {
            verifyHomeScreen()
        }.openThreeDotMenuFromRedesignedToolbar() {
        }.openSettings(composeTestRule) {
        }.openCustomizeSubMenu {
            clickBottomToolbarToggle()
            verifyAddressBarPositionPreference("Bottom")
        }

        exitMenu()

        navigationToolbar {
        }.enterURLAndEnterToBrowser(genericURL.url) {
        }.goToHomescreenWithRedesignedToolbar {
            verifyHomeScreen()
        }
    }

    // TestRail link: https://mozilla.testrail.io/index.php?/cases/view/2728835
    @SmokeTest
    @Test
    fun navigateBackAndForwardTest() {
        val defaultWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 1)
        val nextWebPage = TestAssetHelper.getGenericAsset(mockWebServer, 2)

        navigationToolbar {
        }.enterURLAndEnterToBrowser(defaultWebPage.url) {
            verifyTabCounter("1")
        }.openNavigationToolbar {
        }.enterURLAndEnterToBrowser(nextWebPage.url) {
            verifyTabCounter("1")
            verifyUrl(nextWebPage.url.toString())
            goToPreviousPageFromRedesignedToolbar()
            verifyUrl(defaultWebPage.url.toString())
            verifyTabCounter("1")
            goForwardFromRedesignedToolbar()
            verifyUrl(nextWebPage.url.toString())
        }
    }
}

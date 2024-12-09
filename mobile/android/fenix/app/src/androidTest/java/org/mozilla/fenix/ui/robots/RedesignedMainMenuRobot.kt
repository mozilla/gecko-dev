/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeUp
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource

class RedesignedMainMenuRobot {

    fun expandRedesignedMenu(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "expandRedesignedMenu: Trying to perform swipe up action on the main menu.")
        composeTestRule
            .onNode(hasAnyChild(hasContentDescription("New private tab")))
            .performTouchInput { swipeUp() }
        composeTestRule.waitForIdle()
        Log.i(TAG, "expandRedesignedMenu: Performed swipe up action on the main menu.")
    }

    fun verifyHomeRedesignedMainMenuItems(composeTestRule: ComposeTestRule, areAddonsInstalled: Boolean, addonName: String = "") {
        Log.i(TAG, "verifyHomeRedesignedMainMenuItems: Trying to verify the main menu items on the home page.")
        composeTestRule.signInButton().assertIsDisplayed()
        composeTestRule.signInButtonDescription().assertIsDisplayed()
        composeTestRule.helpButton().assertIsDisplayed()
        composeTestRule.settingsButton().assertIsDisplayed()
        composeTestRule.newTabButton().assertIsNotEnabled()
        composeTestRule.newPrivateTabButton().assertIsDisplayed()
        if (areAddonsInstalled) {
            composeTestRule.pageViewExtensionsButton(addonName).assertIsDisplayed()
        } else {
            composeTestRule.noExtensionsButton().assertIsDisplayed()
        }
        composeTestRule.bookmarksButton().assertIsDisplayed()
        composeTestRule.historyButton().assertIsDisplayed()
        composeTestRule.downloadsButton().assertIsDisplayed()
        composeTestRule.passwordsButton().assertIsDisplayed()
        composeTestRule.whatsNewButton().assertIsDisplayed()
        composeTestRule.customizeHomeButton().assertIsDisplayed()
        Log.i(TAG, "verifyHomeRedesignedMainMenuItems: Verified the main menu items on the home page.")
    }

    fun verifyPageMainMenuItems(composeTestRule: ComposeTestRule, areAddonsInstalled: Boolean, addonName: String = "") {
        Log.i(TAG, "verifyPageMainMenuItems: Trying to verify the main menu items on the web page.")
        composeTestRule.signInButton().assertIsDisplayed()
        composeTestRule.signInButtonDescription().assertIsDisplayed()
        composeTestRule.helpButton().assertIsDisplayed()
        composeTestRule.settingsButton().assertIsDisplayed()
        composeTestRule.newTabButton().assertIsEnabled()
        composeTestRule.newPrivateTabButton().assertIsDisplayed()
        if (areAddonsInstalled) {
            composeTestRule.pageViewExtensionsButton(addonName).assertIsDisplayed()
        } else {
            composeTestRule.noExtensionsButton().assertIsDisplayed()
        }
        composeTestRule.bookmarksButton().assertIsDisplayed()
        composeTestRule.historyButton().assertIsDisplayed()
        composeTestRule.downloadsButton().assertIsDisplayed()
        composeTestRule.passwordsButton().assertIsDisplayed()
        composeTestRule.findInPageButton().assertIsDisplayed()
        composeTestRule.desktopSiteButton().assertIsDisplayed()
        composeTestRule.toolsMenuButton().assertIsDisplayed()
        composeTestRule.saveMenuButton().assertIsDisplayed()
        Log.i(TAG, "verifyPageMainMenuItems: Verified the main menu items on the web page.")
    }

    fun verifySettingsButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifySettingsButton: Trying to verify the Settings button from the new main menu design is displayed.")
        composeTestRule.settingsButton().assertIsDisplayed()
        Log.i(TAG, "verifySettingsButton: Verified the Settings button from the new main menu design is displayed.")
    }

    fun verifySwitchToDesktopSiteButtonIsEnabled(composeTestRule: ComposeTestRule, isEnabled: Boolean) {
        Log.i(TAG, "verifySwitchToDesktopSiteButtonIsEnabled: Trying to verify the Switch to Desktop Site button from the new main menu design is enabled.")
        if (isEnabled) {
            composeTestRule.desktopSiteButton().assertIsEnabled()
            Log.i(TAG, "verifySwitchToDesktopSiteButtonIsEnabled: Verified the Switch to Desktop Site button from the new main menu design is enabled.")
        } else {
            composeTestRule.desktopSiteButton().assertIsNotEnabled()
            Log.i(TAG, "verifySwitchToDesktopSiteButtonIsEnabled: Verified the Switch to Desktop Site button from the new main menu design is disabled.")
        }
    }

    fun verifyNoExtensionsButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyNoExtensionsButton: Trying to verify that the \"Extensions\" button exists.")
        composeTestRule.noExtensionsButton().assertExists()
        Log.i(TAG, "verifyNoExtensionsButton: Verified that the \"Extensions\" button exists.")
    }

    class Transition {
        fun openSettings(composeTestRule: ComposeTestRule, interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the Settings button from the new main menu design.")
            composeTestRule.settingsButton().performClick()
            Log.i(TAG, "openSettings: Clicked the Settings button from the new main menu design.")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openHelp(composeTestRule: ComposeTestRule, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the Help button from the new main menu design.")
            composeTestRule.helpButton().performClick()
            Log.i(TAG, "openSettings: Clicked the Help button from the new main menu design.")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickNewTabButton(composeTestRule: ComposeTestRule, interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the New  tab button from the new main menu design.")
            composeTestRule.newTabButton().performClick()
            Log.i(TAG, "openSettings: Clicked the New tab button from the new main menu design.")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickNewPrivateTabButton(composeTestRule: ComposeTestRule, interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the New private tab button from the new main menu design.")
            composeTestRule.newPrivateTabButton().performClick()
            Log.i(TAG, "openSettings: Clicked the New private tab button from the new main menu design.")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickFindInPageButton(composeTestRule: ComposeTestRule, interact: FindInPageRobot.() -> Unit): FindInPageRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the FindInPage button from the new main menu design.")
            composeTestRule.findInPageButton().performClick()
            Log.i(TAG, "openSettings: Clicked the FindInPage button from the new main menu design.")

            FindInPageRobot().interact()
            return FindInPageRobot.Transition()
        }

        fun openBookmarks(composeTestRule: ComposeTestRule, interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the Bookmarks button from the new main menu design.")
            composeTestRule.bookmarksButton().performClick()
            Log.i(TAG, "openSettings: Clicked the Bookmarks button from the new main menu design.")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun openHistory(composeTestRule: ComposeTestRule, interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the History button from the new main menu design.")
            composeTestRule.historyButton().performClick()
            Log.i(TAG, "openSettings: Clicked the History button from the new main menu design.")

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun openDownloads(composeTestRule: ComposeTestRule, interact: DownloadRobot.() -> Unit): DownloadRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the Download button from the new main menu design.")
            composeTestRule.downloadsButton().performClick()
            Log.i(TAG, "openSettings: Clicked the Download button from the new main menu design.")

            DownloadRobot().interact()
            return DownloadRobot.Transition()
        }

        fun openPasswords(composeTestRule: ComposeTestRule, interact: SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.() -> Unit): SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the Download button from the new main menu design.")
            composeTestRule.passwordsButton().performClick()
            Log.i(TAG, "openSettings: Clicked the Download button from the new main menu design.")

            SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot().interact()
            return SettingsSubMenuLoginsAndPasswordsSavedLoginsRobot.Transition()
        }

        fun openNoExtensionsMenuFromRedesignedMainMenu(composeTestRule: ComposeTestRule, interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            Log.i(TAG, "openExtensionsMenu: Trying to click the \"Extensions\" button")
            composeTestRule.noExtensionsButton().performClick()
            Log.i(TAG, "openExtensionsMenu: Clicked the \"Extensions\" button")
            composeTestRule.waitForIdle()

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }

        fun openPageViewExtensionsMenuFromRedesignedMainMenu(composeTestRule: ComposeTestRule, addonName: String, interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            Log.i(TAG, "openExtensionsMenu: Trying to click the \"Extensions\" button")
            composeTestRule.pageViewExtensionsButton(addonName).performClick()
            Log.i(TAG, "openExtensionsMenu: Clicked the \"Extensions\" button")
            composeTestRule.waitForIdle()

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }

        fun openHomeScreenExtensionsMenuFromRedesignedMainMenu(composeTestRule: ComposeTestRule, addonName: String, interact: SettingsSubMenuAddonsManagerRobot.() -> Unit): SettingsSubMenuAddonsManagerRobot.Transition {
            Log.i(TAG, "openExtensionsMenu: Trying to click the \"Extensions\" button")
            composeTestRule.homeScreenExtensionsButton(addonName).performClick()
            Log.i(TAG, "openExtensionsMenu: Clicked the \"Extensions\" button")
            composeTestRule.waitForIdle()

            SettingsSubMenuAddonsManagerRobot().interact()
            return SettingsSubMenuAddonsManagerRobot.Transition()
        }
    }
}
private fun ComposeTestRule.signInButton() = onNodeWithText(getStringResource(R.string.browser_menu_sign_in))

private fun ComposeTestRule.signInButtonDescription() = onNodeWithText(getStringResource(R.string.browser_menu_sign_in_caption))

private fun ComposeTestRule.customizeHomeButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_customize_home_1))

private fun ComposeTestRule.newPrivateTabButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_new_private_tab))

private fun ComposeTestRule.newTabButton() = onNodeWithContentDescription(getStringResource(R.string.library_new_tab))

private fun ComposeTestRule.helpButton() = onNodeWithContentDescription("Help")

private fun ComposeTestRule.settingsButton() = onNodeWithContentDescription("Settings")

private fun ComposeTestRule.noExtensionsButton() = onNodeWithContentDescription("ExtensionsNo extensions enabled")

private fun ComposeTestRule.homeScreenExtensionsButton(addonName: String) = onNodeWithContentDescription(addonName)

private fun ComposeTestRule.pageViewExtensionsButton(addonName: String) = onNodeWithContentDescription("Extensions$addonName")

private fun ComposeTestRule.bookmarksButton() = onNodeWithContentDescription(getStringResource(R.string.library_bookmarks))

private fun ComposeTestRule.historyButton() = onNodeWithContentDescription(getStringResource(R.string.library_history))

private fun ComposeTestRule.downloadsButton() = onNodeWithContentDescription(getStringResource(R.string.library_downloads))

private fun ComposeTestRule.passwordsButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_passwords))

private fun ComposeTestRule.whatsNewButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_new_in_firefox))

// Page main menu items

private fun ComposeTestRule.findInPageButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_find_in_page_2))

private fun ComposeTestRule.toolsMenuButton() = onNodeWithTag("mainMenu.tools")

private fun ComposeTestRule.saveMenuButton() = onNodeWithTag("mainMenu.save")

private fun ComposeTestRule.desktopSiteButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_switch_to_desktop_site))

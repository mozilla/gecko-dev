package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeUp
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule

class RedesignedMainMenuRobot(private val composeTestRule: HomeActivityComposeTestRule) {

    fun expandRedesignedMenu() {
        Log.i(TAG, "expandRedesignedMenu: Trying to perform swipe up action on the main menu.")
        composeTestRule
            .onNode(hasAnyChild(hasContentDescription("Bookmarks")))
            .performTouchInput { swipeUp() }
        composeTestRule.waitForIdle()
        Log.i(TAG, "expandRedesignedMenu: Performed swipe up action on the main menu.")
    }

    fun verifyHomeRedesignedMainMenuItems() {
        composeTestRule.signInButton().assertIsDisplayed()
        composeTestRule.signInButtonDescription().assertIsDisplayed()
        composeTestRule.helpButton().assertIsDisplayed()
        composeTestRule.settingsButton().assertIsDisplayed()
        composeTestRule.newTabButton().assertIsNotEnabled()
        composeTestRule.newPrivateTabButton().assertIsDisplayed()
        composeTestRule.extensionsButton().assertIsDisplayed()
        composeTestRule.bookmarksButton().assertIsDisplayed()
        composeTestRule.historyButton().assertIsDisplayed()
        composeTestRule.downloadsButton().assertIsDisplayed()
        composeTestRule.passwordsButton().assertIsDisplayed()
        composeTestRule.whatsNewButton().assertIsDisplayed()
        composeTestRule.customizeHomeButton().assertIsDisplayed()
    }

    fun verifySettingsButton() {
        composeTestRule.settingsButton().assertExists()
    }

    class Transition(private val composeTestRule: HomeActivityComposeTestRule) {
        fun openSettings(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the Settings button from the new main menu design.")
            composeTestRule.settingsButton().performClick()
            Log.i(TAG, "openSettings: Clicked the Settings button from the new main menu design.")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun openHelp(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openSettings: Trying to click the Settings button from the new main menu design.")
            composeTestRule.helpButton().performClick()
            Log.i(TAG, "openSettings: Clicked the Settings button from the new main menu design.")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
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

private fun ComposeTestRule.extensionsButton() = onNodeWithContentDescription("ExtensionsNo extensions enabled")

private fun ComposeTestRule.bookmarksButton() = onNodeWithContentDescription(getStringResource(R.string.library_bookmarks))

private fun ComposeTestRule.historyButton() = onNodeWithContentDescription(getStringResource(R.string.library_history))

private fun ComposeTestRule.downloadsButton() = onNodeWithContentDescription(getStringResource(R.string.library_downloads))

private fun ComposeTestRule.passwordsButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_passwords))

private fun ComposeTestRule.whatsNewButton() = onNodeWithContentDescription(getStringResource(R.string.browser_menu_new_in_firefox))

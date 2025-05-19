/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.helpers

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.pageObjects.HomePage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsAccessibilityPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsAutofillPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsCustomizePage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsHomepagePage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsPasswordsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsSearchPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsTabsPage

class PageContext(val composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) {
    val home = HomePage(composeRule)
    val settings = SettingsPage(composeRule)
    val settingsAccessibility = SettingsAccessibilityPage(composeRule)
    val settingsAutofill = SettingsAutofillPage(composeRule)
    val settingsCustomize = SettingsCustomizePage(composeRule)
    val settingsHomepage = SettingsHomepagePage(composeRule)
    val settingsPasswords = SettingsPasswordsPage(composeRule)
    val settingsSearch = SettingsSearchPage(composeRule)
    val settingsTabs = SettingsTabsPage(composeRule)

    fun initTestRule(
        skipOnboarding: Boolean = true,
        isMenuRedesignEnabled: Boolean = true,
        isMenuRedesignCFREnabled: Boolean = false,
        isPageLoadTranslationsPromptEnabled: Boolean = false,
    ): AndroidComposeTestRule<HomeActivityIntentTestRule, *> {
        return AndroidComposeTestRule(
            HomeActivityIntentTestRule(
                skipOnboarding = skipOnboarding,
                isMenuRedesignEnabled = isMenuRedesignEnabled,
                isMenuRedesignCFREnabled = isMenuRedesignCFREnabled,
                isPageLoadTranslationsPromptEnabled = isPageLoadTranslationsPromptEnabled,
            ),
        ) { it.activity }
    }
}

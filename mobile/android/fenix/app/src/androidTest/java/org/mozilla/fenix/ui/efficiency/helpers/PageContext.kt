/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.helpers

import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.ui.efficiency.pageObjects.BookmarksPage
import org.mozilla.fenix.ui.efficiency.pageObjects.BrowserPage
import org.mozilla.fenix.ui.efficiency.pageObjects.CollectionsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.CustomTabsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.DownloadsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.FindInPagePage
import org.mozilla.fenix.ui.efficiency.pageObjects.HistoryPage
import org.mozilla.fenix.ui.efficiency.pageObjects.HomePage
import org.mozilla.fenix.ui.efficiency.pageObjects.MainMenuComposePage
import org.mozilla.fenix.ui.efficiency.pageObjects.MainMenuPage
import org.mozilla.fenix.ui.efficiency.pageObjects.MicrosurveysPage
import org.mozilla.fenix.ui.efficiency.pageObjects.NotificationPage
import org.mozilla.fenix.ui.efficiency.pageObjects.ReaderViewPage
import org.mozilla.fenix.ui.efficiency.pageObjects.RecentlyClosedTabsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SearchBarComponent
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsAboutPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsAccessibilityPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsAddonsManagerPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsAutofillPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsCustomizePage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsDataCollectionPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsDeleteBrowsingDataOnQuitPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsDeleteBrowsingDataPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsEnhancedTrackingProtectionExceptionsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsEnhancedTrackingProtectionPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsExperimentsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsHTTPSOnlyModePage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsHomepagePage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsLanguagePage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsOpenLinksInAppsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsPasswordsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsPrivateBrowsingPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsSavePasswordsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsSavedPasswordsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsSearchPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsSiteSettingsExceptionsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsSiteSettingsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsTabsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsTranslationsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SettingsTurnOnSyncPage
import org.mozilla.fenix.ui.efficiency.pageObjects.ShareOverlayPage
import org.mozilla.fenix.ui.efficiency.pageObjects.ShortcutsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SitePermissionsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SiteSecurityPage
import org.mozilla.fenix.ui.efficiency.pageObjects.SystemSettingsPage
import org.mozilla.fenix.ui.efficiency.pageObjects.TabDrawerPage
import org.mozilla.fenix.ui.efficiency.pageObjects.ToolbarComponent
import org.mozilla.fenix.ui.efficiency.pageObjects.TranslationsPage

class PageContext(val composeRule: AndroidComposeTestRule<HomeActivityIntentTestRule, *>) {
    // Let's make sure we have them in a lexicographic order
    val bookmarks = BookmarksPage(composeRule)
    val browserPage = BrowserPage(composeRule)
    val collections = CollectionsPage(composeRule)
    val customTabs = CustomTabsPage(composeRule)
    val downloads = DownloadsPage(composeRule)
    val findInPage = FindInPagePage(composeRule)
    val history = HistoryPage(composeRule)
    val home = HomePage(composeRule)
    val mainMenu = MainMenuPage(composeRule)
    val mainMenuCompose = MainMenuComposePage(composeRule)
    val microsurveys = MicrosurveysPage(composeRule)
    val notifications = NotificationPage(composeRule)
    val readerView = ReaderViewPage(composeRule)
    val recentlyClosedTabs = RecentlyClosedTabsPage(composeRule)
    val searchBar = SearchBarComponent(composeRule)
    val settings = SettingsPage(composeRule)
    val settingsAbout = SettingsAboutPage(composeRule)
    val settingsAccessibility = SettingsAccessibilityPage(composeRule)
    val settingsAddonsManager = SettingsAddonsManagerPage(composeRule)
    val settingsAutofill = SettingsAutofillPage(composeRule)
    val settingsCustomize = SettingsCustomizePage(composeRule)
    val settingsDataCollection = SettingsDataCollectionPage(composeRule)
    val settingsDeleteBrowsingData = SettingsDeleteBrowsingDataPage(composeRule)
    val settingsDeleteBrowsingDataOnQuit = SettingsDeleteBrowsingDataOnQuitPage(composeRule)
    val settingsEnhancedTrackingProtection = SettingsEnhancedTrackingProtectionPage(composeRule)
    val settingsEnhancedTrackingProtectionExceptions = SettingsEnhancedTrackingProtectionExceptionsPage(composeRule)
    val settingsExperiments = SettingsExperimentsPage(composeRule)
    val settingsHomepage = SettingsHomepagePage(composeRule)
    val settingsHTTPSOnlyMode = SettingsHTTPSOnlyModePage(composeRule)
    val settingsLanguage = SettingsLanguagePage(composeRule)
    val settingsOpenLinksInApps = SettingsOpenLinksInAppsPage(composeRule)
    val settingsPasswords = SettingsPasswordsPage(composeRule)
    val settingsPrivateBrowsing = SettingsPrivateBrowsingPage(composeRule)
    val settingsSavePasswords = SettingsSavePasswordsPage(composeRule)
    val settingsSavedPasswords = SettingsSavedPasswordsPage(composeRule)
    val settingsSearch = SettingsSearchPage(composeRule)
    val settingsSiteSettings = SettingsSiteSettingsPage(composeRule)
    val settingsSiteSettingsExceptions = SettingsSiteSettingsExceptionsPage(composeRule)
    val settingsTabs = SettingsTabsPage(composeRule)
    val settingsTranslations = SettingsTranslationsPage(composeRule)
    val settingsTurnOnSync = SettingsTurnOnSyncPage(composeRule)
    val sitePermissions = SitePermissionsPage(composeRule)
    val siteSecurity = SiteSecurityPage(composeRule)
    val shareOverlay = ShareOverlayPage(composeRule)
    val shortcuts = ShortcutsPage(composeRule)
    val systemSettings = SystemSettingsPage(composeRule)
    val tabDrawer = TabDrawerPage(composeRule)
    val toolbar = ToolbarComponent(composeRule)
    val translations = TranslationsPage(composeRule)

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

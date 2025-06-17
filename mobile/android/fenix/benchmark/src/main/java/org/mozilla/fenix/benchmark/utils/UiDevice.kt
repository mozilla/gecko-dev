/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.benchmark.utils

import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector

const val WAITING_TIME_MS = 15000L

fun UiDevice.clearPackageData(packageName: String) {
    executeShellCommand("pm clear $packageName")
    executeShellCommand("pm revoke $packageName android.permission.POST_NOTIFICATIONS")
}

fun UiDevice.isOnboardingCompleted() : Boolean {
    val dismissSetAsDefault = findObject(UiSelector().resourceId("android:id/button2"))
    return !dismissSetAsDefault.waitForExists(WAITING_TIME_MS)
}

fun UiDevice.isWallpaperOnboardingShown() : Boolean {
    val wallpaperFragmentText = findObject(
        UiSelector().text("Choose a wallpaper that speaks to you.")
    )
    return wallpaperFragmentText.exists()
}

fun UiDevice.dismissWallpaperOnboarding() {
    val closeButton = findObject(
        UiSelector().description("Close tab")
    )
    closeButton.click()
}

fun UiDevice.dismissCookieBannerBlockerCFR() {
    val cfrDismiss = findObject(
        UiSelector().resourceId("cfr.dismiss")
    )
    if (cfrDismiss.exists()) {
        cfrDismiss.click()
    }
}

fun UiDevice.completeOnboarding() {
    val dismissSetAsDefault = findObject(UiSelector().resourceId("android:id/button2"))
    dismissSetAsDefault.waitForExists(WAITING_TIME_MS)
    dismissSetAsDefault.click()

    val dismissFirefoxSearchWidget = findObject(UiSelector().text("Not now"))
    dismissFirefoxSearchWidget.waitForExists(WAITING_TIME_MS)
    dismissFirefoxSearchWidget.click()

    val dismissSignInOnboarding = findObject(UiSelector().text("Not now"))
    dismissSignInOnboarding.waitForExists(WAITING_TIME_MS)
    dismissSignInOnboarding.click()

    val enableNotificationOnboarding = findObject(UiSelector().text("Turn on notifications"))
    enableNotificationOnboarding.waitForExists(WAITING_TIME_MS)
    enableNotificationOnboarding.click()

    val systemAllow = findObject(UiSelector().text("Allow"))
    systemAllow.waitForExists(WAITING_TIME_MS)
    systemAllow.click()
}

fun UiDevice.completeBrowserJourney(packageName: String) {
    if (isWallpaperOnboardingShown()) {
        dismissWallpaperOnboarding()
    }

    openSponsoredShortcut(shortcutName = "Google")

    openTabsTray(packageName = packageName)

    openNewTabOnTabsTray()

    loadSite(packageName = packageName, url = "getpocket.com")

    openTabsTray(packageName = packageName)

    openNewTabOnTabsTray()

    loadSite(packageName = packageName, url = "mozilla.org")

    // Scroll down
    val webScroll = UiScrollable(UiSelector().resourceId("$packageName:id/engineView"))
    webScroll.waitForExists(WAITING_TIME_MS)
    webScroll.flingToEnd(1)

    // Scroll up
    webScroll.flingToBeginning(1)

    openTabsTray(packageName = packageName)

    switchTabs(siteName = "Google", newTabUrl = "google.com")

    openTabsTray(packageName = packageName)

    openNewPrivateTabOnTabsTray()

    loadSite(packageName = packageName, url = "google.com")

    loadSite(packageName = packageName, url = "mozilla.org")

    openTabsTray(packageName = packageName)

    closeAllTabs()

    togglePBMOnHome()

    if (isWallpaperOnboardingShown()) {
        dismissWallpaperOnboarding()
    }

    // Scroll down until the end on homepage
    val homeScroll = UiScrollable(UiSelector().resourceId("$packageName:id/rootContainer"))
    homeScroll.waitForExists(WAITING_TIME_MS)
    homeScroll.flingToEnd(Int.MAX_VALUE)

    openTabsTray(packageName = packageName)

    closeAllTabs()
}

fun UiDevice.waitUntilPageLoaded() {
    // Refresh icon toggles between refresh and stop; refresh is only shown after page has loaded
    val refresh = findObject(
        UiSelector().description("Refresh"),
    )
    refresh.waitForExists(WAITING_TIME_MS)
}

fun UiDevice.openTabsTray(packageName: String) {
    val tabsTrayButton = findObject(
        UiSelector().resourceId("$packageName:id/counter_box")
    )
    tabsTrayButton.waitForExists(WAITING_TIME_MS)
    tabsTrayButton.click()
}

fun UiDevice.openNewTabOnTabsTray() {
    val newTabFab = findObject(
        UiSelector().description("Add tab"),
    )
    newTabFab.waitForExists(WAITING_TIME_MS)
    newTabFab.click()

    if (isWallpaperOnboardingShown()) {
        dismissWallpaperOnboarding()
    }
}

fun UiDevice.openNewPrivateTabOnTabsTray() {
    val pbmButton = findObject(
        UiSelector().descriptionStartsWith("Private Tabs Open:")
    )
    pbmButton.waitForExists(WAITING_TIME_MS)
    pbmButton.click()

    val newTabButton = findObject(
        UiSelector().description("Add private tab"),
    )
    newTabButton.waitForExists(WAITING_TIME_MS)
    newTabButton.click()
}

fun UiDevice.switchTabs(siteName: String, newTabUrl: String) {
    var newTabGridItem = findObject(
        UiSelector().text(siteName)
    )

    if (newTabGridItem.waitForExists(WAITING_TIME_MS)) {
        newTabGridItem.click()
    } else {
        newTabGridItem = findObject(
            UiSelector().text(newTabUrl)
        )
        newTabGridItem.waitForExists(WAITING_TIME_MS)
        newTabGridItem.click()
    }

    waitUntilPageLoaded()
}

fun UiDevice.closeAllTabs() {
    val contextualMenu = findObject(
        UiSelector().description("Open tabs menu")
    )
    contextualMenu.waitForExists(WAITING_TIME_MS)
    contextualMenu.click()

    val closeAllTabsButton = findObject(
        UiSelector().text("Close all tabs")
    )
    closeAllTabsButton.waitForExists(WAITING_TIME_MS)
    closeAllTabsButton.click()
}

fun UiDevice.openSponsoredShortcut(shortcutName: String) {
    val shortcut = findObject(
        UiSelector().text(shortcutName)
    )
    shortcut.waitForExists(WAITING_TIME_MS)
    shortcut.click()

    waitUntilPageLoaded()
}

fun UiDevice.togglePBMOnHome() {
    val pbmButton = findObject(
        UiSelector().description("Private browsing")
    )
    pbmButton.waitForExists(WAITING_TIME_MS)
    pbmButton.click()
}

fun UiDevice.loadSite(packageName: String, url: String) {
    val urlBar = findObject(
        UiSelector().resourceId("$packageName:id/toolbar")
    )
    urlBar.waitForExists(WAITING_TIME_MS)
    urlBar.click()

    val awesomeBar = findObject(
        UiSelector().resourceId("$packageName:id/mozac_browser_toolbar_edit_url_view")
    )
    awesomeBar.setText(url)
    pressEnter()

    waitUntilPageLoaded()

    dismissCookieBannerBlockerCFR()
}

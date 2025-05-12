/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.os.Build
import android.util.Log
import androidx.core.content.pm.PackageInfoCompat
import androidx.test.espresso.Espresso
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.hasDescendant
import androidx.test.espresso.matcher.ViewMatchers.hasMinimumChildCount
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withContentDescription
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.hamcrest.CoreMatchers.allOf
import org.hamcrest.CoreMatchers.containsString
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.LISTS_MAXSWIPES
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.settings.SupportUtils

/**
 * Implementation of Robot Pattern for the settings search sub menu.
 */
class SettingsSubMenuAboutRobot {
    fun verifyAboutFirefoxPreviewInfo() {
        verifyVersionNumber()
        verifyProductCompany()
        verifyCurrentTimestamp()
        verifyTheLinksList()
    }

    fun verifyVersionNumber() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext

        val packageInfo = context.packageManager.getPackageInfoCompat(context.packageName, 0)
        val versionCode = PackageInfoCompat.getLongVersionCode(packageInfo).toString()
        val buildNVersion = "${packageInfo.versionName} (Build #$versionCode)\n"
        val geckoVersion =
            org.mozilla.geckoview.BuildConfig.MOZ_APP_VERSION + "-" + org.mozilla.geckoview.BuildConfig.MOZ_APP_BUILDID
        val asVersion = mozilla.components.Build.APPLICATION_SERVICES_VERSION
        val osVersion = Build.VERSION.RELEASE
        Log.i(TAG, "verifyVersionNumber: Trying to verify that the about section contains build version: $buildNVersion")
        onView(withId(R.id.about_text)).check(matches(withText(containsString(buildNVersion))))
        Log.i(TAG, "verifyVersionNumber: Verified that the about section contains build version: $buildNVersion")
        Log.i(TAG, "verifyVersionNumber: Trying to verify that the about section contains gecko version: $geckoVersion")
        onView(withId(R.id.about_text)).check(matches(withText(containsString(geckoVersion))))
        Log.i(TAG, "verifyVersionNumber: Verified that the about section contains gecko version: $geckoVersion")
        Log.i(TAG, "verifyVersionNumber: Trying to verify that the about section contains android services version: $asVersion")
        onView(withId(R.id.about_text)).check(matches(withText(containsString(asVersion))))
        Log.i(TAG, "verifyVersionNumber: Verified that the about section contains android services version: $asVersion")
        Log.i(TAG, "verifyVersionNumber: Trying to verify that the about section contains Android version: $osVersion")
        onView(withId(R.id.about_text)).check(matches(withText(containsString("Android $osVersion"))))
        Log.i(TAG, "verifyVersionNumber: Verified that the about section contains Android version: $osVersion")
    }

    fun verifyProductCompany() {
        Log.i(TAG, "verifyProductCompany: Trying to verify that the about section contains the company that produced the app info: ${"$appName is produced by Mozilla."}")
        onView(withId(R.id.about_content))
            .check(matches(withText(containsString("$appName is produced by Mozilla."))))
        Log.i(TAG, "verifyProductCompany: Verified that the about section contains the company that produced the app info: \"$appName is produced by Mozilla.\"")
    }

    fun verifyCurrentTimestamp() {
        Log.i(TAG, "verifyCurrentTimestamp: Trying to verify that the about section contains \"debug build\"")
        onView(withId(R.id.build_date))
            // Currently UI tests run against debug builds, which display a hard-coded string 'debug build'
            // instead of the date. See https://github.com/mozilla-mobile/fenix/pull/10812#issuecomment-633746833
            .check(matches(withText(containsString("debug build"))))
        Log.i(TAG, "verifyCurrentTimestamp: Verified that the about section contains \"debug build\"")
    }

    fun verifyAboutToolbar() {
        Log.i(TAG, "verifyAboutToolbar: Trying to verify that the \"About $appName\" toolbar title is visible")
        onView(
            allOf(
                withId(R.id.navigationToolbar),
                hasDescendant(withText("About $appName")),
            ),
        ).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyAboutToolbar: Verified that the \"About $appName\" toolbar title is visible")
    }

    fun verifyWhatIsNewInFirefoxLink() {
        Log.i(TAG, "verifyWhatIsNewInFirefoxLink: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)
        Log.i(TAG, "verifyWhatIsNewInFirefoxLink: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")

        val firefox = TestHelper.appContext.getString(R.string.firefox)
        Log.i(TAG, "verifyWhatIsNewInFirefoxLink: Trying to verify that the \"What’s new in $firefox\" link is visible")
        onView(withText("What’s new in $firefox")).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyWhatIsNewInFirefoxLink: Verified that the \"What’s new in $firefox\" link is visible")
        Log.i(TAG, "verifyWhatIsNewInFirefoxLink: Trying to click the \"What’s new in $firefox\" link")
        onView(withText("What’s new in $firefox")).perform(click())
        Log.i(TAG, "verifyWhatIsNewInFirefoxLink: Clicked the \"What’s new in $firefox\" link")
    }
    fun verifySupportLink() {
        Log.i(TAG, "verifySupport: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)
        Log.i(TAG, "verifySupport: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        Log.i(TAG, "verifySupport: Trying to verify that the \"Support\" link is visible")
        onView(withText("Support")).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifySupport: Verified that the \"Support\" link is visible")
        Log.i(TAG, "verifySupport: Trying to click the \"Support\" link")
        onView(withText("Support")).perform(click())
        Log.i(TAG, "verifySupport: Clicked the \"Support\" link")

        browserScreen {
            verifyHelpUrl()
        }
    }

    fun verifyCrashesLink() {
        navigationToolbar {
        }.openThreeDotMenu {
        }.openSettings {
        }.openAboutFirefoxPreview {}
        Log.i(TAG, "verifyCrashesLink: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)
        Log.i(TAG, "verifyCrashesLink: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        Log.i(TAG, "verifyCrashesLink: Trying to verify that the \"Crashes\" link is visible")
        onView(withText("Crashes")).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyCrashesLink: Verified that the \"Crashes\" link is visible")
        Log.i(TAG, "verifyCrashesLink: Trying to click the \"Crashes\" link")
        onView(withText("Crashes")).perform(click())
        Log.i(TAG, "verifyCrashesLink: Clicked the \"Crashes\" link")

        assertUIObjectExists(itemContainingText("No crash reports have been submitted."))

        for (i in 1..3) {
            Log.i(TAG, "verifyCrashesLink: Trying to perform press back action")
            Espresso.pressBack()
            Log.i(TAG, "verifyCrashesLink: Performed press back action")
        }
    }

    fun verifyPrivacyNoticeLink() {
        Log.i(TAG, "verifyPrivacyNoticeLink: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)
        Log.i(TAG, "verifyPrivacyNoticeLink: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        Log.i(TAG, "verifyPrivacyNoticeLink: Trying to verify that the \"Privacy notice\" link is visible")
        onView(withText("Privacy notice")).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyPrivacyNoticeLink: Verified that the \"Privacy notice\" link is visible")
        Log.i(TAG, "verifyPrivacyNoticeLink: Trying to click the \"Privacy notice\" link")
        onView(withText("Privacy notice")).perform(click())
        Log.i(TAG, "verifyPrivacyNoticeLink: Clicked the \"Privacy notice\" link")

        browserScreen {
            verifyUrl("/privacy/firefox")
        }
    }

    fun verifyKnowYourRightsLink() {
        Log.i(TAG, "verifyKnowYourRightsLink: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)
        Log.i(TAG, "verifyKnowYourRightsLink: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        Log.i(TAG, "verifyKnowYourRightsLink: Trying to verify that the \"Know your rights\" link is visible")
        onView(withText("Know your rights")).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyKnowYourRightsLink: Verified that the \"Know your rights\" link is visible")
        Log.i(TAG, "verifyKnowYourRightsLink: Trying to click the \"Know your rights\" link")
        onView(withText("Know your rights")).perform(click())
        Log.i(TAG, "verifyKnowYourRightsLink: Clicked the \"Know your rights\" link")

        browserScreen {
            verifyUrl(SupportUtils.SumoTopic.YOUR_RIGHTS.topicStr)
        }
    }

    fun verifyLicensingInformationLink() {
        Log.i(TAG, "verifyLicensingInformationLink: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)
        Log.i(TAG, "verifyLicensingInformationLink: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        Log.i(TAG, "verifyLicensingInformationLink: Trying to verify that the \"Licensing information\" link is visible")
        onView(withText("Licensing information")).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyLicensingInformationLink: Verified that the \"Licensing information\" link is visible")
        Log.i(TAG, "verifyLicensingInformationLink: Trying to click the \"Licensing information\" link")
        onView(withText("Licensing information")).perform(click())
        Log.i(TAG, "verifyLicensingInformationLink: Clicked the \"Licensing information\" link")

        browserScreen {
            verifyUrl("about:license")
        }
    }

    fun verifyLibrariesUsedLink() {
        Log.i(TAG, "verifyLibrariesUsedLink: Trying to perform ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        aboutMenuList.scrollToEnd(LISTS_MAXSWIPES)
        Log.i(TAG, "verifyLibrariesUsedLink: Performed ${LISTS_MAXSWIPES}x a scroll action to the end of the about list")
        Log.i(TAG, "verifyLibrariesUsedLink: Trying to verify that the \"Libraries that we use\" link is visible")
        onView(withText("Libraries that we use")).check(matches(withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)))
        Log.i(TAG, "verifyLibrariesUsedLink: Verified that the \"Libraries that we use\" link is visible")
    }

    fun verifyTheLibrariesListNotEmpty() {
        Log.i(TAG, "verifyLibrariesUsedLink: Trying to click the \"Libraries that we use\" link")
        onView(withText("Libraries that we use")).perform(click())
        Log.i(TAG, "verifyLibrariesUsedLink: Clicked the \"Libraries that we use\" link")
        Log.i(TAG, "verifyLibrariesUsedLink: Trying to verify that the toolbar has title: \"$appName | OSS Libraries\"")
        onView(withId(R.id.navigationToolbar)).check(
            matches(
                hasDescendant(
                    withText(
                        containsString("$appName | OSS Libraries"),
                    ),
                ),
            ),
        )
        Log.i(TAG, "verifyLibrariesUsedLink: Verified that the toolbar has title: \"$appName | OSS Libraries\"")
        Log.i(TAG, "verifyTheLibrariesListNotEmpty: Trying to verify that the OSS Libraries list has more then 10 items.")
        onView(withId(R.id.about_libraries_listview))
            .check(matches(isDisplayed()))
            .check(matches(hasMinimumChildCount(10)))
        Log.i(TAG, "verifyTheLibrariesListNotEmpty: Verify that the OSS Libraries list has more then 10 items.")
    }

    fun verifyTheLinksList() {
        verifyAboutToolbar()
        verifyWhatIsNewInFirefoxLink()
        navigateBackToAboutPage()
        verifySupportLink()
        verifyCrashesLink()
        navigateBackToAboutPage()
        verifyPrivacyNoticeLink()
        navigateBackToAboutPage()
        verifyKnowYourRightsLink()
        navigateBackToAboutPage()
        verifyLicensingInformationLink()
        navigateBackToAboutPage()
        verifyLibrariesUsedLink()
    }

    class Transition {
        fun goBack(interact: SettingsRobot.() -> Unit): SettingsRobot.Transition {
            Log.i(TAG, "goBack: Trying to click the navigate up button")
            goBackButton().perform(click())
            Log.i(TAG, "goBack: Clicked the navigate up button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }
    }
}

private fun navigateBackToAboutPage() {
    navigationToolbar {
    }.openThreeDotMenu {
    }.openSettings {
    }.openAboutFirefoxPreview {
    }
}

private val aboutMenuList = UiScrollable(UiSelector().resourceId("$packageName:id/about_layout"))

private fun goBackButton() =
    onView(withContentDescription("Navigate up"))

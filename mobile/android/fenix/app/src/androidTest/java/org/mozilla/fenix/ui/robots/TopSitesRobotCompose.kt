/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.filter
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performTouchInput
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.home.topsites.TopSitesTestTag

/**
 * Implementation of Robot Pattern for the Compose Top Sites.
 */
class TopSitesRobotCompose(private val composeTestRule: HomeActivityComposeTestRule) {

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSitesList() {
        Log.i(TAG, "verifyExistingTopSitesList: Waiting for $waitingTime ms until the top sites list exists")
        composeTestRule.waitUntilAtLeastOneExists(hasTestTag(TopSitesTestTag.topSites), timeoutMillis = waitingTime)
        Log.i(TAG, "verifyExistingTopSitesList: Waited for $waitingTime ms until the top sites list to exists")
    }

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            Log.i(TAG, "verifyExistingTopSiteItem: Waiting for $waitingTime ms until the top site with title: $title exists")
            composeTestRule.waitUntilAtLeastOneExists(
                hasTestTag(TopSitesTestTag.topSiteItemRoot).and(hasAnyChild(hasText(title))),
                timeoutMillis = waitingTimeLong,
            )
            Log.i(TAG, "verifyExistingTopSiteItem: Waited for $waitingTimeLong ms until the top site with title: $title exists")
            Log.i(TAG, "verifyExistingTopSiteItem: Trying to verify that the top site with title: $title exists")
            composeTestRule.topSiteItem(title).assertExists()
            Log.i(TAG, "verifyExistingTopSiteItem: Verified that the top site with title: $title exists")
        }
    }

    fun verifyNotExistingTopSiteItem(vararg titles: String) {
        titles.forEach { title ->
            Log.i(TAG, "verifyNotExistingTopSiteItem: Waiting for $waitingTime ms for top site with title: $title to exist")
            itemContainingText(title).waitForExists(waitingTime)
            Log.i(TAG, "verifyNotExistingTopSiteItem: Waited for $waitingTime ms for top site with title: $title to exist")
            Log.i(TAG, "verifyNotExistingTopSiteItem: Trying to verify that top site with title: $title does not exist")
            composeTestRule.topSiteItem(title).assertDoesNotExist()
            Log.i(TAG, "verifyNotExistingTopSiteItem: Verified that top site with title: $title does not exist")
        }
    }

    fun verifyTopSiteContextMenuItems() {
        verifyTopSiteContextMenuOpenInPrivateTabButton()
        verifyTopSiteContextMenuRemoveButton()
        verifyTopSiteContextMenuEditButton()
    }

    fun verifyTopSiteContextMenuOpenInPrivateTabButton() {
        Log.i(TAG, "verifyTopSiteContextMenuOpenInPrivateTabButton: Trying to verify that the \"Open in private tab\" menu button exists")
        composeTestRule.contextMenuItemOpenInPrivateTab().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuOpenInPrivateTabButton: Verified that the \"Open in private tab\" menu button exists")
    }

    fun verifyTopSiteContextMenuEditButton() {
        Log.i(TAG, "verifyTopSiteContextMenuEditButton: Trying to verify that the \"Edit\" menu button exists")
        composeTestRule.contextMenuItemEdit().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuEditButton: Verified that the \"Edit\" menu button exists")
    }

    fun verifyTopSiteContextMenuRemoveButton() {
        Log.i(TAG, "verifyTopSiteContextMenuRemoveButton: Trying to verify that the \"Remove\" menu button exists")
        composeTestRule.contextMenuItemRemove().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuRemoveButton: Verified that the \"Remove\" menu button exists")
    }

    fun verifyTopSiteContextMenuUrlErrorMessage() {
        Log.i(TAG, "verifyTopSiteContextMenuUrlMessage: Waiting for $waitingTime ms for \"Enter a valid URL\" error message to exist")
        itemContainingText(getStringResource(R.string.top_sites_edit_dialog_url_error)).waitForExists(waitingTime)
        Log.i(TAG, "verifyTopSiteContextMenuUrlMessage: Waited for $waitingTime ms for \"Enter a valid URL\" error message to exist")
    }

    class Transition(private val composeTestRule: HomeActivityComposeTestRule) {

        fun openTopSiteTabWithTitle(
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteTabWithTitle: Trying to scroll to top site with title: $title")
            composeTestRule.topSiteItem(title).performScrollTo()
            Log.i(TAG, "openTopSiteTabWithTitle: Scrolled to top site with title: $title")
            Log.i(TAG, "openTopSiteTabWithTitle: Trying to click top site with title: $title")
            composeTestRule.topSiteItem(title).performClick()
            Log.i(TAG, "openTopSiteTabWithTitle: Clicked top site with title: $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openTopSiteInPrivate(
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteInPrivate: Trying to click the \"Open in private tab\" menu button")
            composeTestRule.contextMenuItemOpenInPrivateTab().performClick()
            Log.i(TAG, "openTopSiteInPrivate: Clicked the \"Open in private tab\" menu button")
            composeTestRule.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun openContextMenuOnTopSitesWithTitle(
            title: String,
            interact: TopSitesRobotCompose.() -> Unit,
        ): Transition {
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Trying to scroll to top site with title: $title")
            composeTestRule.topSiteItem(title).performScrollTo()
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Scrolled to top site with title: $title")
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Trying to long click top site with title: $title")
            composeTestRule.topSiteItem(title).performTouchInput { longClick() }
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Long clicked top site with title: $title")

            TopSitesRobotCompose(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        fun editTopSite(
            title: String,
            url: String,
            interact: TopSitesRobotCompose.() -> Unit,
        ): Transition {
            Log.i(TAG, "editTopSite: Trying to click the \"Edit\" menu button")
            composeTestRule.contextMenuItemEdit().performClick()
            Log.i(TAG, "editTopSite: Clicked the \"Edit\" menu button")
            itemWithResId("$packageName:id/top_site_title")
                .also {
                    Log.i(TAG, "editTopSite: Waiting for $waitingTimeShort ms for top site name text box to exist")
                    it.waitForExists(waitingTimeShort)
                    Log.i(TAG, "editTopSite: Waited for $waitingTimeShort ms for top site name text box to exist")
                    Log.i(TAG, "editTopSite: Trying to set top site name text box text to: $title")
                    it.setText(title)
                    Log.i(TAG, "editTopSite: Top site name text box text was set to: $title")
                }
            itemWithResId("$packageName:id/top_site_url")
                .also {
                    Log.i(TAG, "editTopSite: Waiting for $waitingTimeShort ms for top site url text box to exist")
                    it.waitForExists(waitingTimeShort)
                    Log.i(TAG, "editTopSite: Waited for $waitingTimeShort ms for top site url text box to exist")
                    Log.i(TAG, "editTopSite: Trying to set top site url text box text to: $url")
                    it.setText(url)
                    Log.i(TAG, "editTopSite: Top site url text box text was set to: $url")
                }
            Log.i(TAG, "editTopSite: Trying to click the \"Save\" dialog button")
            itemWithResIdContainingText("android:id/button1", "Save").click()
            Log.i(TAG, "editTopSite: Clicked the \"Save\" dialog button")

            TopSitesRobotCompose(composeTestRule).interact()
            return Transition(composeTestRule)
        }

        @OptIn(ExperimentalTestApi::class)
        fun removeTopSite(
            interact: TopSitesRobotCompose.() -> Unit,
        ): Transition {
            Log.i(TAG, "removeTopSite: Trying to click the \"Remove\" menu button")
            composeTestRule.contextMenuItemRemove().performClick()
            Log.i(TAG, "removeTopSite: Clicked the \"Remove\" menu button")
            Log.i(TAG, "removeTopSite: Waiting for $waitingTime ms until the \"Remove\" menu button does not exist")
            composeTestRule.waitUntilDoesNotExist(hasTestTag(TopSitesTestTag.remove), waitingTime)
            Log.i(TAG, "removeTopSite: Waited for $waitingTime ms until the \"Remove\" menu button does not exist")

            TopSitesRobotCompose(composeTestRule).interact()
            return Transition(composeTestRule)
        }
    }
}

/**
 * Obtains the top site with the provided [title].
 */
private fun ComposeTestRule.topSiteItem(title: String) =
    onAllNodesWithTag(TopSitesTestTag.topSiteItemRoot).filter(hasAnyChild(hasText(title))).onFirst()

/**
 * Obtains the option to open in private tab the top site
 */
private fun ComposeTestRule.contextMenuItemOpenInPrivateTab() =
    onAllNodesWithTag(TopSitesTestTag.openInPrivateTab).onFirst()

/**
 * Obtains the option to edit the top site
 */
private fun ComposeTestRule.contextMenuItemEdit() = onAllNodesWithTag(TopSitesTestTag.edit).onFirst()

/**
 * Obtains the option to remove the top site
 */
private fun ComposeTestRule.contextMenuItemRemove() = onAllNodesWithTag(TopSitesTestTag.remove).onFirst()

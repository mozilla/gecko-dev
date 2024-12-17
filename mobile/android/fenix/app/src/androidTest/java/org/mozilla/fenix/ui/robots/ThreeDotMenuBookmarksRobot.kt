/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.matcher.ViewMatchers.withText
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.click

/**
 * Implementation of Robot Pattern for the Bookmarks three dot menu.
 */
class ThreeDotMenuBookmarksRobot {

    class Transition {

        fun clickEdit(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickEdit: Trying to click the \"Edit\" button")
            editButton().click()
            Log.i(TAG, "clickEdit: Clicked the \"Edit\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickEditFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickEditFromRedesignedBookmarksMenu: Trying to click the \"Edit\" button")
            redesignedBookmarkMenuEditButton(composeTestRule).performClick()
            Log.i(TAG, "clickEditFromRedesignedBookmarksMenu: Clicked the \"Edit\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickCopy(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickCopy: Trying to click the \"Copy\" button")
            copyButton().click()
            Log.i(TAG, "clickCopy: Clicked the \"Copy\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickShare(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickShare: Trying to click the \"Share\" button")
            shareButton().click()
            Log.i(TAG, "clickShare: Clicked the \"Share\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickShareFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            Log.i(TAG, "clickShareFromRedesignedBookmarksMenu: Trying to click the \"Share\" button")
            redesignedBookmarkMenuShareButton(composeTestRule).performClick()
            Log.i(TAG, "clickShareFromRedesignedBookmarksMenu: Clicked the \"Share\" button")

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun clickOpenInNewTab(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInNewTab: Trying to click the \"Open in new tab\" button")
            openInNewTabButton().click()
            Log.i(TAG, "clickOpenInNewTab: Clicked the \"Open in new tab\" button")

            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenInPrivateTab(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenInPrivateTab: Trying to click the \"Open in private tab\" button")
            openInPrivateTabButton().click()
            Log.i(TAG, "clickOpenInPrivateTab: Clicked the \"Open in private tab\" button")

            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenAllInTabs(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenAllInTabs: Trying to click the \"Open all in new tabs\" button")
            openAllInTabsButton().click()
            Log.i(TAG, "clickOpenAllInTabs: Clicked the \"Open all in new tabs\" button")

            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun clickOpenAllInPrivateTabs(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickOpenAllInPrivateTabs: Trying to click the \"Open all in private tabs\" button")
            openAllInPrivateTabsButton().click()
            Log.i(TAG, "clickOpenAllInPrivateTabs: Clicked the \"Open all in private tabs\" button")

            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun clickDelete(interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickDelete: Trying to click the \"Delete\" button")
            deleteButton().click()
            Log.i(TAG, "clickDelete: Clicked the \"Delete\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }

        fun clickDeleteFromRedesignedBookmarksMenu(composeTestRule: ComposeTestRule, interact: BookmarksRobot.() -> Unit): BookmarksRobot.Transition {
            Log.i(TAG, "clickDeleteFromRedesignedBookmarksMenu: Trying to click the \"Delete\" button")
            redesignedBookmarkMenuDeleteButton(composeTestRule).performClick()
            Log.i(TAG, "clickDeleteFromRedesignedBookmarksMenu: Clicked the \"Delete\" button")

            BookmarksRobot().interact()
            return BookmarksRobot.Transition()
        }
    }
}

private fun editButton() = onView(withText("Edit"))

private fun redesignedBookmarkMenuEditButton(composeTestRule: ComposeTestRule) = composeTestRule.onNodeWithText(getStringResource(R.string.bookmark_menu_edit_button))

private fun copyButton() = onView(withText("Copy"))

private fun shareButton() = onView(withText("Share"))

private fun redesignedBookmarkMenuShareButton(composeTestRule: ComposeTestRule) = composeTestRule.onNodeWithText(getStringResource(R.string.bookmark_menu_share_button))

private fun openInNewTabButton() = onView(withText("Open in new tab"))

private fun openInPrivateTabButton() = onView(withText("Open in private tab"))

private fun openAllInTabsButton() = onView(withText("Open all in new tabs"))

private fun openAllInPrivateTabsButton() = onView(withText("Open all in private tabs"))

private fun deleteButton() = onView(withText("Delete"))

private fun redesignedBookmarkMenuDeleteButton(composeTestRule: ComposeTestRule) = composeTestRule.onNodeWithText(getStringResource(R.string.bookmark_menu_delete_button))

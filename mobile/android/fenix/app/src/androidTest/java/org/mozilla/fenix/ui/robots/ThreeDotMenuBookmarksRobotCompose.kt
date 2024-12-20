package org.mozilla.fenix.ui.robots

import android.util.Log
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource

class ThreeDotMenuBookmarksRobotCompose() {
    class Transition(private val composeTestRule: ComposeTestRule) {
        fun clickDelete(interact: BookmarksRobotCompose.() -> Unit): BookmarksRobotCompose.Transition {
            Log.i(TAG, "clickDelete: Trying to click the \"Delete\" button")
            composeTestRule.deleteButton().performClick()
            Log.i(TAG, "clickDelete: Clicked the \"Delete\" button")

            BookmarksRobotCompose(composeTestRule).interact()
            return BookmarksRobotCompose.Transition(composeTestRule)
        }

        fun clickShare(interact: ShareOverlayRobot.() -> Unit): ShareOverlayRobot.Transition {
            Log.i(TAG, "clickShare: Trying to click the \"Share\" button")
            composeTestRule.shareButton().performClick()
            Log.i(TAG, "clickShare: Clicked the \"Share\" button")

            ShareOverlayRobot().interact()
            return ShareOverlayRobot.Transition()
        }

        fun clickEdit(interact: BookmarksRobotCompose.() -> Unit): BookmarksRobotCompose.Transition {
            Log.i(TAG, "clickEdit: Trying to click the \"Edit\" button")
            composeTestRule.editButton().performClick()
            Log.i(TAG, "clickEdit: Clicked the \"Edit\" button")

            BookmarksRobotCompose(composeTestRule).interact()
            return BookmarksRobotCompose.Transition(composeTestRule)
        }
    }
}

private fun ComposeTestRule.editButton() = onNodeWithText(getStringResource(R.string.bookmark_menu_edit_button))

private fun ComposeTestRule.shareButton() = onNodeWithText(getStringResource(R.string.bookmark_menu_share_button))

private fun ComposeTestRule.deleteButton() = onNodeWithText(getStringResource(R.string.bookmark_menu_delete_button))

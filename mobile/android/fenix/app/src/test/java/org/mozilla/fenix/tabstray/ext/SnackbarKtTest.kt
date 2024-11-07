/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import android.content.Context
import android.widget.FrameLayout
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.SnackbarBehavior
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.compose.snackbar.Snackbar
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.helpers.MockkRetryTestRule
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SnackbarKtTest {

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Test
    fun `GIVEN the snackbar is a child of dynamic container WHEN it is shown THEN enable the dynamic behavior`() {
        val container = FrameLayout(testContext).apply {
            id = R.id.dynamicSnackbarContainer
            layoutParams = CoordinatorLayout.LayoutParams(0, 0)
        }
        val settings: Settings = mockk(relaxed = true) {
            every { toolbarPosition } returns ToolbarPosition.BOTTOM
        }
        mockkStatic("org.mozilla.fenix.ext.ContextKt") {
            every { any<Context>().settings() } returns settings

            Snackbar.make(
                snackBarParentView = container,
                snackbarState = SnackbarState(message = "test"),
            )

            val behavior = (container.layoutParams as? CoordinatorLayout.LayoutParams)?.behavior
            assertTrue(behavior is SnackbarBehavior)
            assertEquals(ToolbarPosition.BOTTOM, (behavior as? SnackbarBehavior)?.toolbarPosition)
        }
    }
}

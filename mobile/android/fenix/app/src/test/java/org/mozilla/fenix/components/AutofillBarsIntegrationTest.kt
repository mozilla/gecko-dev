/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import mozilla.components.feature.prompts.login.SuggestStrongPasswordBar
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.AutofillSelectBarBehavior
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class AutofillBarsIntegrationTest {
    private val passwordBarLayoutParams = CoordinatorLayout.LayoutParams(0, 0)
    private val passwordBar = spyk(SuggestStrongPasswordBar(testContext)) {
        every { layoutParams } returns passwordBarLayoutParams
    }
    private val settings: Settings = mockk {
        every { toolbarPosition } returns ToolbarPosition.BOTTOM
    }
    private var visibilityInListener = false
    private val onLoginsBarShown = { visibilityInListener = true }
    private val onLoginsBarHidden = { visibilityInListener = false }
    private val integration = AutofillBarsIntegration(passwordBar, settings, onLoginsBarShown, onLoginsBarHidden)

    @Test
    fun `GIVEN a password bar WHEN it is shown THEN inform about this and set a custom layout behavior`() {
        passwordBar.toggleablePromptListener?.onShown()

        assertTrue(integration.isVisible)
        assertTrue(visibilityInListener)
        assertTrue((passwordBar.layoutParams as CoordinatorLayout.LayoutParams).behavior is AutofillSelectBarBehavior)
    }

    @Test
    fun `GIVEN a password bar WHEN it is hidden THEN inform about this and remove any layout behavior`() {
        visibilityInListener = true

        passwordBar.toggleablePromptListener?.onHidden()

        assertFalse(integration.isVisible)
        assertFalse(visibilityInListener)
        assertNull((passwordBar.layoutParams as CoordinatorLayout.LayoutParams).behavior)
    }
}

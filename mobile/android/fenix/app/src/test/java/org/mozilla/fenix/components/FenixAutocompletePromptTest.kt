/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.Gravity
import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.login.LoginSelectBar
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.toolbar.ToolbarPosition

@RunWith(AndroidJUnit4::class)
class FenixAutocompletePromptTest {
    @Test
    fun `GIVEN FenixAutocompletePrompt WHEN selectablePromptListener is set THEN don't initialize the view`() {
        val viewProvider: () -> LoginSelectBar = mockk()
        val fenixPrompt = FenixAutocompletePrompt(
            viewProvider = viewProvider,
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = { },
            onHide = { },
        )

        fenixPrompt.selectablePromptListener = object : SelectablePromptView.Listener<Login> {
            override fun onOptionSelect(option: Login) {}
            override fun onManageOptions() {}
        }

        verify(exactly = 0) { viewProvider.invoke() }
    }

    @Test
    fun `GIVEN FenixAutocompletePrompt WHEN populate is called THEN call populate on the view`() {
        val view: LoginSelectBar = mockk(relaxed = true)
        val fenixPrompt = FenixAutocompletePrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = { },
            onHide = { },
        )

        fenixPrompt.populate(listOf())

        verify { view.populate(listOf()) }
    }

    @Test
    fun `GIVEN FenixAutocompletePrompt WHEN showPrompt is called THEN call showPrompt on the view`() {
        var onShowCalled = false
        val view: LoginSelectBar = mockk(relaxed = true)
        val fenixPrompt = FenixAutocompletePrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = { onShowCalled = true },
            onHide = { },
        )

        val listener = object : SelectablePromptView.Listener<Login> {
            override fun onOptionSelect(option: Login) {}
            override fun onManageOptions() {}
        }

        fenixPrompt.selectablePromptListener = listener
        fenixPrompt.showPrompt()

        verify { view.showPrompt() }
        verify { view.selectablePromptListener = listener }
        assertTrue(onShowCalled)
    }

    @Test
    fun `GIVEN FenixAutocompletePrompt WHEN hidePrompt is called THEN call hidePrompt on the view`() {
        var onHideCalled = false
        val view: LoginSelectBar = mockk(relaxed = true)
        val fenixPrompt = FenixAutocompletePrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = { },
            onHide = { onHideCalled = true },
        )

        fenixPrompt.hidePrompt()

        verify { view.hidePrompt() }
        assertTrue(onHideCalled)
    }

    @Test
    fun `GIVEN the prompt is shown with toolbar at bottom WHEN view is expanded THEN the autocomplete bar is placed at the bottom`() {
        val view = LoginSelectBar(testContext).apply {
            layoutParams = CoordinatorLayout.LayoutParams(0, 0)
        }

        val fenixPrompt = FenixAutocompletePrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = { },
            onHide = { },
        )

        fenixPrompt.showPrompt()
        view.expand()

        // since the behavior is created internally
        // the way to verify that the autocomplete bar is at the bottom is to:
        // verify the layout params shows the gravity at the bottom
        val paramsAfterExpanding = view.layoutParams as CoordinatorLayout.LayoutParams

        assertEquals(View.NO_ID, paramsAfterExpanding.anchorId)
        assertEquals(Gravity.NO_GRAVITY, paramsAfterExpanding.anchorGravity)
        assertEquals(Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL, paramsAfterExpanding.gravity)
    }
}

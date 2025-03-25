/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import io.mockk.mockk
import mozilla.components.feature.prompts.login.SuggestStrongPasswordBar
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.toolbar.ToolbarPosition

class FenixSuggestStrongPasswordPromptTest {

    @Test
    fun `GIVEN FenixSuggestStrongPasswordPrompt when showPrompt is called THEN display the prompt`() {
        val view: SuggestStrongPasswordBar = mockk(relaxed = true)
        val prompt = FenixSuggestStrongPasswordPrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = {},
            onHide = {},
        )

        prompt.showPrompt()

        assertTrue(prompt.isPromptDisplayed)
    }

    @Test
    fun `GIVEN FenixSuggestStrongPasswordPrompt when hidePrompt is called THEN the prompt is not displayed`() {
        val view: SuggestStrongPasswordBar = mockk(relaxed = true)
        val prompt = FenixSuggestStrongPasswordPrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = {},
            onHide = {},
        )
        prompt.showPrompt()

        prompt.hidePrompt()

        assertFalse(prompt.isPromptDisplayed)
    }

    @Test
    fun `GIVEN FenixSuggestStrongPasswordPrompt when showPrompt is called THEN the onShow callback is invoked`() {
        var onShowCalled = false
        val view: SuggestStrongPasswordBar = mockk(relaxed = true)
        val prompt = FenixSuggestStrongPasswordPrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = { onShowCalled = true },
            onHide = {},
        )
        prompt.showPrompt()

        assertTrue(onShowCalled)
    }

    @Test
    fun `GIVEN FenixSuggestStrongPasswordPrompt when hidePrompt is called THEN the onHide callback is invoked`() {
        var onHideCalled = false
        val view: SuggestStrongPasswordBar = mockk(relaxed = true)
        val prompt = FenixSuggestStrongPasswordPrompt(
            viewProvider = { view },
            toolbarPositionProvider = { ToolbarPosition.BOTTOM },
            onShow = {},
            onHide = { onHideCalled = true },
        )
        prompt.hidePrompt()

        assertTrue(onHideCalled)
    }
}

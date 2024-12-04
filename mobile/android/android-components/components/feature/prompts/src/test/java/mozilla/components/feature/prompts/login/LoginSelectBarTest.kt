/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.feature.prompts.concept.ExpandablePrompt
import mozilla.components.feature.prompts.concept.ToggleablePrompt
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class LoginSelectBarTest {
    private val bar = LoginSelectBar(testContext)

    @Test
    fun `WHEN the prompt is shown THEN update state to reflect that and inform listeners about it`() {
        val listener: ToggleablePrompt.Listener = mock()
        bar.toggleablePromptListener = listener

        bar.showPrompt()

        assertTrue(bar.isPromptDisplayed)
        verify(listener).onShown()
    }

    @Test
    fun `WHEN the prompt is hidden THEN update state to reflect that and inform listeners about it`() {
        val listener: ToggleablePrompt.Listener = mock()
        bar.toggleablePromptListener = listener

        bar.hidePrompt()

        assertFalse(bar.isPromptDisplayed)
        verify(listener).onHidden()
    }

    @Test
    fun `WHEN the prompt is expanded THEN inform listeners about it`() {
        val listener: ExpandablePrompt.Listener = mock()
        bar.expandablePromptListener = listener

        bar.expand()

        verify(listener).onExpanded()
    }

    @Test
    fun `WHEN the prompt is collapsed THEN inform listeners about it`() {
        val listener: ExpandablePrompt.Listener = mock()
        bar.expandablePromptListener = listener

        bar.collapse()

        verify(listener).onCollapsed()
    }
}

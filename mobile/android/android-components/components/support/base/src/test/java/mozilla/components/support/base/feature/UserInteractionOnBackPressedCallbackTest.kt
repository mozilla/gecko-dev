/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

import androidx.activity.OnBackPressedDispatcher
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.Mockito.withSettings

class UserInteractionOnBackPressedCallbackTest {

    private lateinit var fragmentManager: FragmentManager
    private lateinit var dispatcher: OnBackPressedDispatcher
    private lateinit var userInteractionCallback: UserInteractionOnBackPressedCallback
    private lateinit var fragment: Fragment
    private lateinit var childFragmentManager: FragmentManager
    private lateinit var interactionFragment: Fragment

    @Before
    fun setup() {
        fragmentManager = mock()
        dispatcher = mock()
        fragment = mock()
        childFragmentManager = mock()

        interactionFragment = mock(Fragment::class.java, withSettings().extraInterfaces(UserInteractionHandler::class.java))

        `when`(fragmentManager.primaryNavigationFragment).thenReturn(fragment)
        `when`(fragment.childFragmentManager).thenReturn(childFragmentManager)

        userInteractionCallback = UserInteractionOnBackPressedCallback(fragmentManager, dispatcher)
    }

    @Test
    fun `GIVEN fragment handles back press WHEN back pressed THEN no back stack pop or system back press`() {
        `when`(childFragmentManager.fragments).thenReturn(listOf(interactionFragment))
        `when`((interactionFragment as UserInteractionHandler).onBackPressed()).thenReturn(true)

        userInteractionCallback.isEnabled = true
        userInteractionCallback.handleOnBackPressed()

        verify(fragmentManager, never()).popBackStack()
        verify(dispatcher, never()).onBackPressed()
        assertTrue(userInteractionCallback.isEnabled)
    }

    @Test
    fun `GIVEN no UserInteractionHandler handled back press and back stack is zero WHEN back pressed THEN system back press is triggered`() {
        `when`(childFragmentManager.fragments).thenReturn(listOf(interactionFragment))
        `when`((interactionFragment as UserInteractionHandler).onBackPressed()).thenReturn(false)
        `when`(childFragmentManager.backStackEntryCount).thenReturn(0)

        userInteractionCallback.isEnabled = true
        userInteractionCallback.handleOnBackPressed()

        verify(dispatcher).onBackPressed()
        assertFalse(userInteractionCallback.isEnabled)
    }

    @Test
    fun `GIVEN no UserInteractionHandler handled back press and back stack is not zero WHEN back pressed THEN back stack is popped`() {
        `when`(childFragmentManager.fragments).thenReturn(listOf(interactionFragment))
        `when`((interactionFragment as UserInteractionHandler).onBackPressed()).thenReturn(false)
        `when`(childFragmentManager.backStackEntryCount).thenReturn(1)

        userInteractionCallback.isEnabled = true
        userInteractionCallback.handleOnBackPressed()

        // The back stack should be popped
        verify(fragmentManager).popBackStack()
        assertTrue(userInteractionCallback.isEnabled)
    }
}

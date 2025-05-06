/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import mozilla.components.support.test.any
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.inOrder
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class SecretDebugMenuTriggerTest {

    private lateinit var onLogoClicked: (Int) -> Unit
    private lateinit var onDebugMenuActivated: () -> Unit
    private lateinit var trigger: SecretDebugMenuTrigger

    @Before
    fun setup() {
        onLogoClicked = mock()
        onDebugMenuActivated = mock()
        trigger = SecretDebugMenuTrigger(onLogoClicked, onDebugMenuActivated)
    }

    @Test
    fun `first click does not do anything`() {
        trigger.onClick() // 1 click

        verify(onLogoClicked, never()).invoke(any())
        verify(onDebugMenuActivated, never()).invoke()
    }

    @Test
    fun `clicking less than 5 times should call onLogoClicked with remaining clicks`() {
        trigger.onClick() // 1 click
        trigger.onClick() // 2 clicks
        trigger.onClick() // 3 clicks
        trigger.onClick() // 4 clicks

        verify(onLogoClicked).invoke(3)
        verify(onLogoClicked).invoke(2)
        verify(onLogoClicked).invoke(1)

        verify(onDebugMenuActivated, never()).invoke()
    }

    @Test
    fun `clicking 5 times should call onDebugMenuActivated`() {
        trigger.onClick() // 1 click
        trigger.onClick() // 2 clicks
        trigger.onClick() // 3 clicks
        trigger.onClick() // 4 clicks
        trigger.onClick() // 5 clicks

        val orderVerifier = inOrder(onLogoClicked, onDebugMenuActivated)

        orderVerifier.verify(onLogoClicked).invoke(3)
        orderVerifier.verify(onLogoClicked).invoke(2)
        orderVerifier.verify(onLogoClicked).invoke(1)
        orderVerifier.verify(onDebugMenuActivated).invoke()
    }

    @Test
    fun `clicking more than 5 times should call onDebugMenuActivated`() {
        trigger.onClick() // 1 click
        trigger.onClick() // 2 clicks
        trigger.onClick() // 3 clicks
        trigger.onClick() // 4 clicks
        trigger.onClick() // 5 clicks
        trigger.onClick() // 6 clicks
        trigger.onClick() // 7 clicks

        val orderVerifier = inOrder(onLogoClicked, onDebugMenuActivated)

        orderVerifier.verify(onLogoClicked).invoke(3)
        orderVerifier.verify(onLogoClicked).invoke(2)
        orderVerifier.verify(onLogoClicked).invoke(1)
        orderVerifier.verify(onDebugMenuActivated, times(1)).invoke()
    }

    @Test
    fun `onResume should reset the counter`() {
        trigger.onClick() // 1 click
        trigger.onClick() // 2 clicks
        trigger.onClick() // 3 clicks

        val orderVerifier = inOrder(onLogoClicked, onDebugMenuActivated)

        orderVerifier.verify(onLogoClicked).invoke(3)
        orderVerifier.verify(onLogoClicked).invoke(2)

        trigger.onResume(mock()) // Reset the counter

        trigger.onClick() // 1 click after reset

        verifyNoMoreInteractions(onLogoClicked)
    }
}

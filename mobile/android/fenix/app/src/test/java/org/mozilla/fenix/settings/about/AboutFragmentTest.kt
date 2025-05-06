/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.content.Context
import android.view.View
import android.widget.Toast
import androidx.lifecycle.LifecycleRegistry
import mozilla.components.support.test.KArgumentCaptor
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.about.AboutFragment.ToastHandler
import org.mozilla.fenix.utils.Settings

class AboutFragmentTest {

    private lateinit var context: Context
    private lateinit var settings: Settings
    private lateinit var logoView: View
    private lateinit var fragment: AboutFragment
    private lateinit var lifecycle: LifecycleRegistry
    private lateinit var secretDebugMenuTrigger: KArgumentCaptor<SecretDebugMenuTrigger>
    private lateinit var toastHandler: ToastHandler

    private val message = "Debug menu: 3 clicks left to enable"
    private val doneMessage = "Debug menu enabled"

    @Before
    fun setup() {
        toastHandler = mock()
        fragment = AboutFragment(toastHandler)
        lifecycle = mock()
        context = mock()
        settings = mock()
        logoView = mock()

        secretDebugMenuTrigger = argumentCaptor()

        whenever(context.getString(R.string.about_debug_menu_toast_progress, 3)).thenReturn(message)
        whenever(context.getString(R.string.about_debug_menu_toast_done)).thenReturn(doneMessage)
        doNothing().`when`(settings).showSecretDebugMenuThisSession = true

        whenever(lifecycle.addObserver(secretDebugMenuTrigger.capture())).then { }
    }

    @Test
    fun `setupDebugMenu sets proper click listener when showSecretDebugMenuThisSession is false`() {
        whenever(settings.showSecretDebugMenuThisSession).thenReturn(false)

        fragment.setupDebugMenu(logoView, settings, lifecycle)

        verify(logoView).setOnClickListener(any())
    }

    @Test
    fun `setupDebugMenu doesn't set click listener when showSecretDebugMenuThisSession is true`() {
        whenever(settings.showSecretDebugMenuThisSession).thenReturn(true)

        fragment.setupDebugMenu(logoView, settings, lifecycle)

        verify(logoView, never()).setOnClickListener(any())
    }

    @Test
    fun `setupDebugMenu adds secretDebugMenuTrigger as lifecycle observer`() {
        whenever(settings.showSecretDebugMenuThisSession).thenReturn(false)

        fragment.setupDebugMenu(logoView, settings, lifecycle)

        verify(lifecycle).addObserver(secretDebugMenuTrigger.value)
    }

    @Test
    fun `onDebugMenuActivated should update settings and show a Toast`() {
        fragment.onDebugMenuActivated(context, settings)

        verify(toastHandler).showToast(context, doneMessage, Toast.LENGTH_LONG)
        verify(settings).showSecretDebugMenuThisSession = true
    }

    @Test
    fun `onLogoClicked should update settings and show a Toast`() {
        fragment.onLogoClicked(context, 3)

        verify(toastHandler).showToast(context, message, Toast.LENGTH_SHORT)
    }
}

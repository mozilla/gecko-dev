/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertFalse
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

@RunWith(AndroidJUnit4::class)
class OpenInFirefoxBindingTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()

    private lateinit var activity: HomeActivity
    private lateinit var customTabsUseCases: CustomTabsUseCases
    private lateinit var openInFenixIntent: Intent
    private lateinit var sessionFeature: ViewBoundFeatureWrapper<SessionFeature>

    @Before
    fun setUp() {
        activity = mock()
        customTabsUseCases = mock()
        openInFenixIntent = mock()
        sessionFeature = mock()
    }

    @Test
    fun `WHEN open in Firefox is requested THEN open in Firefox`() = runTestOnMain {
        val appStore = AppStore()

        val binding = OpenInFirefoxBinding(
            activity = activity,
            appStore = appStore,
            customTabSessionId = "",
            customTabsUseCases = customTabsUseCases,
            openInFenixIntent = openInFenixIntent,
            sessionFeature = sessionFeature,
        )

        val getSessionFeature: SessionFeature = mock()
        whenever(sessionFeature.get()).thenReturn(getSessionFeature)

        val migrateCustomTabsUseCases: CustomTabsUseCases.MigrateCustomTabUseCase = mock()
        whenever(customTabsUseCases.migrate).thenReturn(migrateCustomTabsUseCases)

        binding.start()

        appStore.dispatch(AppAction.OpenInFirefoxStarted)

        // Wait for AppAction.OpenInFirefoxStarted
        appStore.waitUntilIdle()

        verify(getSessionFeature).release()
        verify(migrateCustomTabsUseCases).invoke("", select = true)
        verify(activity).startActivity(openInFenixIntent)
        verify(openInFenixIntent).apply {
            flags = flags or Intent.FLAG_ACTIVITY_NEW_TASK
        }
        verify(activity).finishAndRemoveTask()

        assertFalse(appStore.state.openInFirefoxRequested)
    }
}

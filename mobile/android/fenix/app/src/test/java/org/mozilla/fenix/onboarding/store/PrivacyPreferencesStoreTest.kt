/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class PrivacyPreferencesStoreTest {

    @Test
    fun `WHEN the init action is dispatched THEN the state remains the same`() {
        val state = PrivacyPreferencesState()
        val store = PrivacyPreferencesStore(initialState = state)
        safeDispatch(store, PrivacyPreferencesAction.Init)
        assertEquals(state, store.state)
        assertFalse(state.crashReportingEnabled)
        assertTrue(state.usageDataEnabled)
        assertFalse(state.crashReportingChecked)
        assertTrue(state.usageDataChecked)
    }

    @Test
    fun `WHEN the crash reporting updated action is dispatched THEN the state is updated to match`() {
        val store = PrivacyPreferencesStore(initialState = PrivacyPreferencesState())
        assertFalse(store.state.crashReportingEnabled)

        safeDispatch(store, PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo(true))
        assertTrue(store.state.crashReportingEnabled)

        safeDispatch(store, PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo(false))
        assertFalse(store.state.crashReportingEnabled)
    }

    @Test
    fun `WHEN the usage data updated action is dispatched THEN the state is updated to match`() {
        val store = PrivacyPreferencesStore(initialState = PrivacyPreferencesState())
        assertTrue(store.state.usageDataEnabled)

        safeDispatch(store, PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo(false))
        assertFalse(store.state.usageDataEnabled)

        safeDispatch(store, PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo(true))
        assertTrue(store.state.usageDataEnabled)
    }

    @Test
    fun `WHEN the crash reporting checked action is dispatched THEN the state is updated to match`() {
        val store = PrivacyPreferencesStore(initialState = PrivacyPreferencesState())
        assertFalse(store.state.crashReportingChecked)

        safeDispatch(store, PrivacyPreferencesAction.CrashReportingChecked(true))
        assertTrue(store.state.crashReportingChecked)

        safeDispatch(store, PrivacyPreferencesAction.CrashReportingChecked(false))
        assertFalse(store.state.crashReportingChecked)
    }

    @Test
    fun `WHEN the usage data checked action is dispatched THEN the state is updated to match`() {
        val store = PrivacyPreferencesStore(initialState = PrivacyPreferencesState())
        assertTrue(store.state.usageDataChecked)

        safeDispatch(store, PrivacyPreferencesAction.UsageDataUserChecked(false))
        assertFalse(store.state.usageDataChecked)

        safeDispatch(store, PrivacyPreferencesAction.UsageDataUserChecked(true))
        assertTrue(store.state.usageDataChecked)
    }
}

/**
 * Dispatches the [action] and ensures all [store] processing is completed.
 */
private fun safeDispatch(store: PrivacyPreferencesStore, action: PrivacyPreferencesAction) {
    store.dispatch(action).joinBlocking()
    store.waitUntilIdle()
}

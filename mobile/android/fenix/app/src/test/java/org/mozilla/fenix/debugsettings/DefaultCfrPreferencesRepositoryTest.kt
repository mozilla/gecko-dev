/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import androidx.core.content.edit
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.take
import kotlinx.coroutines.flow.toList
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.debugsettings.cfrs.CfrPreferencesRepository
import org.mozilla.fenix.debugsettings.cfrs.DefaultCfrPreferencesRepository
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class DefaultCfrPreferencesRepositoryTest {
    private val cfrKeys = CfrPreferencesRepository.CfrPreference.entries

    private lateinit var settings: Settings

    @Before
    fun setup() {
        settings = Settings(testContext)
        every { testContext.settings() } returns settings
    }

    @Test
    fun `WHEN the repository is initialized THEN the initial state of the preferences should be emitted`() = runTest {
        val cfrPreferencesRepository = DefaultCfrPreferencesRepository(
            context = testContext,
            lifecycleOwner = mockk(relaxed = true),
            coroutineScope = this,
        )

        settings.preferences.edit {
            cfrKeys.forEach {
                putBoolean(testContext.getString(it.preferenceKey), true)
            }
        }

        cfrPreferencesRepository.init()

        val actual = cfrPreferencesRepository.cfrPreferenceUpdates.take(cfrKeys.size).toList()

        assertTrue(actual.isNotEmpty())
        assertTrue(
            actual.all {
                it.value
            },
        )
    }

    @Test
    fun `WHEN a change is made to the preference values THEN the repository emits the change`() = runTest {
        val cfrPreferencesRepository = DefaultCfrPreferencesRepository(
            context = testContext,
            lifecycleOwner = mockk(relaxed = true),
            coroutineScope = this,
        )

        CfrPreferencesRepository.CfrPreference.entries.forEach {
            val preferenceKey = testContext.getString(it.preferenceKey)

            settings.preferences.edit { putBoolean(preferenceKey, true) }
            cfrPreferencesRepository.onPreferenceChange(
                sharedPreferences = settings.preferences,
                key = preferenceKey,
            )

            assertTrue(cfrPreferencesRepository.cfrPreferenceUpdates.first().value)
        }
    }

    @Test
    fun `WHEN the repository resets the last CFR shown time THEN timestamp preference is set to 0`() = runTest {
        val cfrPreferencesRepository = DefaultCfrPreferencesRepository(
            context = testContext,
            lifecycleOwner = mockk(relaxed = true),
            coroutineScope = this,
        )

        val resetValue = 0L
        settings.lastCfrShownTimeInMillis = 100

        assertNotEquals(resetValue, settings.lastCfrShownTimeInMillis)
        cfrPreferencesRepository.resetLastCfrTimestamp()
        assertEquals(resetValue, settings.lastCfrShownTimeInMillis)
    }
}

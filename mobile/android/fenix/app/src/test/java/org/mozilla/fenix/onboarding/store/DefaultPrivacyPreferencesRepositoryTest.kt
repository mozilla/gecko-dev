/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

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
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class DefaultPrivacyPreferencesRepositoryTest {
    private val privacyPreferenceKeys = PrivacyPreferencesRepository.PrivacyPreference.entries

    private lateinit var settings: Settings

    @Before
    fun setup() {
        settings = Settings(testContext)
        every { testContext.settings() } returns settings
    }

    @Test
    fun `test the privacy preference enum keys map to the correct preference key`() {
        assertEquals(
            R.string.pref_key_crash_reporting_always_report,
            privacyPreferenceKeys[0].preferenceKey,
        )
        assertEquals(R.string.pref_key_telemetry, privacyPreferenceKeys[1].preferenceKey)
    }

    @Test
    fun `WHEN the repository is initialized THEN the initial state of the preferences should be emitted`() =
        runTest {
            val repository = DefaultPrivacyPreferencesRepository(
                context = testContext,
                lifecycleOwner = mockk(relaxed = true),
                coroutineScope = this,
            )

            settings.preferences.edit {
                privacyPreferenceKeys.forEach {
                    putBoolean(testContext.getString(it.preferenceKey), false)
                }
            }

            repository.init()

            val actual =
                repository.privacyPreferenceUpdates.take(privacyPreferenceKeys.size).toList()

            assertTrue(actual.isNotEmpty())
            assertFalse(actual.all { it.value })
        }

    @Test
    fun `WHEN a change is made to the preference values THEN the repository emits the change`() =
        runTest {
            val repository = DefaultPrivacyPreferencesRepository(
                context = testContext,
                lifecycleOwner = mockk(relaxed = true),
                coroutineScope = this,
            )

            PrivacyPreferencesRepository.PrivacyPreference.entries.forEach {
                val preferenceKey = testContext.getString(it.preferenceKey)

                settings.preferences.edit { putBoolean(preferenceKey, false) }
                repository.onPreferenceChange(
                    sharedPreferences = settings.preferences,
                    key = preferenceKey,
                )

                assertFalse(repository.privacyPreferenceUpdates.first().value)
            }
        }

    @Test
    fun `GIVEN crash reporting preference update WHEN updatePrivacyPreference is called THEN the real preference is updated`() =
        runTest {
            val repository = DefaultPrivacyPreferencesRepository(
                context = testContext,
                lifecycleOwner = mockk(relaxed = true),
                coroutineScope = this,
            )

            settings.preferences.edit {
                privacyPreferenceKeys.forEach {
                    putBoolean(testContext.getString(it.preferenceKey), false)
                }
            }

            assertFalse(settings.crashReportAlwaysSend)

            val preferenceUpdate = PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                preferenceType = PrivacyPreferencesRepository.PrivacyPreference.CrashReporting,
                value = true,
            )
            repository.updatePrivacyPreference(preferenceUpdate)

            assertTrue(settings.crashReportAlwaysSend)
        }

    @Test
    fun `GIVEN usage data preference update WHEN updatePrivacyPreference is called THEN the real preference is updated`() =
        runTest {
            val repository = DefaultPrivacyPreferencesRepository(
                context = testContext,
                lifecycleOwner = mockk(relaxed = true),
                coroutineScope = this,
            )

            settings.preferences.edit {
                privacyPreferenceKeys.forEach {
                    putBoolean(testContext.getString(it.preferenceKey), false)
                }
            }

            assertFalse(settings.isTelemetryEnabled)

            val preferenceUpdate = PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                preferenceType = PrivacyPreferencesRepository.PrivacyPreference.UsageData,
                value = true,
            )
            repository.updatePrivacyPreference(preferenceUpdate)

            assertTrue(settings.isTelemetryEnabled)
        }
}

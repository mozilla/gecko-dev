/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.content.Context
import android.content.SharedPreferences
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class SharedPreferencesBooleanTest {
    private val key = "key"
    private val defaultValue = false
    private lateinit var preferencesHolder: BooleanTestPreferenceHolder
    private lateinit var testPreferences: SharedPreferences

    @Before
    fun setup() {
        testPreferences = testContext.getSharedPreferences("test", Context.MODE_PRIVATE)
    }

    @After
    fun tearDown() {
        testPreferences.edit().clear().apply()
    }

    @Test
    fun `GIVEN boolean does not exist and asked to persist the default WHEN asked for it THEN persist the default and return it`() {
        preferencesHolder = BooleanTestPreferenceHolder(
            persistDefaultIfNotExists = true,
        )

        val result = preferencesHolder.boolean

        assertEquals(defaultValue, result)
        assertEquals(defaultValue, testPreferences.getBoolean(key, !defaultValue))
    }

    @Test
    fun `GIVEN boolean does not exist and not asked to persist the default WHEN asked for it THEN return the default but not persist it`() {
        preferencesHolder = BooleanTestPreferenceHolder(
            persistDefaultIfNotExists = false,
        )

        val result = preferencesHolder.boolean

        assertEquals(defaultValue, result)
        assertEquals(!defaultValue, testPreferences.getBoolean(key, !defaultValue))
    }

    @Test
    fun `GIVEN boolean exists and asked to persist the default WHEN asked for it THEN return the existing boolean and don't persist the default`() {
        testPreferences.edit().putBoolean(key, !defaultValue).apply()
        preferencesHolder = BooleanTestPreferenceHolder(
            persistDefaultIfNotExists = true,
        )

        val result = preferencesHolder.boolean

        assertEquals(!defaultValue, result)
    }

    @Test
    fun `GIVEN boolean exists and not asked to persist the default WHEN asked for it THEN return the existing boolean and don't persist the default`() {
        testPreferences.edit().putBoolean(key, !defaultValue).apply()
        preferencesHolder = BooleanTestPreferenceHolder(
            persistDefaultIfNotExists = false,
        )

        val result = preferencesHolder.boolean

        assertEquals(!defaultValue, result)
    }

    @Test
    fun `GIVEN a value exists WHEN asked to persist a new value THEN update the persisted value`() {
        testPreferences.edit().putBoolean(key, !defaultValue).apply()
        preferencesHolder = BooleanTestPreferenceHolder()

        preferencesHolder.boolean = defaultValue

        assertEquals(defaultValue, testPreferences.getBoolean(key, !defaultValue))
    }

    @Test
    fun `GIVEN a value does not exist WHEN asked to persist a new value THEN persist the requested value`() {
        preferencesHolder = BooleanTestPreferenceHolder()

        preferencesHolder.boolean = !defaultValue

        assertTrue(testPreferences.getBoolean(key, defaultValue))
    }

    private inner class BooleanTestPreferenceHolder(
        persistDefaultIfNotExists: Boolean = false,
    ) : PreferencesHolder {
        override val preferences = testPreferences

        var boolean by booleanPreference(key, defaultValue, persistDefaultIfNotExists)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.settings

import android.content.Context
import android.content.SharedPreferences
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.ktx.android.content.PreferencesHolder
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy

@RunWith(AndroidJUnit4::class)
class FeatureFlagPreferenceTest {
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
    fun `WHEN feature flag if true THEN feature acts like boolean preference`() {
        testPreferences.edit().putBoolean("key", true).apply()
        val holder = spy(FeatureFlagHolder(featureFlag = true))

        assertTrue(holder.property)

        holder.property = false
        assertFalse(testPreferences.getBoolean("key", true))
    }

    @Test
    fun `WHEN feature flag if false THEN feature flag always return false`() {
        testPreferences.edit().putBoolean("key", true).apply()
        val holder = FeatureFlagHolder(featureFlag = false)

        assertFalse(holder.property)
        holder.property = true
        assertFalse(holder.property)
    }

    private inner class FeatureFlagHolder(featureFlag: Boolean) : PreferencesHolder {
        override val preferences = testPreferences

        var property by featureFlagPreference(
            "key",
            default = false,
            featureFlag = featureFlag,
        )
    }
}

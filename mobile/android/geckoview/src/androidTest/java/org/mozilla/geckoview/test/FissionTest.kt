/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertNull
import junit.framework.TestCase.assertTrue
import org.hamcrest.CoreMatchers.equalTo
import org.junit.Assume.assumeThat
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntimeSettings

@RunWith(AndroidJUnit4::class)
@MediumTest
class FissionTest : BaseSessionTest() {
    @Test
    fun fissionEnabledByEnvironment() {
        assumeThat(sessionRule.env.isFission, equalTo(true))

        // Check preference with Gecko
        val fissionAutostart = sessionRule.getPrefs(
            "fission.autostart",
        )
        assertTrue(
            fissionAutostart[0] as Boolean,
        )

        // Check preference with GeckoView
        assertNull(
            "Default will have no value since we are relying on Gecko.",
            sessionRule.runtime.settings.fissionEnabled,
        )

        // Verify fission is on
        assertTrue(
            "Fission is running.",
            sessionRule.isFissionRunning,
        )
    }

    @Test
    fun fissionDisabledByEnvironment() {
        assumeThat(sessionRule.env.isFission, equalTo(false))

        // Check preference with Gecko
        val fissionAutostart = sessionRule.getPrefs(
            "fission.autostart",
        )
        assertFalse(
            fissionAutostart[0] as Boolean,
        )

        // Check preference with GeckoView
        assertNull(
            "Default will have no value since we are relying on Gecko.",
            sessionRule.runtime.settings.fissionEnabled,
        )

        // Verify fission is off
        assertFalse(
            "Fission is not running.",
            sessionRule.isFissionRunning,
        )
    }

    @Test
    fun testFissionSettingsBuilder() {
        val settingFissionEnabled = GeckoRuntimeSettings.Builder()
            .fissionEnabled(true)
            .build()
        assertTrue(
            "Fission setting should be set to true.",
            settingFissionEnabled.fissionEnabled!!,
        )

        val settingFissionDisabled = GeckoRuntimeSettings.Builder()
            .fissionEnabled(false)
            .build()
        assertFalse(
            "Fission setting should be set to false.",
            settingFissionDisabled.fissionEnabled!!,
        )
    }
}

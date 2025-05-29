/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.hamcrest.Matchers.equalTo
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.test.util.RuntimeCreator

@RunWith(AndroidJUnit4::class)
@MediumTest
class UserCharUUIDResetTest : BaseSessionTest() {
    @Test
    fun testUUIDIsReset() {
        val prefName = "toolkit.telemetry.user_characteristics_ping.uuid"
        val prefVal = "Hello World!"

        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                prefName to prefVal,
            ),
        )

        assertThat("Pref is set", sessionRule.getPrefs(prefName)[0], equalTo(prefVal))

        RuntimeCreator.getRuntime().notifyTelemetryPrefChanged(true)
        assertThat("Pref is still the same when telemetry is enabled", sessionRule.getPrefs(prefName)[0], equalTo(prefVal))

        RuntimeCreator.getRuntime().notifyTelemetryPrefChanged(false)
        assertThat("Pref is reset when telemetry is disabled", sessionRule.getPrefs(prefName)[0], equalTo(""))
    }
}

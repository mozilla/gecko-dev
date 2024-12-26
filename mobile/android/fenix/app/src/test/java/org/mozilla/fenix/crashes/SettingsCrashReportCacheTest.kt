/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import mozilla.components.lib.crash.store.TimeInMillis
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mozilla.fenix.utils.Settings

class SettingsCrashReportCacheTest {

    @get:Rule val coroutineRule = MainCoroutineRule()

    private lateinit var settings: Settings

    @Before
    fun setup() {
        settings = mock()
    }

    @Test
    fun `GIVEN cache has 0 stored for crashReportCutoffDate WHEN accessed THEN returns null`() = runTestOnMain {
        `when`(settings.crashReportCutoffDate).thenReturn(0)

        val cache = SettingsCrashReportCache(settings)
        val result: TimeInMillis? = cache.getCutoffDate()

        assertEquals(null, result)
    }

    @Test
    fun `WHEN setting CutoffDate with null value THEN 0 is stored`() = runTestOnMain {
        val cache = SettingsCrashReportCache(settings)
        cache.setCutoffDate(null)

        verify(settings).crashReportCutoffDate = 0
    }

    @Test
    fun `GIVEN cache has 0 stored for DeferredUntil WHEN accessed THEN returns null`() = runTestOnMain {
        `when`(settings.crashReportDeferredUntil).thenReturn(0)

        val cache = SettingsCrashReportCache(settings)
        val result: TimeInMillis? = cache.getDeferredUntil()

        assertEquals(null, result)
    }

    @Test
    fun `WHEN setting DeferredUntil with null value THEN 0 is stored`() = runTestOnMain {
        val cache = SettingsCrashReportCache(settings)
        cache.setDeferredUntil(null)

        verify(settings).crashReportDeferredUntil = 0
    }
}

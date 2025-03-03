/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
internal class AdjustMetricsServiceTest {
    val context: Context = ApplicationProvider.getApplicationContext()

    @Test
    fun `WHEN Adjust attribution data already exist THEN already known is true`() {
        val settings = Settings(context)
        assertFalse(AdjustMetricsService.alreadyKnown(settings))

        settings.adjustCampaignId = "campaign"
        assertTrue(AdjustMetricsService.alreadyKnown(settings))

        settings.adjustCampaignId = ""
        assertFalse(AdjustMetricsService.alreadyKnown(settings))

        settings.adjustNetwork = "network"
        assertTrue(AdjustMetricsService.alreadyKnown(settings))

        settings.adjustNetwork = ""
        assertFalse(AdjustMetricsService.alreadyKnown(settings))

        settings.adjustAdGroup = "ad group"
        assertTrue(AdjustMetricsService.alreadyKnown(settings))

        settings.adjustAdGroup = ""
        assertFalse(AdjustMetricsService.alreadyKnown(settings))

        settings.adjustCreative = "creative"
        assertTrue(AdjustMetricsService.alreadyKnown(settings))
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
internal class MarketingAttributionServiceTest {
    @Test
    fun `WHEN installReferrerResponse is empty or null THEN we should not show marketing onboarding`() {
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding(null))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding(""))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding(" "))
    }

    @Test
    fun `WHEN installReferrerResponse is in the marketing prefixes THEN we should show marketing onboarding`() {
        assertTrue(MarketingAttributionService.shouldShowMarketingOnboarding("gclid="))
        assertTrue(MarketingAttributionService.shouldShowMarketingOnboarding("gclid=12345"))
        assertTrue(MarketingAttributionService.shouldShowMarketingOnboarding("adjust_reftag="))
        assertTrue(MarketingAttributionService.shouldShowMarketingOnboarding("adjust_reftag=test"))
    }

    @Test
    fun `WHEN installReferrerResponse is not in the marketing prefixes THEN we should show marketing onboarding`() {
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding(" gclid=12345"))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding("utm_source=google-play&utm_medium=organic"))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding("utm_source=(not%20set)&utm_medium=(not%20set)"))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding("utm_source=eea-browser-choice&utm_medium=preload"))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding("gclida="))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding("adjust_reftag_test"))
        assertFalse(MarketingAttributionService.shouldShowMarketingOnboarding("test"))
    }
}

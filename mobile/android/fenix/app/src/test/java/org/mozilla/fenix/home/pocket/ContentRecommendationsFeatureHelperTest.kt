/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class ContentRecommendationsFeatureHelperTest {

    @Test
    fun `GIVEN unsupported locale WHEN pocket recommendations feature enabled check is invoked THEN return false`() {
        val locale = Locale.Builder().setLanguage("ro").setRegion("RO").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertFalse(ContentRecommendationsFeatureHelper.isPocketRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en-US locale WHEN pocket recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").setRegion("US").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isPocketRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en-CA locale WHEN pocket recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").setRegion("CA").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isPocketRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN unsupported locale WHEN pocket sponsored stories feature enabled check is invoked THEN return false`() {
        val locale = Locale.Builder().setLanguage("ro").setRegion("RO").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertFalse(ContentRecommendationsFeatureHelper.isPocketSponsoredStoriesFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en-US locale WHEN pocket sponsored stories feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").setRegion("US").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isPocketSponsoredStoriesFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en-CA locale WHEN pocket sponsored stories feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").setRegion("CA").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isPocketSponsoredStoriesFeatureEnabled(testContext))
    }
}

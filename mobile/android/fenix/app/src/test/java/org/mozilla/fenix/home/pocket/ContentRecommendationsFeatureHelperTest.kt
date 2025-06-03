/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.Locale

@RunWith(AndroidJUnit4::class)
class ContentRecommendationsFeatureHelperTest {

    @Before
    @After
    fun cleanUp() {
        LocaleManager.resetToSystemDefault(context = testContext, localeUseCase = null)
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

    @Test
    fun `GIVEN unsupported locale WHEN content recommendations feature enabled check is invoked THEN return false`() {
        val locale = Locale.Builder().setLanguage("ro").setRegion("RO").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertFalse(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported fr locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("fr").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported fr-FR locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("fr").setRegion("FR").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported es locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("es").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported es-ES locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("es").setRegion("ES").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported it locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("it").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported it-IT locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("it").setRegion("IT").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en-CA locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").setRegion("CA").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en-GB locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").setRegion("GB").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported en-US locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("en").setRegion("US").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported de locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("de").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported de-DE locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("de").setRegion("DE").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported de-AT locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("de").setRegion("AT").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }

    @Test
    fun `GIVEN supported de-CH locale WHEN content recommendations feature enabled check is invoked THEN return true`() {
        val locale = Locale.Builder().setLanguage("de").setRegion("CH").build()
        LocaleManager.setNewLocale(testContext, localeUseCase = null, locale = locale)

        assertTrue(ContentRecommendationsFeatureHelper.isContentRecommendationsFeatureEnabled(testContext))
    }
}

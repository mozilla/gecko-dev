/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import android.app.Activity
import android.content.Context
import android.content.res.Configuration
import android.content.res.Resources
import kotlinx.coroutines.test.runTest
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.locale.LocaleUseCases
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mock
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations.openMocks
import org.mozilla.focus.settings.InstalledSearchEnginesSettingsFragment
import java.util.Locale

class LanguageMiddlewareTest {

    @Mock
    private lateinit var mockActivity: Activity

    @Mock
    private lateinit var resources: Resources

    @Mock
    private lateinit var configuration: Configuration

    @Mock
    private lateinit var mockLocaleUseCases: LocaleUseCases

    @Mock
    private lateinit var mockMiddlewareContext: MiddlewareContext<LanguageScreenState, LanguageScreenAction>

    @Mock
    private lateinit var mockNext: (LanguageScreenAction) -> Unit

    @Mock
    private lateinit var mockStorage: LanguageStorage

    private lateinit var middleware: LanguageMiddleware

    private val context: Context = mock()
    private val mockLocale = Locale.forLanguageTag("en-US")

    @Before
    fun setup() {
        openMocks(this)
        middleware = spy(
            LanguageMiddleware(mockActivity, mockLocaleUseCases, mockStorage) { mockLocale },
        )

        `when`(mockActivity.applicationContext).thenReturn(context)
        `when`(context.resources).thenReturn(resources)
        `when`(resources.configuration).thenReturn(configuration)
        @Suppress("DEPRECATION")
        doNothing().`when`(resources).updateConfiguration(any(), any())

        InstalledSearchEnginesSettingsFragment.languageChanged = false
        doNothing().`when`(middleware).setNewLocale(any())
        doNothing().`when`(middleware).resetToSystemDefault()
        doNothing().`when`(middleware).recreateActivity()
    }

    @Test
    fun `GIVEN Select action WHEN invoke THEN saves language, sets current language and calls next`() {
        val selectedLanguage = Language("es-ES", "Español (España)", 0)
        val action = LanguageScreenAction.Select(selectedLanguage)

        middleware.invoke(mockMiddlewareContext, mockNext, action)

        verify(mockStorage).saveCurrentLanguageInSharePref(selectedLanguage.tag)
        @Suppress("DEPRECATION")
        verify(resources).updateConfiguration(any(), any())
        verify(mockNext).invoke(action)
        assertTrue(InstalledSearchEnginesSettingsFragment.languageChanged)
    }

    @Test
    fun `GIVEN Select action with system default WHEN invoke THEN resets to system default`() {
        val selectedLanguage = Language("System Default", LanguageStorage.LOCALE_SYSTEM_DEFAULT, 0)
        val action = LanguageScreenAction.Select(selectedLanguage)

        middleware.invoke(mockMiddlewareContext, mockNext, action)

        verify(mockStorage).saveCurrentLanguageInSharePref(LanguageStorage.LOCALE_SYSTEM_DEFAULT)
        @Suppress("DEPRECATION")
        verify(resources).updateConfiguration(any(), any())
        verify(mockNext).invoke(action)
        assertTrue(InstalledSearchEnginesSettingsFragment.languageChanged)
    }

    @Test
    fun `GIVEN InitLanguages action WHEN invoke THEN dispatches UpdateLanguages`() {
        val languages =
            listOf(Language("en-US", "English (US)", 1), Language("es-ES", "Español (España)", 0))
        val selectedLanguage = Language("en-US", "English (US)", 1)
        val action = LanguageScreenAction.InitLanguages
        `when`(mockStorage.languages).thenReturn(languages)
        `when`(mockStorage.selectedLanguage).thenReturn(selectedLanguage)

        middleware.invoke(mockMiddlewareContext, mockNext, action)

        val dispatchedActionCaptor = argumentCaptor<LanguageScreenAction>()
        verify(mockMiddlewareContext).dispatch(dispatchedActionCaptor.capture())
        val dispatchedAction = dispatchedActionCaptor.value
        assertTrue(dispatchedAction is LanguageScreenAction.UpdateLanguages)
        dispatchedAction as LanguageScreenAction.UpdateLanguages
        assertEquals(languages, dispatchedAction.languageList)
        assertEquals(selectedLanguage, dispatchedAction.selectedLanguage)
    }

    @Test
    fun `GIVEN other action WHEN invoke THEN calls next`() {
        val action =
            LanguageScreenAction.UpdateLanguages(emptyList(), Language("en-US", "English (US)", 0))

        middleware.invoke(mockMiddlewareContext, mockNext, action)

        verify(mockNext).invoke(action)
    }

    @Test
    fun `GIVEN non-system default language tag WHEN setCurrentLanguage THEN sets new locale and recreates activity`() =
        runTest {
            val languageTag = "es-ES"
            val locale = Locale.forLanguageTag(languageTag)

            middleware.setCurrentLanguage(languageTag)

            verify(middleware).setNewLocale(locale)
            @Suppress("DEPRECATION")
            verify(resources).updateConfiguration(any(), any())
            verify(middleware).recreateActivity()
        }

    @Test
    fun `GIVEN system default language tag WHEN setCurrentLanguage THEN resets to system default and recreates activity`() =
        runTest {
            val languageTag = LanguageStorage.LOCALE_SYSTEM_DEFAULT

            middleware.setCurrentLanguage(languageTag)

            verify(middleware).resetToSystemDefault()
            @Suppress("DEPRECATION")
            verify(resources).updateConfiguration(any(), any())
            verify(middleware).recreateActivity()
        }
}

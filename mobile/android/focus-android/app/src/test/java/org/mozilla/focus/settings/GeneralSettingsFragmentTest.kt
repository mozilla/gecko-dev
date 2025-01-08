package org.mozilla.focus.settings

import android.content.Context
import android.content.SharedPreferences
import android.content.res.Resources
import androidx.preference.PreferenceManager
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doCallRealMethod
import org.mockito.Mockito.doReturn
import org.mozilla.focus.R
import org.mozilla.focus.locale.screen.LanguageStorage.Companion.LOCALE_SYSTEM_DEFAULT
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GeneralSettingsFragmentTest {

    private lateinit var fragment: GeneralSettingsFragment
    private lateinit var context: Context
    private lateinit var sharedPreferences: SharedPreferences
    private lateinit var resources: Resources

    @Before
    fun setUp() {
        fragment = mock()
        context = mock()
        sharedPreferences = mock()
        resources = mock()

        doCallRealMethod().`when`(fragment).getLocaleSummary()

        whenever(fragment.getString(R.string.preference_language_systemdefault)).thenReturn("System Default")
        whenever(fragment.requireContext()).thenReturn(context)
        whenever(fragment.resources).thenReturn(resources)
        whenever(PreferenceManager.getDefaultSharedPreferences(context)).thenReturn(sharedPreferences)
        whenever(resources.getString(R.string.pref_key_locale)).thenReturn("pref_key_locale")
    }

    @Test
    fun `getLocaleSummary returns system default when value is null or empty`() {
        setupSharedPreferencesValue(null)
        assertEquals("System Default", fragment.getLocaleSummary())

        setupSharedPreferencesValue("")
        assertEquals("System Default", fragment.getLocaleSummary())
    }

    @Test
    fun `getLocaleSummary returns system default when value is LOCALE_SYSTEM_DEFAULT`() {
        setupSharedPreferencesValue(LOCALE_SYSTEM_DEFAULT)

        assertEquals("System Default", fragment.getLocaleSummary())
    }

    @Test
    fun `getLocaleSummary returns native name when value is not null`() {
        doReturn("English").`when`(fragment).getLocaleDescriptorNativeName("en")

        setupSharedPreferencesValue("en")
        assertEquals("English", fragment.getLocaleSummary())
    }

    @Test
    fun `getLocaleSummary returns system default when native name is null`() {
        doReturn(null).`when`(fragment).getLocaleDescriptorNativeName("en")

        setupSharedPreferencesValue("en")
        assertEquals("System Default", fragment.getLocaleSummary())
    }

    private fun setupSharedPreferencesValue(value: String?) {
        whenever(sharedPreferences.getString("pref_key_locale", LOCALE_SYSTEM_DEFAULT)).thenReturn(value)
    }
}

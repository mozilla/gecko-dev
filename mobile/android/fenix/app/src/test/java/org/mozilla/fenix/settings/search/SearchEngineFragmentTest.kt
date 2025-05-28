/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.content.SharedPreferences
import androidx.preference.Preference
import androidx.preference.SwitchPreference
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import junit.framework.TestCase.assertEquals
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SearchEngineFragmentTest {

    private lateinit var settings: Settings
    private lateinit var preferences: SharedPreferences
    private lateinit var preferencesEditor: SharedPreferences.Editor
    private lateinit var fragment: SearchEngineFragment

    @Before
    fun setUp() {
        settings = mockk<Settings>(relaxed = true)
        every { settings.preferences }
        every { testContext.components.settings } returns settings
        preferences = mockk()
        preferencesEditor = mockk(relaxed = true)
        every { settings.preferences } returns preferences
        every { preferences.edit() } returns preferencesEditor

        fragment = spyk(SearchEngineFragment()) {
            every { context } returns testContext
            every { isAdded } returns true
            every { activity } returns mockk<HomeActivity>(relaxed = true)
        }

        every { fragment.updateAllWidgets(testContext) } just Runs
    }

    @Test
    fun `GIVEN pref_key_show_voice_search preference WHEN it is modified THEN the value is persisted and widgets updated`() {
        val voiceSearchPreferenceKey = testContext.getString(R.string.pref_key_show_voice_search)
        val voiceSearchPreference = spyk(SwitchPreference(testContext)) {
            every { key } returns voiceSearchPreferenceKey
        }

        // Trigger the voice preference setup.
        fragment.initialiseVoiceSearchPreference(voiceSearchPreference)
        voiceSearchPreference.callChangeListener(true)

        verify { preferencesEditor.putBoolean(voiceSearchPreferenceKey, true) }
        verify { fragment.updateAllWidgets(testContext) }
    }

    @Test
    fun `GIVEN pref_key_default_search_engine preference it has selected engine as summary WHEN clicked navigates to default engine settings`() {
        val searchEngineName = "MySearchEngine"

        val searchEngine = mockk<SearchEngine>(relaxed = true) {
            every { name } returns searchEngineName
        }

        every { fragment.getSelectedSearchEngine(testContext) } returns searchEngine
        every { fragment.openDefaultEngineSettings() } just Runs

        val defaultSearchEngineKey = testContext.getString(R.string.pref_key_default_search_engine)
        val defaultSearchEnginePreference = spyk(Preference(testContext)) {
            every { key } returns defaultSearchEngineKey
        }

        every { fragment.findPreference<Preference>(defaultSearchEngineKey) } returns defaultSearchEnginePreference

        // Trigger the default search engine preference setup.
        fragment.updateDefaultSearchEnginePreference()

        verify { defaultSearchEnginePreference.summary = searchEngineName }
        assertEquals(searchEngineName, defaultSearchEnginePreference.summary.toString())

        fragment.onPreferenceTreeClick(defaultSearchEnginePreference)
        verify { fragment.openDefaultEngineSettings() }
    }

    @Test
    fun `GIVEN pref_key_manage_search_shortcuts preference WHEN clicked THEN navigates to SearchShortcutsFragment`() {
        every { fragment.openSearchShortcutsSettings() } just Runs

        val manageShortcutsKey = testContext.getString(R.string.pref_key_manage_search_shortcuts)
        val manageShortcutsPreference = spyk(Preference(testContext)) {
            every { key } returns manageShortcutsKey
        }

        every { fragment.findPreference<Preference>(manageShortcutsKey) } returns manageShortcutsPreference

        fragment.onPreferenceTreeClick(manageShortcutsPreference)
        verify { fragment.openSearchShortcutsSettings() }
    }

    @Test
    fun `GIVEN pref_key_learn_about_fx_suggest preference WHEN clicked THEN navigates to Fx Suggest SUMO page`() {
        every { fragment.openLearnMoreLink() } just Runs

        val learnAboutFxSuggestKey = testContext.getString(R.string.pref_key_learn_about_fx_suggest)
        val learnAboutFxSuggestPreference = spyk(Preference(testContext)) {
            every { key } returns learnAboutFxSuggestKey
        }

        every { fragment.findPreference<Preference>(learnAboutFxSuggestKey) } returns learnAboutFxSuggestPreference

        fragment.onPreferenceTreeClick(learnAboutFxSuggestPreference)
        verify { fragment.openLearnMoreLink() }
    }
}

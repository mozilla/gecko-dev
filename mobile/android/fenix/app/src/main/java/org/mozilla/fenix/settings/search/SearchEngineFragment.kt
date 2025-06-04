/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.appwidget.AppWidgetManager
import android.content.Context
import android.os.Bundle
import androidx.annotation.VisibleForTesting
import androidx.core.content.edit
import androidx.navigation.fragment.findNavController
import androidx.preference.CheckBoxPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.navigateWithBreadcrumb
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SharedPreferenceUpdater
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.requirePreference
import org.mozilla.gecko.search.SearchWidgetProvider

class SearchEngineFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(
            R.xml.search_settings_preferences,
            rootKey,
        )

        requirePreference<SwitchPreference>(R.string.pref_key_show_sponsored_suggestions).apply {
            isVisible = context.settings().enableFxSuggest
        }
        requirePreference<SwitchPreference>(R.string.pref_key_show_nonsponsored_suggestions).apply {
            isVisible = context.settings().enableFxSuggest
        }
        requirePreference<Preference>(R.string.pref_key_learn_about_fx_suggest).apply {
            isVisible = context.settings().enableFxSuggest
        }
        requirePreference<CheckBoxPreference>(R.string.pref_key_show_trending_search_suggestions).apply {
            isVisible = context.settings().isTrendingSearchesVisible
        }
        requirePreference<SwitchPreference>(R.string.pref_key_show_recent_search_suggestions).apply {
            isVisible = context.settings().isRecentSearchesVisible
        }
        requirePreference<SwitchPreference>(R.string.pref_key_show_shortcuts_suggestions).apply {
            isVisible = context.settings().isShortcutSuggestionsVisible
        }

        view?.hideKeyboard()
    }

    @Suppress("LongMethod")
    override fun onResume() {
        super.onResume()
        view?.hideKeyboard()
        showToolbar(getString(R.string.preferences_search))

        val showVoiceSearchPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_show_voice_search).apply {
                isChecked = context.settings().shouldShowVoiceSearch
            }

        initialiseVoiceSearchPreference(showVoiceSearchPreference)
        updateDefaultSearchEnginePreference()

        val searchSuggestionsPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_show_search_suggestions).apply {
                isChecked = context.settings().shouldShowSearchSuggestions
            }

        val trendingSearchSuggestionsPreference =
            requirePreference<CheckBoxPreference>(R.string.pref_key_show_trending_search_suggestions).apply {
                isVisible = context.settings().isTrendingSearchesVisible
                isEnabled = getSelectedSearchEngine(requireContext())?.trendingUrl != null &&
                    context.settings().shouldShowSearchSuggestions
            }

        val recentSearchSuggestionsPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_show_recent_search_suggestions).apply {
                isChecked = context.settings().shouldShowRecentSearchSuggestions
            }

        val autocompleteURLsPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_enable_autocomplete_urls).apply {
                isChecked = context.settings().shouldAutocompleteInAwesomebar
            }

        val searchSuggestionsInPrivatePreference =
            requirePreference<CheckBoxPreference>(R.string.pref_key_show_search_suggestions_in_private).apply {
                isChecked = context.settings().shouldShowSearchSuggestionsInPrivate
                isEnabled = context.settings().shouldShowSearchSuggestions
            }

        val showHistorySuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_search_browsing_history).apply {
                isChecked = context.settings().shouldShowHistorySuggestions
            }

        val showBookmarkSuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_search_bookmarks).apply {
                isChecked = context.settings().shouldShowBookmarkSuggestions
            }

        val showShortcutsSuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_show_shortcuts_suggestions).apply {
                isChecked = context.settings().shouldShowShortcutSuggestions
            }

        val showSyncedTabsSuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_search_synced_tabs).apply {
                isChecked = context.settings().shouldShowSyncedTabsSuggestions
            }

        val showClipboardSuggestions =
            requirePreference<SwitchPreference>(R.string.pref_key_show_clipboard_suggestions).apply {
                isChecked = context.settings().shouldShowClipboardSuggestions
            }

        val showSponsoredSuggestionsPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_show_sponsored_suggestions).apply {
                isChecked = context.settings().showSponsoredSuggestions
                summary = getString(
                    R.string.preferences_show_sponsored_suggestions_summary,
                    getString(R.string.app_name),
                )
            }

        val showNonSponsoredSuggestionsPreference =
            requirePreference<SwitchPreference>(R.string.pref_key_show_nonsponsored_suggestions).apply {
                isChecked = context.settings().showNonSponsoredSuggestions
                title = getString(
                    R.string.preferences_show_nonsponsored_suggestions,
                    getString(R.string.app_name),
                )
            }

        searchSuggestionsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()
        showHistorySuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        showBookmarkSuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        showShortcutsSuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        showSyncedTabsSuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        showClipboardSuggestions.onPreferenceChangeListener = SharedPreferenceUpdater()
        searchSuggestionsInPrivatePreference.onPreferenceChangeListener = SharedPreferenceUpdater()
        trendingSearchSuggestionsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()
        recentSearchSuggestionsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()
        autocompleteURLsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()

        searchSuggestionsPreference.setOnPreferenceClickListener {
            searchSuggestionsInPrivatePreference.isEnabled = searchSuggestionsPreference.isChecked
            trendingSearchSuggestionsPreference.isEnabled =
                getSelectedSearchEngine(requireContext())?.trendingUrl != null && searchSuggestionsPreference.isChecked
            true
        }

        showSponsoredSuggestionsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()
        showNonSponsoredSuggestionsPreference.onPreferenceChangeListener = SharedPreferenceUpdater()
    }

    /**
     * Updates the summary of the default search engine preference to display the name
     * of the currently selected search engine.
     */
    @VisibleForTesting
    internal fun updateDefaultSearchEnginePreference() {
        with(requirePreference<Preference>(R.string.pref_key_default_search_engine)) {
            summary = getSelectedSearchEngine(requireContext())?.name
        }
    }

    /**
     * Updates all search widgets. This is necessary when the visibility of the voice search button
     * changes, as the widget needs to be redrawn.
     *
     * @param context The context.
     */
    @VisibleForTesting
    internal fun updateAllWidgets(context: Context) {
        val appWidgetManager = AppWidgetManager.getInstance(context)
        SearchWidgetProvider.updateAllWidgets(context, appWidgetManager)
    }

    /**
     * Gets the currently selected search engine or the default search engine if none is selected.
     *
     * @param context The application context.
     * @return The selected or default [SearchEngine], or null if no search engines are available.
     */
    @VisibleForTesting
    internal fun getSelectedSearchEngine(context: Context): SearchEngine? {
          return context.components.core.store.state.search.selectedOrDefaultSearchEngine
    }

    /**
     * Initialises the "Show voice search" preference.
     *
     * This preference allows the user to enable or disable the voice search feature.
     * When the preference value changes, it updates the corresponding setting in SharedPreferences
     * and triggers an update for all search widgets.
     *
     * @param showVoiceSearchPreference The [SwitchPreference] for the "Show voice search" setting.
     */
    @VisibleForTesting
    internal fun initialiseVoiceSearchPreference(showVoiceSearchPreference: SwitchPreference) {
        showVoiceSearchPreference.onPreferenceChangeListener =
            object : Preference.OnPreferenceChangeListener {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    val newBooleanValue = newValue as? Boolean ?: return false
                    requireContext().settings().preferences.edit {
                        putBoolean(preference.key, newBooleanValue)
                    }
                    updateAllWidgets(requireContext())
                    return true
                }
            }
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            getPreferenceKey(R.string.pref_key_default_search_engine) -> {
                openDefaultEngineSettings()
            }
            getPreferenceKey(R.string.pref_key_manage_search_shortcuts) -> {
                openSearchShortcutsSettings()
            }
            getPreferenceKey(R.string.pref_key_learn_about_fx_suggest) -> {
                openLearnMoreLink()
            }
        }

        return super.onPreferenceTreeClick(preference)
    }

    /**
     * Opens the "Learn More" link for Firefox Suggest in a new browser tab.
     *
     * This function retrieves the appropriate SUMO (support.mozilla.org) URL
     * for the Firefox Suggest topic and instructs the HomeActivity to open
     * this URL in a new browser tab.
     */
    @VisibleForTesting
    internal fun openLearnMoreLink() {
        (activity as HomeActivity).openToBrowserAndLoad(
            searchTermOrURL = SupportUtils.getGenericSumoURLForTopic(
                SupportUtils.SumoTopic.FX_SUGGEST,
            ),
            newTab = true,
            from = BrowserDirection.FromSearchEngineFragment,
        )
    }

    /**
     * Navigates to the search shortcuts settings screen.
     *
     * This function uses the Navigation Component to navigate from the current fragment
     * (SearchEngineFragment) to the SearchShortcutsFragment. It also logs breadcrumbs
     * for crash reporting.
     */
    @VisibleForTesting
    internal fun openSearchShortcutsSettings() {
        val directions = SearchEngineFragmentDirections
            .actionSearchEngineFragmentToSearchShortcutsFragment()
        context?.let {
            findNavController().navigateWithBreadcrumb(
                directions = directions,
                navigateFrom = "SearchEngineFragment",
                navigateTo = "ActionSearchEngineFragmentToSearchShortcutsFragment",
                it.components.analytics.crashReporter,
            )
        }
    }

    @VisibleForTesting
    internal fun openDefaultEngineSettings() {
        val directions = SearchEngineFragmentDirections
            .actionSearchEngineFragmentToDefaultEngineFragment()
        findNavController().navigate(directions)
    }
}

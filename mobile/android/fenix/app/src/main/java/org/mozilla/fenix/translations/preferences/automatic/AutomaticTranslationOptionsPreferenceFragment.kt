/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.translations.preferences.automatic

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.navArgs
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.observeAsComposableState
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A fragment displaying Automatic Translation Options screen.
 */
class AutomaticTranslationOptionsPreferenceFragment : Fragment() {
    private val args by navArgs<AutomaticTranslationOptionsPreferenceFragmentArgs>()
    private val browserStore: BrowserStore by lazy { requireComponents.core.store }

    override fun onResume() {
        super.onResume()
        args.selectedLanguageDisplayName?.let { showToolbar(it) }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setContent {
            val languageSettings = browserStore.observeAsComposableState { state ->
                state.translationEngine.languageSettings
            }.value

            val selectedLanguage = languageSettings?.get(args.selectedLanguageCode)

            FirefoxTheme {
                selectedLanguage?.let {
                    AutomaticTranslationOptionsPreference(
                        selectedOption = getAutomaticTranslationOptionPreference(selectedLanguage),
                        onItemClick = {
                            browserStore.dispatch(
                                TranslationsAction.UpdateLanguageSettingsAction(
                                    languageCode = args.selectedLanguageCode,
                                    setting = getLanguageSetting(it),
                                ),
                            )
                        },
                    )
                }
            }
        }
    }
}

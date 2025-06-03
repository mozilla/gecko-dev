/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import android.app.Activity
import androidx.annotation.VisibleForTesting
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.locale.LocaleManager
import mozilla.components.support.locale.LocaleUseCases
import org.mozilla.focus.locale.Locales
import org.mozilla.focus.settings.InstalledSearchEnginesSettingsFragment
import org.mozilla.gecko.util.ThreadUtils.runOnUiThread
import java.util.Locale

/**
 * [Middleware] responsible for handling actions related to language selection and updates in the
 * language settings screen. It interacts with [LanguageStorage] to persist the selected language and
 * [LocaleUseCases] to apply the selected language to the system.
 *
 * This middleware intercepts [LanguageScreenAction]s, updates the stored language preference, and triggers
 * a locale change that affects the application's displayed language. It also handles the initial population
 * of available languages when the language screen is first displayed.
 *
 * @param activity The current activity. Used for accessing resources and triggering activity recreation.
 * @param localeUseCase Use cases for interacting with locales, provided by [LocaleUseCases].
 * @param storage The storage for managing language preferences, provided by [LanguageStorage].
 * @param getSystemDefault A lambda that returns the system's default locale.
 */
class LanguageMiddleware(
    private val activity: Activity,
    private val localeUseCase: LocaleUseCases,
    private val storage: LanguageStorage,
    private val getSystemDefault: () -> Locale,
) : Middleware<LanguageScreenState, LanguageScreenAction> {

    override fun invoke(
        context: MiddlewareContext<LanguageScreenState, LanguageScreenAction>,
        next: (LanguageScreenAction) -> Unit,
        action: LanguageScreenAction,
    ) {
        when (action) {
            is LanguageScreenAction.Select -> {
                storage.saveCurrentLanguageInSharePref(action.selectedLanguage.tag)
                setCurrentLanguage(action.selectedLanguage.tag)
                next(action)
            }
            is LanguageScreenAction.InitLanguages -> {
                /**
                 * The initial LanguageScreenState when the user enters first in the screen
                 */
                context.dispatch(
                    LanguageScreenAction.UpdateLanguages(
                        storage.languages,
                        storage.selectedLanguage,
                    ),
                )
            }
            else -> {
                next(action)
            }
        }
    }

    /**
     * It changes the system defined locale to the indicated Language .
     * It recreates the current activity for changes to take effect.
     *
     * @param languageTag selected Language Tag that comes from Language object
     */
    internal fun setCurrentLanguage(languageTag: String) {
        InstalledSearchEnginesSettingsFragment.languageChanged = true
        val locale: Locale?

        if (languageTag == LanguageStorage.LOCALE_SYSTEM_DEFAULT) {
            locale = getSystemDefault()
            resetToSystemDefault()
        } else {
            locale = Locales.parseLocaleCode(languageTag)
            setNewLocale(locale)
        }

        activity.applicationContext.resources.apply {
            configuration.setLocale(locale)
            configuration.setLayoutDirection(locale)
            @Suppress("DEPRECATION")
            updateConfiguration(configuration, displayMetrics)
        }

        recreateActivity()
    }

    /**
     * Recreates the current activity to apply language changes.
     * This is necessary for the new locale to take effect throughout the application.
     * The recreation is performed on the UI thread.
     */
    @VisibleForTesting
    internal fun recreateActivity() {
        runOnUiThread { activity.recreate() }
    }

    @VisibleForTesting
    internal fun resetToSystemDefault() {
        LocaleManager.resetToSystemDefault(activity, localeUseCase)
    }

    @VisibleForTesting
    internal fun setNewLocale(locale: Locale) {
        LocaleManager.setNewLocale(activity, localeUseCase, locale)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.locale.screen

import android.content.Context
import android.content.SharedPreferences
import androidx.preference.PreferenceManager
import org.mozilla.focus.R
import org.mozilla.focus.locale.LocaleManager

class LanguageStorage(private val context: Context) {
    private val sharedPref: SharedPreferences =
        PreferenceManager.getDefaultSharedPreferences(context)

    private val localePrefKey: String by lazy {
        context.resources.getString(R.string.pref_key_locale)
    }

    internal val languages: List<Language> by lazy {
        getLanguageList()
    }

    private val systemDefaultLanguage: Language by lazy {
        Language(
            context.getString(R.string.preference_language_systemdefault),
            LOCALE_SYSTEM_DEFAULT,
            0,
        )
    }

    /**
     * The current selected Language or System default Language if nothing is selected
     */
    internal val selectedLanguage: Language
        get() {
            val savedLanguageTag =
                sharedPref.getString(localePrefKey, LOCALE_SYSTEM_DEFAULT) ?: LOCALE_SYSTEM_DEFAULT

            val matchingLanguage = languages.firstOrNull { it.tag == savedLanguageTag }

            return matchingLanguage ?: systemDefaultLanguage
        }

    /**
     * The full list of available languages.
     * System default Language will be the first item in the list.
     */
    private fun getLanguageList(): List<Language> {
        return listOf(
            systemDefaultLanguage,
        ) + getUsableLocales().mapIndexedNotNull { i, descriptor ->
            descriptor?.let {
                Language(
                    displayName = descriptor.getNativeName(),
                    tag = it.getTag(),
                    index = i + 1,
                )
            }
        }
    }

    /**
     * Saves the current selected language tag
     *
     * @property languageTag the tag of the language
     */
    fun saveCurrentLanguageInSharePref(languageTag: String) {
        with(sharedPref.edit()) {
            putString(localePrefKey, languageTag)
            apply()
        }
    }

    /**
     * This method generates the descriptor array.
     */
    private fun getUsableLocales(): Array<LocaleDescriptor?> {
        return LocaleManager.packagedLocaleTags.map {
            LocaleDescriptor(it)
        }.sorted().toTypedArray()
    }

    companion object {
        const val LOCALE_SYSTEM_DEFAULT = "LOCALE_SYSTEM_DEFAULT"
    }
}

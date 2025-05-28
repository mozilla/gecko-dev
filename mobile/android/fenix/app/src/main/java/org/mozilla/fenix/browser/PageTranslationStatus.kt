/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import mozilla.components.concept.engine.translate.Language

/**
 * Translations action button state.
 *
 * @property isTranslationPossible Whether or not translating the current page is possible.
 * @property isTranslated Whether the page is currently translated.
 * @property isTranslateProcessing Whether a translation of the current page is currently in progress.
 * @property fromSelectedLanguage Initial "from" language based on the translation state and page state.
 * @property toSelectedLanguage Initial "to" language based on the translation state and page state.
 */
data class PageTranslationStatus(
    val isTranslationPossible: Boolean,
    val isTranslated: Boolean,
    val isTranslateProcessing: Boolean,
    val fromSelectedLanguage: Language? = null,
    val toSelectedLanguage: Language? = null,
) {
    /**
     * Static configuration and properties of [PageTranslationStatus].
     */
    companion object {
        /**
         * [PageTranslationStatus] for when translating the current page is not possible.
         */
        val NOT_POSSIBLE = PageTranslationStatus(false, false, false)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.concept

import androidx.annotation.IntDef
import androidx.annotation.StringRes
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent

/**
 * Details about the website origin.
 *
 * @property title The title of the website.
 * @property url The URL of the website.
 * @property hint Text displayed in the toolbar when there's no URL to display (e.g.: no tab or empty URL)
 * @property onClick [BrowserToolbarInteraction] describing how to handle any page origin detail being clicked.
 * @property onLongClick Optional [BrowserToolbarInteraction] describing how to handle any page origin detail
 * being long clicked.
 * @property fadeDirection The direction in which the text should fade.
 * @property textGravity The gravity of the text - whether to show the start or end of long text
 * that does not fit the available space.
 */
data class PageOrigin(
    @StringRes val hint: Int,
    val title: String?,
    val url: String?,
    val onClick: BrowserToolbarEvent,
    val onLongClick: BrowserToolbarInteraction? = null,
    @FadeDirection val fadeDirection: Int = FADE_DIRECTION_END,
    @TextGravity val textGravity: Int = TEXT_GRAVITY_START,
) {
    /**
     * Static values used in the configuration of the [PageOrigin] class.
     */
    companion object {
        const val FADE_DIRECTION_NONE = -1
        const val FADE_DIRECTION_START = 0
        const val FADE_DIRECTION_END = 1

        const val TEXT_GRAVITY_START = 0
        const val TEXT_GRAVITY_END = 1

        /**
         * The direction in which the text should fade.
         * Values: [FADE_DIRECTION_NONE], [FADE_DIRECTION_START], [FADE_DIRECTION_END].
         */
        @IntDef(value = [FADE_DIRECTION_NONE, FADE_DIRECTION_START, FADE_DIRECTION_END])
        @Retention(AnnotationRetention.SOURCE)
        annotation class FadeDirection

        /**
         * The gravity of the text - whether to show the start or end of long text.
         * Values: [TEXT_GRAVITY_START], [TEXT_GRAVITY_END].
         */
        @IntDef(value = [TEXT_GRAVITY_START, TEXT_GRAVITY_END])
        @Retention(AnnotationRetention.SOURCE)
        annotation class TextGravity
    }
}

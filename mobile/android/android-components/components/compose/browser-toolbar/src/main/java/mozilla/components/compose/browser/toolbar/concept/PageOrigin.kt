/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.concept

import androidx.annotation.StringRes
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.CopyToClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.LoadFromClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.PageOriginContextualMenuInteractions.PasteFromClipboardClicked
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.TextGravity.TEXT_GRAVITY_START
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent

/**
 * Details about the website origin.
 *
 * @property title The title of the website.
 * @property url The URL of the website.
 * @property hint Text displayed in the toolbar when there's no URL to display (e.g.: no tab or empty URL)
 * @property onClick Optional [BrowserToolbarInteraction] describing how to handle any page origin detail being clicked.
 * @property onLongClick Optional [BrowserToolbarInteraction] describing how to handle any page origin detail
 * being long clicked. To ensure long click functionality the normal click behavior should also be set.
 * @property textGravity The gravity of the text - whether to show the start or end of long text
 * that does not fit the available space.
 */
data class PageOrigin(
    @StringRes val hint: Int,
    val title: String?,
    val url: CharSequence?,
    val textGravity: TextGravity = TEXT_GRAVITY_START,
    val contextualMenuOptions: List<ContextualMenuOption> = emptyList(),
    val onClick: BrowserToolbarEvent?,
    val onLongClick: BrowserToolbarEvent? = null,
) {
    /**
     * Static values used in the configuration of the [PageOrigin] class.
     */
    companion object {
        /**
         * All events dispatched when users interact with the contextual menu shown
         * when long clicking on the page origin.
         */
        sealed class PageOriginContextualMenuInteractions : BrowserToolbarEvent {
            /**
             * [BrowserToolbarEvent] dispatched when the user clicks on the button from the contextual menu
             * shown when long clicking on the page origin that allows copying the current URL to clipboard.
             */
            data object CopyToClipboardClicked : PageOriginContextualMenuInteractions()

            /**
             * [BrowserToolbarEvent] dispatched when the user clicks on the button from the contextual menu
             * shown when long clicking on the page origin that allows pasting the current text from clipboard.
             */
            data object PasteFromClipboardClicked : PageOriginContextualMenuInteractions()

            /**
             * [BrowserToolbarEvent] dispatched when the user clicks on the button from the contextual menu
             * shown when long clicking on the page origin that allows pasting the current text from clipboard
             * with the intention to load it as an URL.
             */
            data object LoadFromClipboardClicked : PageOriginContextualMenuInteractions()
        }

        /**
         * Available options to show in the contextual menu from long clicking on the page origin.
         */
        enum class ContextualMenuOption(val event: PageOriginContextualMenuInteractions) {
            /**
             * Show a button that allows copying the current URL to device's clipboard.
             */
            CopyURLToClipboard(CopyToClipboardClicked),

            /**
             * Show a button that allows pasting the current text from device's clipboard.
             */
            PasteFromClipboard(PasteFromClipboardClicked),

            /**
             * Show a button that allows pasting the current text from device's clipboard with the
             * intention to use it as an URL.
             */
            LoadFromClipboard(LoadFromClipboardClicked),
        }

        /**
         * The gravity of the text - whether to show the start or end of long text
         * that does not fit the available space.
         */
        enum class TextGravity {
            /**
             * Show the start of long text and hide the end part which overflows the available space.
             */
            TEXT_GRAVITY_START,

            /**
             * Show the end of long text and hide the start part which overflows the available space.
             */
            TEXT_GRAVITY_END,
        }
    }
}

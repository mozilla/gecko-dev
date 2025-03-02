/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.store

import androidx.compose.runtime.Composable
import mozilla.components.service.nimbus.messaging.Message
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.compose.MessageCardState
import org.mozilla.fenix.messaging.FenixMessageSurfaceId

/**
 * State representing the text and formatting for a nimbus message card displayed on the homepage.
 *
 * @property cardState State of the message card.
 * @property message Message for callbacks.
 */
data class NimbusMessageState(val cardState: MessageCardState, val message: Message) {

    /**
     * Companion object for building [NimbusMessageState].
     */
    companion object {

        /**
         * Builds a new [NimbusMessageState] from the current [AppState].
         *
         * @param appState State to build the [NimbusMessageState] from.
         */
        @Composable
        internal fun build(appState: AppState) = with(appState) {
            messaging.messageToShow[FenixMessageSurfaceId.HOMESCREEN]?.let {
                NimbusMessageState(
                    cardState = MessageCardState.build(
                        message = it,
                        wallpaperState = wallpaperState,
                    ),
                    message = it,
                )
            }
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Reducer
import mozilla.components.lib.state.UiStore

/**
 * A Store for handling [BookmarksState] and dispatching [BookmarksAction].
 *
 * @param initialState The initial state for the Store.
 * @param reducer Reducer to handle state updates based on dispatched actions.
 * @param middleware Middleware to handle side-effects in response to dispatched actions.
 * @param bookmarkToLoad The guid of a bookmark to load when landing on the edit screen.
 */
internal class BookmarksStore(
    initialState: BookmarksState = BookmarksState.default,
    reducer: Reducer<BookmarksState, BookmarksAction> = ::bookmarksReducer,
    middleware: List<Middleware<BookmarksState, BookmarksAction>> = listOf(),
    bookmarkToLoad: String? = null,
) : UiStore<BookmarksState, BookmarksAction>(
    initialState = initialState,
    reducer = reducer,
    middleware = middleware,
) {
    init {
        val action = bookmarkToLoad?.let { InitEdit(bookmarkToLoad) } ?: Init
        dispatch(action)
    }
}

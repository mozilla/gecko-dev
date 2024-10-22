/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Reducer
import mozilla.components.lib.state.UiStore
import org.mozilla.fenix.HomeActivity

/**
 * A helper class to be able to change the reference to objects that get replaced when the activity
 * gets recreated.
 *
 * @property context the android [Context]
 * @property navController A [NavController] for interacting with the androidx navigation library.
 * @property composeNavController A [NavController] for navigating within the local Composable nav graph.
 * @property homeActivity so that we can reference openToBrowserAndLoad and browsingMode :(
 */
internal class LifecycleHolder(
    var context: Context,
    var navController: NavController,
    var composeNavController: NavController,
    var homeActivity: HomeActivity,
)

/**
 * A Store for handling [BookmarksState] and dispatching [BookmarksAction].
 *
 * @param initialState The initial state for the Store.
 * @param reducer Reducer to handle state updates based on dispatched actions.
 * @param middleware Middleware to handle side-effects in response to dispatched actions.
 * @property lifecycleHolder a hack to box the references to objects that get recreated with the activity.
 * @param bookmarkToLoad The guid of a bookmark to load when landing on the edit screen.
 */
internal class BookmarksStore(
    initialState: BookmarksState = BookmarksState.default,
    reducer: Reducer<BookmarksState, BookmarksAction> = ::bookmarksReducer,
    middleware: List<Middleware<BookmarksState, BookmarksAction>> = listOf(),
    val lifecycleHolder: LifecycleHolder? = null,
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

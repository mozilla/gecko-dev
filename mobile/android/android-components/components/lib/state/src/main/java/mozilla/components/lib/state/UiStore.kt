/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

import androidx.annotation.MainThread
import mozilla.components.lib.state.internal.UiStoreDispatcher

/**
 * A generic store holding an immutable [State] that will always run on the Main thread.
 *
 * The [State] can only be modified by dispatching [Action]s which will create a new state and notify all registered
 * [Observer]s.
 *
 * @param initialState The initial state until a dispatched [Action] creates a new state.
 * @param reducer A function that gets the current [State] and [Action] passed in and will return a new [State].
 * @param middleware Optional list of [Middleware] sitting between the [Store] and the [Reducer].
 */
@MainThread
open class UiStore<S : State, A : Action>(
    initialState: S,
    reducer: Reducer<S, A>,
    middleware: List<Middleware<S, A>> = emptyList(),
) : Store<S, A>(
    initialState,
    reducer,
    middleware,
    UiStoreDispatcher(),
)

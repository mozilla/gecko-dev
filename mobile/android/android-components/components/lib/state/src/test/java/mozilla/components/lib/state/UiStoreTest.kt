/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.IOException

/**
 * The tests here are largely identical to those in [StoreTest], however, these run using [AndroidJUnit4]
 * so that we can run them with a [UiStore] on the main thread with [runTestOnMain].
 *
 * The only new test that is added here is [UiStoreTest.Dispatching on a non-UI dispatcher will throw an exception].
 */
@RunWith(AndroidJUnit4::class)
class UiStoreTest {

    @get:Rule
    val coroutineTestRule = MainCoroutineRule()

    @Test
    fun `Dispatching Action executes reducers and creates new State`() {
        val store = UiStore(
            TestState(counter = 23),
            ::reducer,
        )

        store.dispatch(TestAction.IncrementAction)

        assertEquals(24, store.state.counter)

        store.dispatch(TestAction.DecrementAction)
        store.dispatch(TestAction.DecrementAction)

        assertEquals(22, store.state.counter)
    }

    @Test
    fun `Observer gets notified about state changes`() {
        val store = UiStore(
            TestState(counter = 23),
            ::reducer,
        )

        var observedValue = 0

        store.observeManually { state -> observedValue = state.counter }.also {
            it.resume()
        }

        store.dispatch(TestAction.IncrementAction)

        assertEquals(24, observedValue)
    }

    @Test
    fun `Observer gets initial value before state changes`() {
        val store = UiStore(
            TestState(counter = 23),
            ::reducer,
        )

        var observedValue = 0

        store.observeManually { state -> observedValue = state.counter }.also {
            it.resume()
        }

        assertEquals(23, observedValue)
    }

    @Test
    fun `Middleware can catch exceptions in reducer`() = runTestOnMain {
        var caughtException: Exception? = null

        val catchingMiddleware: Middleware<TestState, TestAction> = { _, next, action ->
            try {
                next(action)
            } catch (e: Exception) {
                caughtException = e
            }
        }

        val store = UiStore(
            TestState(counter = 0),
            { _: State, _: Action -> throw IOException() },
            listOf(catchingMiddleware),
        )

        store.dispatch(TestAction.IncrementAction)

        assertTrue(caughtException is IOException)
    }

    @Test(expected = IllegalThreadStateException::class)
    fun `Dispatching on a non-UI dispatcher will throw an exception`() = runTestOnMain {
        val observingMiddleware: Middleware<TestState, TestAction> = { store, next, action ->
            CoroutineScope(Dispatchers.IO).launch {
                store.dispatch(TestAction.IncrementAction)
            }

            next(action)
        }
        val store = UiStore(
            TestState(counter = 0),
            ::reducer,
            listOf(observingMiddleware),
        )

        store.dispatch(TestAction.DoNothingAction)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import org.junit.Assert.assertEquals
import org.junit.Test

class LoginsReducerTest {
    @Test
    fun `WHEN store initializes THEN no changes to state`() {
        val state = LoginsState()

        assertEquals(state, loginsReducer(state, Init))
    }

    @Test
    fun `WHEN logins are loaded THEN they are added to state`() {
        val state = LoginsState()
        val items = List(5) {
            LoginItem(
                guid = "$it",
                url = "url",
                username = "user$it",
                password = "pass$it",
                timeLastUsed = System.currentTimeMillis(),
            )
        }

        val result = loginsReducer(
            state,
            LoginsLoaded(
                loginItems = items,
            ),
        )

        val expected = state.copy(
            loginItems = items,
        )
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the list logins screen WHEN add login is clicked THEN initialize the add login state`() {
        val state = LoginsState().copy(loginsAddLoginState = LoginsAddLoginState.None)

        val result = loginsReducer(state, InitAdd)

        assertEquals(LoginsAddLoginState.None, result.loginsAddLoginState)
    }

    @Test
    fun `GIVEN there is no substate screen present WHEN back is clicked THEN state is unchanged`() {
        val state = LoginsState()

        val result = loginsReducer(state, LoginsListBackClicked)

        assertEquals(LoginsState(), result)
    }

    @Test
    fun `GIVEN a logins list WHEN the alphabetical sort menu item is clicked THEN sort the logins list`() {
        val items = List(3) {
            LoginItem(
                guid = "$it",
                url = "$it url",
                username = "user$it",
                password = "pass$it",
                timeLastUsed = 0L + it,
            )
        }

        val state = LoginsState().copy(loginItems = items)

        val alphabetical = loginsReducer(state, LoginsListSortMenuAction.OrderByNameClicked)
        assertEquals(listOf(items[0], items[1], items[2]), alphabetical.loginItems)
    }

    @Test
    fun `GIVEN a logins list WHEN the last used sort menu item is clicked THEN sort the logins list`() {
        val items = List(3) {
            LoginItem(
                guid = "$it",
                url = "$it url",
                username = "user$it",
                password = "pass$it",
                timeLastUsed = 0L + it,
            )
        }

        val state = LoginsState().copy(loginItems = items)

        val newest = loginsReducer(state, LoginsListSortMenuAction.OrderByLastUsedClicked)
        assertEquals(listOf(items[2], items[1], items[0]), newest.loginItems)
    }

    @Test
    fun `GIVEN a logins list WHEN the search is used THEN filter the logins list`() {
        val items = List(7) {
            LoginItem(
                guid = "$it",
                url = if (it % 2 == 0) "$it url" else "$it uri",
                username = "user$it",
                password = "pass$it",
                timeLastUsed = System.currentTimeMillis(),
            )
        }

        val state = LoginsState().copy(loginItems = items)

        val filterUrl = loginsReducer(state, SearchLogins("url", items))
        assertEquals("url", filterUrl.searchText)
        assertEquals(4, filterUrl.loginItems.size)
        assertEquals(listOf(items[0], items[2], items[4], items[6]), filterUrl.loginItems)
    }

    @Test
    fun `GIVEN we are on the list logins screen WHEN a login is clicked THEN initialize the detail login state`() {
        val items = List(7) {
            LoginItem(
                guid = "$it",
                url = if (it % 2 == 0) "$it url" else "$it uri",
                username = "user$it",
                password = "pass$it",
                timeLastUsed = System.currentTimeMillis(),
            )
        }

        val state = LoginsState().copy(loginItems = items)
        val result = loginsReducer(state, LoginClicked(items[1]))
        val expectedState = state.copy(loginsLoginDetailState = LoginsLoginDetailState(items[1]))

        assertEquals(result.loginsLoginDetailState, expectedState.loginsLoginDetailState)
        assertEquals(result, expectedState)
    }

    @Test
    fun `WHEN login is clicked THEN it is added to state`() {
        val state = LoginsState()
        val loginItem = LoginItem(
            guid = "guid123",
            url = "url123",
            username = "user123",
            password = "pass123",
            timeLastUsed = System.currentTimeMillis(),
        )

        val result = loginsReducer(
            state,
            LoginClicked(item = loginItem),
        )

        val expected = state.copy(
            loginsLoginDetailState = LoginsLoginDetailState(loginItem),
        )
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN we are on the login details screen WHEN the back button is clicked THEN go back to login list state`() {
        val items = List(7) {
            LoginItem(
                guid = "$it",
                url = if (it % 2 == 0) "$it url" else "$it uri",
                username = "user$it",
                password = "pass$it",
                timeLastUsed = System.currentTimeMillis(),
            )
        }

        val state = LoginsState().copy(loginItems = items)
        loginsReducer(state, LoginClicked(items[1]))

        val resultListStateAfterBackClick = loginsReducer(state, LoginsListBackClicked)
        val expectedListStateAfterBackClick = state.copy(loginsLoginDetailState = null)

        assertEquals(resultListStateAfterBackClick, expectedListStateAfterBackClick)
    }
}

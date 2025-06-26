/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins.ui

import android.content.ClipboardManager
import androidx.navigation.NavController
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.storage.Login
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class LoginsMiddlewareTest {

    @get:Rule
    val coroutineRule = MainCoroutineRule()

    private lateinit var loginsStorage: LoginsStorage
    private lateinit var clipboardManager: ClipboardManager
    private lateinit var navController: NavController
    private lateinit var exitLogins: () -> Unit
    private lateinit var showUsernameCopiedSnackbar: () -> Unit
    private lateinit var showPasswordCopiedSnackbar: () -> Unit
    private lateinit var openTab: (String, Boolean) -> Unit
    private lateinit var persistLoginsSortOrder: suspend (LoginsSortOrder) -> Unit

    private val loginList = List(5) {
        Login(
            guid = "guid$it",
            origin = "origin$it",
            username = "username$it",
            password = "password$it",
        )
    }

    @Before
    fun setup() {
        loginsStorage = mock()
        clipboardManager = mock()
        navController = mock()
        exitLogins = { }
        openTab = { _, _ -> }
        persistLoginsSortOrder = { }
        showUsernameCopiedSnackbar = { }
        showPasswordCopiedSnackbar = { }
    }

    @Test
    fun `GIVEN no logins in storage WHEN store is initialized THEN list of logins will be empty`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(listOf())
            val middleware = buildMiddleware()
            val store = middleware.makeStore()
            store.waitUntilIdle()

            assertEquals(0, store.state.loginItems.size)
        }

    @Test
    fun `GIVEN current screen is list logins WHEN add password is clicked THEN navigate to add login screen`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(listOf())

            val middleware = buildMiddleware()
            val store = middleware.makeStore()
            store.dispatch(InitAdd)
            store.waitUntilIdle()
            verify(navController).navigate(LoginsDestinations.ADD_LOGIN)
        }

    @Test
    fun `GIVEN current screen is list logins WHEN any login is clicked THEN navigate to detail login screen`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(loginList)

            val middleware = buildMiddleware()
            val store = middleware.makeStore()
            store.dispatch(
                LoginClicked(
                    LoginItem(
                        guid = "guid1",
                        url = "url1",
                        username = "u1",
                        password = "p1",
                        timeLastUsed = 0L,
                    ),
                ),
            )
            store.waitUntilIdle()
            verify(navController).navigate(LoginsDestinations.LOGIN_DETAILS)
        }

    @Test
    fun `GIVEN current screen is list logins WHEN a login is clicked THEN navigate to edit login screen`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(loginList)
            val middleware = buildMiddleware()
            val store = middleware.makeStore()

            store.dispatch(
                DetailLoginMenuAction.EditLoginMenuItemClicked(
                    LoginItem(
                        guid = "guid1",
                        url = "url1",
                        username = "u1",
                        password = "p1",
                        timeLastUsed = 0L,
                    ),
                ),
            )
            store.waitUntilIdle()

            verify(navController).navigate(LoginsDestinations.EDIT_LOGIN)
        }

    @Test
    fun `GIVEN current screen is list and the top-level is loaded WHEN back is clicked THEN exit logins`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(loginList)
            var exited = false
            exitLogins = { exited = true }
            val middleware = buildMiddleware()
            val store = middleware.makeStore()

            store.dispatch(LoginsListBackClicked)
            store.waitUntilIdle()

            assertTrue(exited)
        }

    @Test
    fun `GIVEN logins in storage WHEN store is initialized THEN logins will be loaded as display format`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(loginList)
            val middleware = buildMiddleware()
            val store = middleware.makeStore()
            store.waitUntilIdle()

            assertEquals(5, store.state.loginItems.size)
        }

    @Test
    fun `GIVEN a logins store WHEN SortMenuItem is clicked THEN Save the new sort order`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(loginList)
            var newSortOrder = LoginsSortOrder.default
            persistLoginsSortOrder = {
                newSortOrder = it
            }
            val middleware = buildMiddleware()
            val store = middleware.makeStore()
            store.dispatch(LoginsListSortMenuAction.OrderByLastUsedClicked)
            store.waitUntilIdle()
            assertEquals(LoginsSortOrder.LastUsed, newSortOrder)
        }

    @Test
    fun `GIVEN login detail screen WHEN a login url button is clicked THEN open it in new tab`() =
        runTestOnMain {
            `when`(loginsStorage.list()).thenReturn(loginList)
            val url = loginList[2].origin
            var capturedUrl = ""
            var capturedNewTab = false
            openTab = { urlCalled, newTab ->
                capturedUrl = urlCalled
                capturedNewTab = newTab
            }

            val middleware = buildMiddleware()
            val store = middleware.makeStore()

            store.dispatch(DetailLoginAction.GoToSiteClicked(loginList[2].origin))

            assertEquals(url, capturedUrl)
            assertTrue(capturedNewTab)
        }

    private fun buildMiddleware() = LoginsMiddleware(
        loginsStorage = loginsStorage,
        getNavController = { navController },
        exitLogins = exitLogins,
        openTab = openTab,
        ioDispatcher = coroutineRule.testDispatcher,
        persistLoginsSortOrder = persistLoginsSortOrder,
        clipboardManager = clipboardManager,
        showUsernameCopiedSnackbar = showUsernameCopiedSnackbar,
        showPasswordCopiedSnackbar = showPasswordCopiedSnackbar,
    )

    private fun LoginsMiddleware.makeStore(
        initialState: LoginsState = LoginsState(),
    ) = LoginsStore(
        initialState = initialState,
        middleware = listOf(this),
    ).also {
        it.waitUntilIdle()
    }
}

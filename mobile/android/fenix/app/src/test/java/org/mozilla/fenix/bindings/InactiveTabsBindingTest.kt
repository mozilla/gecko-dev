/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.bindings

import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.tabstray.InactiveTabsBinding
import org.mozilla.fenix.tabstray.TabsTrayAction
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

class InactiveTabsBindingTest {

    @get:Rule
    val coroutineRule = MainCoroutineRule()

    lateinit var tabsTrayStore: TabsTrayStore
    lateinit var appStore: AppStore

    private val tabId1 = "1"
    private val tab1 = createTab(url = tabId1, id = tabId1)

    @Test
    fun `WHEN inactiveTabsExpanded changes THEN tabs tray action dispatched with update`() = runTestOnMain {
        appStore = AppStore(
            AppState(
                inactiveTabsExpanded = false,
            ),
        )
        tabsTrayStore = spy(
            TabsTrayStore(
                TabsTrayState(
                    inactiveTabs = listOf(tab1),
                    inactiveTabsExpanded = false,
                ),
            ),
        )

        val binding = InactiveTabsBinding(
            appStore = appStore,
            tabsTrayStore = tabsTrayStore,
        )
        binding.start()
        appStore.dispatch(AppAction.UpdateInactiveExpanded(true)).joinBlocking()

        verify(tabsTrayStore).dispatch(TabsTrayAction.UpdateInactiveExpanded(true))
    }
}

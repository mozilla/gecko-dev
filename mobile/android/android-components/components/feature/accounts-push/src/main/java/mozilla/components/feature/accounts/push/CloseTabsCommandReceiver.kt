/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * Closes open tabs on this device that other devices in the
 * [DeviceConstellation] have requested to close.
 *
 * @param browserStore The [BrowserStore] that holds the currently open tabs.
 */
class CloseTabsCommandReceiver(
    private val browserStore: BrowserStore,
) : Observable<CloseTabsCommandReceiver.Observer> by ObserverRegistry() {
    /**
     * Processes a [DeviceCommandIncoming.TabsClosed] command
     * received from another device in the [DeviceConstellation].
     *
     * @param command The received command.
     */
    fun receive(command: DeviceCommandIncoming.TabsClosed) {
        val openTabs = browserStore.state.normalTabs
        val urlsToClose = command.urls.toSet()
        val tabsToRemove = openTabs.filter { urlsToClose.contains(it.content.url) }.ifEmpty { return }
        val remainingTabsCount = openTabs.size - tabsToRemove.size
        val willCloseLastTab = tabsToRemove.any { it.id == browserStore.state.selectedTabId } && remainingTabsCount <= 0

        browserStore.dispatch(TabListAction.RemoveTabsAction(tabsToRemove.map { it.id }))

        notifyObservers { onTabsClosed(tabsToRemove.map { it.content.url }) }
        if (willCloseLastTab) {
            notifyObservers { onLastTabClosed() }
        }
    }

    /** Receives notifications for closed tabs. */
    interface Observer {
        /**
         * One or more tabs on this device were closed.
         *
         * @param urls The URLs of the tabs that were closed.
         */
        fun onTabsClosed(urls: List<String>) = Unit

        /** The last tab on this device was closed. */
        fun onLastTabClosed() = Unit
    }
}

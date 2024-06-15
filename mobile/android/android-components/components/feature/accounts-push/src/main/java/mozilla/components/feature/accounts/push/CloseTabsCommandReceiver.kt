/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.TabSessionState
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
        val tabsToRemove = getTabsToRemove(command.urls).ifEmpty { return }
        val remainingTabsCount = browserStore.state.tabs.size - tabsToRemove.size
        val willCloseLastTab = tabsToRemove.any { it.id == browserStore.state.selectedTabId } && remainingTabsCount <= 0

        browserStore.dispatch(TabListAction.RemoveTabsAction(tabsToRemove.map { it.id }))

        notifyObservers { onTabsClosed(tabsToRemove.map { it.content.url }) }
        if (willCloseLastTab) {
            notifyObservers { onLastTabClosed() }
        }
    }

    private fun getTabsToRemove(remotelyClosedUrls: List<String>): List<TabSessionState> {
        // The user might have the same URL open in multiple tabs on this device, and might want
        // to remotely close some or all of those tabs. Synced tabs don't carry enough
        // information to know which duplicates the user meant to close, so we use a heuristic:
        // if a URL appears N times in the remotely closed URLs list, we'll close up to
        // N instances of that URL.
        val countsByUrl = remotelyClosedUrls.groupingBy { it }.eachCount()
        return browserStore.state.tabs
            .groupBy { it.content.url }
            .asSequence()
            .mapNotNull { (url, tabs) ->
                countsByUrl[url]?.let { count -> tabs.take(count) }
            }
            .flatten()
            .toList()
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.concept.sync.AccountEvent
import mozilla.components.concept.sync.AccountEventsObserver
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceConstellation
import mozilla.components.service.fxa.manager.FxaAccountManager

/**
 * A feature for closing tabs on this device from other devices
 * in the [DeviceConstellation].
 *
 * This feature receives commands to close tabs using the [FxaAccountManager].
 *
 * See [CloseTabsUseCases] for the ability to close tabs that are open on
 * other devices from this device.
 *
 * @param receiver A [CloseTabsCommandReceiver] to process the received commands.
 * @param accountManager The account manager.
 * @param owner The Android lifecycle owner for the observers. Defaults to
 * the [ProcessLifecycleOwner].
 * @param autoPause Whether or not the observer should automatically be
 * paused/resumed with the bound lifecycle.
 */
class CloseTabsFeature(
    private val receiver: CloseTabsCommandReceiver,
    private val accountManager: FxaAccountManager,
    private val owner: LifecycleOwner = ProcessLifecycleOwner.get(),
    private val autoPause: Boolean = false,
) {
    @VisibleForTesting internal val observer = TabsClosedEventsObserver { device, urls ->
        receiver.receive(DeviceCommandIncoming.TabsClosed(device, urls))
    }

    /**
     * Begins observing the [accountManager] for "tabs closed" events.
     */
    fun observe() {
        accountManager.registerForAccountEvents(observer, owner, autoPause)
    }
}

internal class TabsClosedEventsObserver(
    internal val onTabsClosed: (Device?, List<String>) -> Unit,
) : AccountEventsObserver {
    override fun onEvents(events: List<AccountEvent>) {
        // Group multiple commands from the same device, so that we can close
        // more tabs at once.
        events.asSequence()
            .filterIsInstance<AccountEvent.DeviceCommandIncoming>()
            .map { it.command }
            .filterIsInstance<DeviceCommandIncoming.TabsClosed>()
            .groupingBy { it.from }
            .fold(emptyList<String>()) { urls, command -> urls + command.urls }
            .forEach { (device, urls) ->
                onTabsClosed(device, urls)
            }
    }
}

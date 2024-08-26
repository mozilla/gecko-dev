/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.accounts

import android.app.Activity
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.webextension.WebExtensionRuntime
import mozilla.components.feature.accounts.FxaCapability
import mozilla.components.feature.accounts.FxaWebChannelFeature
import mozilla.components.feature.accounts.FxaWebChannelFeature.Companion.WebChannelCommand
import mozilla.components.service.fxa.ServerConfig
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.ext.isIntentInternal
import java.lang.ref.WeakReference

/**
 * An integration class that connects the [FxaWebChannelFeature] with custom web command handling
 * for Fenix UI affordances. For example, "log out" messages from web content will close the view.
 *
 * @param customTabSessionId see [FxaWebChannelFeature.customTabSessionId].
 * @param runtime  see [FxaWebChannelFeature.runtime].
 * @param store see [FxaWebChannelFeature.store].
 * @param accountManager see [FxaWebChannelFeature.accountManager].
 * @param serverConfig see [FxaWebChannelFeature.serverConfig].
 * @property activityRef a reference to provide the [Activity] to dismiss.
 */
@Suppress("OutdatedDocumentation") // false-positive
class FxaWebChannelIntegration(
    customTabSessionId: String?,
    runtime: WebExtensionRuntime,
    store: BrowserStore,
    accountManager: FxaAccountManager,
    serverConfig: ServerConfig,
    private val activityRef: WeakReference<Activity?>,
) : LifecycleAwareFeature {
    private val feature by lazy {
        FxaWebChannelFeature(
            customTabSessionId = customTabSessionId,
            runtime = runtime,
            store = store,
            accountManager = accountManager,
            serverConfig = serverConfig,
            fxaCapabilities = setOf(FxaCapability.CHOOSE_WHAT_TO_SYNC),
            onCommandExecuted = ::handleWebCommandLogOut,
        )
    }

    override fun start() {
        feature.start()
    }

    override fun stop() {
        feature.stop()
    }

    private fun handleWebCommandLogOut(command: WebChannelCommand) {
        if (command != WebChannelCommand.LOGOUT && command != WebChannelCommand.DELETE_ACCOUNT) {
            return
        }

        // We could get this command while the activity is no longer available, so we only take it
        // when we are about to use it.
        val activity = activityRef.get() ?: return

        val isInternalIntent = activity.isIntentInternal()
        val isExternalTabActivity = activity is ExternalAppBrowserActivity

        // We use a Custom Tab to show the account, but to avoid mistakenly closing a real
        // Custom Tab, we need to make sure the intent is internal.
        if (!isExternalTabActivity && !isInternalIntent) {
            return
        }

        activity.finish()
    }
}

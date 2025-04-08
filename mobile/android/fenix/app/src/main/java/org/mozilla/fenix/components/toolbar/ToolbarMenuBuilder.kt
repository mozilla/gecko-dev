/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.HapticFeedbackConstants
import android.view.View
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.utils.Settings

/**
 * Helper to build a [ToolbarMenu] for the browser toolbar.
 * This will show different options depending on if a custom tab session is active.
 *
 * @param context [Context] used for various system interactions.
 * @param components [Components] for building and configuring the menu.
 * @param settings [Settings] for accessing user preferences.
 * @param interactor [BrowserToolbarInteractor] for handling user interactions with the menu items.
 * @param lifecycleOwner [LifecycleOwner] for preventing dangling long running jobs.
 * @param customTabSessionId The ID of the custom tab session, if one is active.
 */
internal class ToolbarMenuBuilder(
    private val context: Context,
    private val components: Components,
    private val settings: Settings,
    private val interactor: BrowserToolbarInteractor,
    private val lifecycleOwner: LifecycleOwner,
    private val customTabSessionId: String?,
) {
    fun build(): ToolbarMenu = when (customTabSessionId) {
        null -> DefaultToolbarMenu(
            context = context,
            store = components.core.store,
            hasAccountProblem = components.backgroundServices.accountManager.accountNeedsReauth(),
            onItemTapped = {
                it.performHapticIfNeeded(View(context))
                interactor.onBrowserToolbarMenuItemTapped(it)
            },
            lifecycleOwner = lifecycleOwner,
            bookmarksStorage = context.bookmarkStorage,
            pinnedSiteStorage = components.core.pinnedSiteStorage,
            isPinningSupported = components.useCases.webAppUseCases.isPinningSupported(),
        )

        else -> CustomTabToolbarMenu(
            context = context,
            store = components.core.store,
            sessionId = customTabSessionId,
            shouldReverseItems = settings.toolbarPosition == ToolbarPosition.TOP,
            isSandboxCustomTab = false,
            onItemTapped = {
                it.performHapticIfNeeded(View(context))
                interactor.onBrowserToolbarMenuItemTapped(it)
            },
        )
    }
}

@Suppress("ComplexCondition")
private fun ToolbarMenu.Item.performHapticIfNeeded(view: View) {
    if (this is ToolbarMenu.Item.Reload && this.bypassCache ||
        this is ToolbarMenu.Item.Back && this.viewHistory ||
        this is ToolbarMenu.Item.Forward && this.viewHistory
    ) {
        view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.view.Window
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.android.view.setNavigationBarTheme
import mozilla.components.support.ktx.android.view.setStatusBarTheme
import org.mozilla.fenix.browser.store.BrowserScreenState
import org.mozilla.fenix.browser.store.BrowserScreenStore

/**
 * [BrowserScreenStore] binding for observing custom colors changes and updating with which to
 * update the system navigation bar's backgrounds.
 *
 * @param browserScreenStore [BrowserScreenStore] to observe for custom colors changes.
 * @param window [Window] allowing to update the system bars' backgrounds.
 */
class CustomTabColorsBinding(
    browserScreenStore: BrowserScreenStore,
    private val window: Window? = null,
) : AbstractBinding<BrowserScreenState>(browserScreenStore) {
    override suspend fun onState(flow: Flow<BrowserScreenState>) {
        flow.distinctUntilChangedBy { it.customTabColors }
            .collect {
                val customColors = it.customTabColors ?: return@collect
                updateTheme(
                     statusBarColor = customColors.systemBarsColor,
                     navigationBarColor = customColors.systemBarsColor,
                     navigationBarDividerColor = customColors.navigationBarDividerColor,
                )
            }
    }

    @VisibleForTesting
    internal fun updateTheme(
        @ColorInt statusBarColor: Int? = null,
        @ColorInt navigationBarColor: Int? = null,
        @ColorInt navigationBarDividerColor: Int? = null,
    ) {
        if (statusBarColor != null) {
            window?.setStatusBarTheme(statusBarColor)
        }

        if (navigationBarColor != null || navigationBarDividerColor != null) {
            window?.setNavigationBarTheme(navigationBarColor, navigationBarDividerColor)
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.annotation.SuppressLint
import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.AbstractComposeView
import mozilla.components.concept.toolbar.ScrollableToolbar

/**
 * A composable View that also implements the [ScrollableToolbar] interface.
 *
 * @param context [Context] used for instantiating this View.
 * @param scrollableToolbarDelegate [ScrollableToolbar] which will have all related methods delegated to.
 * @param content [Composable] content to be displayed.
 */
@SuppressLint("ViewConstructor")
class ScrollableToolbarComposeView(
    context: Context,
    private val scrollableToolbarDelegate: ScrollableToolbar,
    private val content: @Composable () -> Unit = {},
) : ScrollableToolbar, AbstractComposeView(context) {
    override fun enableScrolling() = scrollableToolbarDelegate.enableScrolling()

    override fun disableScrolling() = scrollableToolbarDelegate.disableScrolling()

    override fun expand() = scrollableToolbarDelegate.expand()

    override fun collapse() = scrollableToolbarDelegate.collapse()

    @Composable
    override fun Content() = content()
}

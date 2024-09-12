/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.session.ui

import androidx.annotation.DrawableRes
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.state.state.TabSessionState
import org.mozilla.focus.R
import org.mozilla.focus.databinding.ItemSessionBinding
import org.mozilla.focus.ext.beautifyUrl
import java.lang.ref.WeakReference

class TabViewHolder(
    private val binding: ItemSessionBinding,
) : RecyclerView.ViewHolder(binding.root) {

    private var tabReference: WeakReference<TabSessionState> = WeakReference<TabSessionState>(null)

    /**
     * Binds a given tab to the view holder or adds additional options if tab is null.
     *
     * @param tab The tab session state to bind. Can be null.
     * @param isCurrentSession Indicates if the tab is the current session.
     * @param selectSession Function to call when the tab is selected.
     * @param closeSession Function to call when the tab is closed.
     * @param closeOtherSessions Function to call when closing all other tabs.
     */

    fun bind(
        tab: TabSessionState?,
        isCurrentSession: Boolean,
        selectSession: (TabSessionState) -> Unit,
        closeSession: (TabSessionState) -> Unit,
        closeOtherSessions: () -> Unit = {},
    ) {
        val drawable = if (isCurrentSession) {
            R.drawable.background_list_item_current_session
        } else {
            R.drawable.background_list_item_session
        }

        if (tab != null) {
            bindTab(tab, drawable, selectSession, closeSession)
        } else {
            bindCloseAllTabs(drawable, closeOtherSessions)
        }
    }

    /**
     * Binds the given tab to the view holder.
     *
     * @param tab The tab session state to bind.
     * @param drawable The background drawable resource ID.
     * @param selectSession Function to call when the tab is selected.
     * @param closeSession Function to call when the tab is closed.
     */
    private fun bindTab(
        tab: TabSessionState,
        @DrawableRes drawable: Int,
        selectSession: (TabSessionState) -> Unit,
        closeSession: (TabSessionState) -> Unit,
    ) {
        tabReference = WeakReference(tab)

        val title = tab.content.title.ifEmpty { tab.content.url.beautifyUrl() }

        binding.sessionItem.setBackgroundResource(drawable)
        binding.sessionTitle.apply {
            setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_link, 0, 0, 0)
            text = title
            setOnClickListener {
                val clickedTab = tabReference.get() ?: return@setOnClickListener
                selectSession(clickedTab)
            }
        }

        binding.closeButton.setOnClickListener {
            val clickedTab = tabReference.get() ?: return@setOnClickListener
            closeSession(clickedTab)
        }
    }

    /**
     * Binds the view holder to close all other tabs.
     *
     * @param drawable The background drawable resource ID.
     * @param closeOtherSessions Function to call when closing all other tabs.
     */
    private fun bindCloseAllTabs(
        @DrawableRes drawable: Int,
        closeOtherSessions: () -> Unit,
    ) {
        binding.sessionItem.setBackgroundResource(drawable)

        val drawableWidth =
            ContextCompat.getDrawable(binding.root.context, R.drawable.ic_link)?.intrinsicWidth ?: 0

        binding.sessionTitle.apply {
            text = binding.root.context.getString(R.string.tabs_tray_action_erase_other)
            setCompoundDrawablesWithIntrinsicBounds(0, 0, 0, 0)

            setPaddingRelative(
                paddingStart + drawableWidth + compoundDrawablePadding,
                paddingTop,
                paddingRight,
                paddingBottom,
            )

            setOnClickListener {
                closeOtherSessions.invoke()
            }
        }

        binding.closeButton.isVisible = false
    }
}

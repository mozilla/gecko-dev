/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.ui

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.lib.crash.R

/**
 * RecyclerView adapter for displaying the list of crashes.
 */
internal class CrashListAdapter(
    private val onShareCrashClicked: (DisplayableCrash) -> Unit,
    private val onCrashServiceSelected: (DisplayableCrash.Report) -> Unit,
) : ListAdapter<DisplayableCrash, CrashViewHolder>(CrashListDiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CrashViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.mozac_lib_crash_item_crash, parent, false)
        return CrashViewHolder(view, onShareCrashClicked, onCrashServiceSelected)
    }

    override fun onBindViewHolder(holder: CrashViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    internal object CrashListDiffCallback : DiffUtil.ItemCallback<DisplayableCrash>() {
        override fun areItemsTheSame(
            oldItem: DisplayableCrash,
            newItem: DisplayableCrash,
        ): Boolean = oldItem.uuid == newItem.uuid

        override fun areContentsTheSame(oldItem: DisplayableCrash, newItem: DisplayableCrash): Boolean =
            oldItem == newItem
    }
}

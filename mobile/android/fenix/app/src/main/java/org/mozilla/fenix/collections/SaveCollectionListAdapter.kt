/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.core.graphics.BlendModeColorFilterCompat.createBlendModeColorFilterCompat
import androidx.core.graphics.BlendModeCompat.SRC_IN
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.components.description
import org.mozilla.fenix.databinding.CollectionsListItemBinding
import org.mozilla.fenix.ext.getIconColor
import org.mozilla.fenix.utils.view.ViewHolder

/**
 * Adapter for a list of collections.
 *
 * @param onCollectionClickedListener Invoked when a collection is clicked.
 */
class SaveCollectionListAdapter(
    private val onCollectionClickedListener: (TabCollection) -> Unit,
) : ListAdapter<TabCollection, CollectionViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CollectionViewHolder {
        val binding = CollectionsListItemBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false,
        )

        return CollectionViewHolder(binding)
    }

    override fun onBindViewHolder(holder: CollectionViewHolder, position: Int) {
        val collection = getItem(position)
        holder.bind(collection)
        holder.itemView.setOnClickListener {
            onCollectionClickedListener(collection)
        }
    }

    private object DiffCallback : DiffUtil.ItemCallback<TabCollection>() {
        override fun areItemsTheSame(oldItem: TabCollection, newItem: TabCollection) =
            oldItem.id == newItem.id

        @SuppressLint("DiffUtilEquals")
        override fun areContentsTheSame(oldItem: TabCollection, newItem: TabCollection) =
            oldItem.title == newItem.title && oldItem.tabs == newItem.tabs
    }
}

class CollectionViewHolder(private val binding: CollectionsListItemBinding) : ViewHolder(binding.root) {

    fun bind(collection: TabCollection) {
        binding.collectionItem.text = collection.title
        binding.collectionDescription.text = collection.description(itemView.context)
        binding.collectionIcon.colorFilter =
            createBlendModeColorFilterCompat(collection.getIconColor(itemView.context), SRC_IN)
    }
}

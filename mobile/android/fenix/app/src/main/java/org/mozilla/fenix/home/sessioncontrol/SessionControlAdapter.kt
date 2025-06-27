/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.service.nimbus.messaging.Message
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.home.collections.CollectionViewHolder
import org.mozilla.fenix.home.collections.TabInCollectionViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.NoCollectionsMessageViewHolder
import org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding.MessageCardViewHolder
import mozilla.components.feature.tab.collections.Tab as ComponentTab

sealed class AdapterItem(@LayoutRes val viewType: Int) {
    object NoCollectionsMessage : AdapterItem(NoCollectionsMessageViewHolder.LAYOUT_ID)

    data class CollectionItem(
        val collection: TabCollection,
        val expanded: Boolean,
    ) : AdapterItem(CollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is CollectionItem && collection.id == other.collection.id

        override fun contentsSameAs(other: AdapterItem): Boolean {
            (other as? CollectionItem)?.let {
                return it.expanded == this.expanded &&
                    it.collection.title == this.collection.title &&
                    it.collection.tabs == this.collection.tabs
            } ?: return false
        }
    }

    data class TabInCollectionItem(
        val collection: TabCollection,
        val tab: ComponentTab,
        val isLastTab: Boolean,
    ) : AdapterItem(TabInCollectionViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is TabInCollectionItem && tab.id == other.tab.id

        override fun contentsSameAs(other: AdapterItem): Boolean {
            return other is TabInCollectionItem && this.isLastTab == other.isLastTab
        }
    }

    data class NimbusMessageCard(
        val message: Message,
    ) : AdapterItem(MessageCardViewHolder.LAYOUT_ID) {
        override fun sameAs(other: AdapterItem) =
            other is NimbusMessageCard && message.id == other.message.id
    }

    /**
     * True if this item represents the same value as other. Used by [AdapterItemDiffCallback].
     */
    open fun sameAs(other: AdapterItem) = this::class == other::class

    /**
     * Returns a payload if there's been a change, or null if not
     */
    open fun getChangePayload(newItem: AdapterItem): Any? = null

    open fun contentsSameAs(other: AdapterItem) = this::class == other::class
}

class AdapterItemDiffCallback : DiffUtil.ItemCallback<AdapterItem>() {
    override fun areItemsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
        oldItem.sameAs(newItem)

    @Suppress("DiffUtilEquals")
    override fun areContentsTheSame(oldItem: AdapterItem, newItem: AdapterItem) =
        oldItem.contentsSameAs(newItem)

    override fun getChangePayload(oldItem: AdapterItem, newItem: AdapterItem): Any? {
        return oldItem.getChangePayload(newItem) ?: return super.getChangePayload(oldItem, newItem)
    }
}

class SessionControlAdapter(
    private val interactor: SessionControlInteractor,
    private val viewLifecycleOwner: LifecycleOwner,
    private val components: Components,
) : ListAdapter<AdapterItem, RecyclerView.ViewHolder>(AdapterItemDiffCallback()) {

    // This method triggers the ComplexMethod lint error when in fact it's quite simple.
    @SuppressWarnings("ComplexMethod", "LongMethod", "ReturnCount")
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        when (viewType) {
            MessageCardViewHolder.LAYOUT_ID -> return MessageCardViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            CollectionViewHolder.LAYOUT_ID -> return CollectionViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
            TabInCollectionViewHolder.LAYOUT_ID -> return TabInCollectionViewHolder(
                composeView = ComposeView(parent.context),
                viewLifecycleOwner = viewLifecycleOwner,
                interactor = interactor,
            )
        }

        val view = LayoutInflater.from(parent.context).inflate(viewType, parent, false)
        return when (viewType) {
            NoCollectionsMessageViewHolder.LAYOUT_ID ->
                NoCollectionsMessageViewHolder(
                    view,
                    viewLifecycleOwner,
                    components.core.store,
                    components.appStore,
                    interactor,
                )
            else -> throw IllegalStateException()
        }
    }

    override fun onViewRecycled(holder: RecyclerView.ViewHolder) {
        when (holder) {
            is CollectionViewHolder -> {
                // Dispose the underlying composition immediately.
                // This ViewHolder can be removed / re-added and we need it to show a fresh new composition.
                holder.composeView.disposeComposition()
            }
            is MessageCardViewHolder -> {
                // Dispose the underlying composition immediately.
                // This ViewHolder can be removed / re-added and we need it to show a fresh new composition.
                holder.composeView.disposeComposition()
            }
            is TabInCollectionViewHolder -> {
                // Dispose the underlying composition immediately.
                // This ViewHolder can be removed / re-added and we need it to show a fresh new composition.
                holder.composeView.disposeComposition()
            }
            else -> super.onViewRecycled(holder)
        }
    }

    override fun getItemViewType(position: Int) = getItem(position).viewType

    override fun onBindViewHolder(
        holder: RecyclerView.ViewHolder,
        position: Int,
        payloads: MutableList<Any>,
    ) {
        if (payloads.isNullOrEmpty()) {
            onBindViewHolder(holder, position)
        }
    }

    @SuppressWarnings("ComplexMethod")
    override fun onBindViewHolder(holder: RecyclerView.ViewHolder, position: Int) {
        val item = getItem(position)
        when (holder) {
            is MessageCardViewHolder -> {
                holder.bind((item as AdapterItem.NimbusMessageCard).message)
            }
            is CollectionViewHolder -> {
                val (collection, expanded) = item as AdapterItem.CollectionItem
                holder.bindSession(collection, expanded)
            }
            is TabInCollectionViewHolder -> {
                val (collection, tab, isLastTab) = item as AdapterItem.TabInCollectionItem
                holder.bindSession(collection, tab, isLastTab)
            }
        }
    }
}

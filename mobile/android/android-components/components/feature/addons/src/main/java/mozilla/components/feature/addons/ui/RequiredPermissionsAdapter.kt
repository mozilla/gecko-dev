/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.annotation.VisibleForTesting
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView.ViewHolder
import mozilla.components.feature.addons.R

private const val VIEW_HOLDER_TYPE_PERMISSION = 0
private const val VIEW_HOLDER_TYPE_PERMISSION_DOMAIN = 1
private const val VIEW_HOLDER_TYPE_SHOW_HIDE_SITES = 2

private const val DOMAINS_CONTRACTED_SUBLIST_SIZE = 5

/**
 * Classes of items that can be included in [RequiredPermissionsAdapter]
 */
sealed class RequiredPermissionsListItem {

    /**
     * A permission to show as a list item.
     * @param permissionText - The text to show in the compound textview
     */
    class PermissionItem(
        val permissionText: String,
    ) : RequiredPermissionsListItem()

    /**
     * A list item of a domain that is required
     * @param domain - The domain text
     */
    class DomainItem(
        val domain: String,
    ) : RequiredPermissionsListItem()

    /**
     * A textview that is clickable to show or hide the full list of domain items
     * @param isShowAction - Whether the clickable text is for showing or hiding the full list
     * of domains
     */
    class ShowHideDomainAction(
        val isShowAction: Boolean,
    ) : RequiredPermissionsListItem()
}

/**
 * [RequiredPermissionsAdapter] diff util for [RequiredPermissionsListItem]
 */
private class DiffCallback : DiffUtil.ItemCallback<RequiredPermissionsListItem>() {
    override fun areItemsTheSame(
        oldItem: RequiredPermissionsListItem,
        newItem: RequiredPermissionsListItem,
    ): Boolean {
        return when {
            oldItem is RequiredPermissionsListItem.PermissionItem &&
                newItem is RequiredPermissionsListItem.PermissionItem ->
                oldItem.permissionText == newItem.permissionText

            oldItem is RequiredPermissionsListItem.DomainItem &&
                newItem is RequiredPermissionsListItem.DomainItem ->
                oldItem.domain == newItem.domain

            oldItem is RequiredPermissionsListItem.ShowHideDomainAction &&
                newItem is RequiredPermissionsListItem.ShowHideDomainAction ->
                oldItem.isShowAction == newItem.isShowAction

            else -> false
        }
    }

    override fun areContentsTheSame(
        oldItem: RequiredPermissionsListItem,
        newItem: RequiredPermissionsListItem,
    ): Boolean {
        return when {
            oldItem is RequiredPermissionsListItem.PermissionItem &&
                newItem is RequiredPermissionsListItem.PermissionItem ->
                oldItem.permissionText == newItem.permissionText

            oldItem is RequiredPermissionsListItem.DomainItem &&
                newItem is RequiredPermissionsListItem.DomainItem ->
                oldItem.domain == newItem.domain

            oldItem is RequiredPermissionsListItem.ShowHideDomainAction &&
                newItem is RequiredPermissionsListItem.ShowHideDomainAction ->
                oldItem.isShowAction == newItem.isShowAction

            else -> false
        }
    }
}

/**
 * An adapter for displaying optional or required permissions before installing an addon.
 *
 * @property permissions The list of permissions to be displayed as a single row.
 * @property domains The list of domains to be displayed as URL's
 * @property domainsHeaderText The text header above the list of domains to display
 */
class RequiredPermissionsAdapter(
    private val permissions: List<String>,
    private val domains: Set<String>,
    private val domainsHeaderText: String,
) :
    ListAdapter<RequiredPermissionsListItem, ViewHolder>(DiffCallback()) {

    private val displayList = mutableListOf<RequiredPermissionsListItem>()
    private var isDomainSectionExpanded = false

    init {
        buildDisplayList()
        submitList(displayList.toList())
    }

    /**
     * ViewHolder for displaying a Permission list item
     */
    class PermissionViewHolder(itemView: View) : ViewHolder(itemView) {
        private val permissionRequiredTv: TextView =
            itemView.findViewById(R.id.permission_required_item)

        /**
         * bind[RequiredPermissionsListItem.PermissionItem] data to view
         */
        fun bind(item: RequiredPermissionsListItem.PermissionItem) {
            permissionRequiredTv.text = item.permissionText
        }
    }

    /**
     * ViewHolder for displaying a Domain list item
     */
    class DomainViewHolder(itemView: View) : ViewHolder(itemView) {
        private val domainTv: TextView = itemView.findViewById(R.id.permission_domain_item)

        /**
         * bind [RequiredPermissionsListItem.DomainItem] data to view
         */
        fun bind(item: RequiredPermissionsListItem.DomainItem) {
            domainTv.text = item.domain
        }
    }

    /**
     * ViewHolder for displaying a Textview that allows a user to show or hide the full
     * list of domains
     */
    class ShowHideViewHolder(itemView: View) : ViewHolder(itemView) {
        private val showHideTv: TextView = itemView.findViewById(R.id.show_hide_permissions)

        /**
         * bind [RequiredPermissionsListItem.ShowHideDomainAction] data to view
         * @param callback - the callback to show or hide the full list of domains when clicked
         */
        fun bind(action: RequiredPermissionsListItem.ShowHideDomainAction, callback: () -> Unit) {
            this.showHideTv.text =
                if (action.isShowAction) {
                    itemView.resources.getString(
                        R.string.mozac_feature_addons_permissions_show_all_sites,
                    )
                } else {
                    itemView.resources.getString(
                        R.string.mozac_feature_addons_permissions_show_fewer_sites,
                    )
                }
            showHideTv.setOnClickListener {
                callback.invoke()
            }
        }
    }

    override fun getItemViewType(position: Int): Int {
        return when (displayList[position]) {
            is RequiredPermissionsListItem.PermissionItem -> VIEW_HOLDER_TYPE_PERMISSION
            is RequiredPermissionsListItem.DomainItem -> VIEW_HOLDER_TYPE_PERMISSION_DOMAIN
            is RequiredPermissionsListItem.ShowHideDomainAction -> VIEW_HOLDER_TYPE_SHOW_HIDE_SITES
        }
    }

    override fun onCreateViewHolder(viewGroup: ViewGroup, viewType: Int): ViewHolder {
        return when (viewType) {
            VIEW_HOLDER_TYPE_PERMISSION -> PermissionViewHolder(
                LayoutInflater.from(viewGroup.context)
                    .inflate(
                        R.layout.mozac_feature_addons_permissions_required_item,
                        viewGroup,
                        false,
                    ),
            )

            VIEW_HOLDER_TYPE_PERMISSION_DOMAIN -> DomainViewHolder(
                LayoutInflater.from(viewGroup.context)
                    .inflate(
                        R.layout.mozac_feature_addons_permissions_domain_item,
                        viewGroup,
                        false,
                    ),
            )

            VIEW_HOLDER_TYPE_SHOW_HIDE_SITES -> ShowHideViewHolder(
                LayoutInflater.from(viewGroup.context)
                    .inflate(
                        R.layout.mozac_feature_addons_permissions_show_hide_domains_item,
                        viewGroup,
                        false,
                    ),
            )

            else -> throw IllegalArgumentException("Unrecognized viewType for Permissions Adapter")
        }
    }

    override fun onBindViewHolder(viewHolder: ViewHolder, position: Int) {
        when (val item = displayList[position]) {
            is RequiredPermissionsListItem.PermissionItem -> {
                (viewHolder as PermissionViewHolder).bind(
                    item,
                )
            }

            is RequiredPermissionsListItem.DomainItem -> {
                (viewHolder as DomainViewHolder).bind(item)
            }

            is RequiredPermissionsListItem.ShowHideDomainAction -> {
                (viewHolder as ShowHideViewHolder).bind(
                    item,
                    ::toggleDomainSection,
                )
            }
        }
    }

    /**
     * Toggles the domain section to be expanded and show all [domains] or contracted
     * and only show [DOMAINS_CONTRACTED_SUBLIST_SIZE]
     */
    @VisibleForTesting
    internal fun toggleDomainSection() {
        isDomainSectionExpanded = !isDomainSectionExpanded
        buildDisplayList()
        submitList(displayList.toList())
    }

    /**
     * Function used in tests to verify the permissions.
     */
    fun getItemAtPosition(position: Int): RequiredPermissionsListItem {
        return displayList[position]
    }

    /**
     * Item count is based on the [displayList] size which will change if a user
     * expands or shrinks the domain section
     */
    override fun getItemCount(): Int {
        return displayList.size
    }

    /**
     * The [displayList] is cleared and built from the list of [permissions] and [domains] which
     * can be expanded or contracted by a user
     */
    private fun buildDisplayList() {
        displayList.clear()

        if (domains.isNotEmpty()) {
            displayList.add(RequiredPermissionsListItem.PermissionItem(domainsHeaderText))

            if (isDomainSectionExpanded) {
                // Add the Show fewer sites text button then all domains
                displayList.add(
                    RequiredPermissionsListItem.ShowHideDomainAction(
                        isShowAction = false,
                    ),
                )

                domains.forEach {
                    displayList.add(RequiredPermissionsListItem.DomainItem(it))
                }
            } else {
                // Add the sublist of domains and the Show all sites button if necessary
                domains.take(DOMAINS_CONTRACTED_SUBLIST_SIZE).forEach {
                    displayList.add(RequiredPermissionsListItem.DomainItem(it))
                }

                if (domains.size > DOMAINS_CONTRACTED_SUBLIST_SIZE) {
                    displayList.add(
                        RequiredPermissionsListItem.ShowHideDomainAction(
                            isShowAction = true,
                        ),
                    )
                }
            }
        }

        permissions.forEach {
            displayList.add(
                RequiredPermissionsListItem.PermissionItem(it),
            )
        }
    }
}

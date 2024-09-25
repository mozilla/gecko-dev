/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.MenuItem
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.home.fake.FakeHomepagePreview
import org.mozilla.fenix.home.sessioncontrol.CollectionInteractor
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * List of expandable collections.
 *
 * @param collections List of [TabCollection] to display.
 * @param showAddTabToCollection Whether to show the "Add tab" menu item in the collections menu.
 * @param expandedCollections List of ids corresponding to [TabCollection]s which are currently expanded.
 * @param interactor Interactor for interactions with the UI.
 */
@Composable
fun Collections(
    collections: List<TabCollection>,
    showAddTabToCollection: Boolean,
    expandedCollections: Set<Long> = emptySet(),
    interactor: CollectionInteractor,
) {
    Column {
        for (collection in collections) {
            Spacer(Modifier.height(12.dp))

            Collection(
                collection = collection,
                expanded = expandedCollections.contains(collection.id),
                menuItems = getMenuItems(
                    collection = collection,
                    showAddTabs = showAddTabToCollection,
                    onOpenTabsTapped = interactor::onCollectionOpenTabsTapped,
                    onRenameCollectionTapped = interactor::onRenameCollectionTapped,
                    onAddTabTapped = interactor::onCollectionAddTabTapped,
                    onDeleteCollectionTapped = interactor::onDeleteCollectionTapped,
                ),
                onToggleCollectionExpanded = interactor::onToggleCollectionExpanded,
                onCollectionShareTabsClicked = interactor::onCollectionShareTabsClicked,
            )

            if (expandedCollections.contains(collection.id)) {
                val lastId = collection.tabs.last().id

                for (tab in collection.tabs) {
                    CollectionItem(
                        tab = tab,
                        isLastInCollection = tab.id == lastId,
                        onClick = { interactor.onCollectionOpenTabClicked(tab) },
                        onRemove = {
                            interactor.onCollectionRemoveTab(
                                collection = collection,
                                tab = tab,
                            )
                        },
                    )
                }
            }
        }
    }
}

/**
 * Constructs and returns the default list of menu options for a [TabCollection].
 *
 * @param collection [TabCollection] for which the menu will be shown.
 * Might serve as an argument for the callbacks for when the user interacts with certain menu options.
 * @param showAddTabs Whether to show the option to add a currently open tab to the [collection].
 * @param onOpenTabsTapped Invoked when the user chooses to open the tabs from [collection].
 * @param onRenameCollectionTapped Invoked when the user chooses to rename the [collection].
 * @param onAddTabTapped Invoked when the user chooses to add tabs to [collection].
 * @param onDeleteCollectionTapped Invoked when the user chooses to delete [collection].
 */
@Composable
private fun getMenuItems(
    collection: TabCollection,
    showAddTabs: Boolean,
    onOpenTabsTapped: (TabCollection) -> Unit,
    onRenameCollectionTapped: (TabCollection) -> Unit,
    onAddTabTapped: (TabCollection) -> Unit,
    onDeleteCollectionTapped: (TabCollection) -> Unit,
): List<MenuItem> {
    return listOfNotNull(
        MenuItem(
            title = stringResource(R.string.collection_open_tabs),
            color = FirefoxTheme.colors.textPrimary,
        ) {
            onOpenTabsTapped(collection)
        },
        MenuItem(
            title = stringResource(R.string.collection_rename),
            color = FirefoxTheme.colors.textPrimary,
        ) {
            onRenameCollectionTapped(collection)
        },

        if (showAddTabs) {
            MenuItem(
                title = stringResource(R.string.add_tab),
                color = FirefoxTheme.colors.textPrimary,
            ) {
                onAddTabTapped(collection)
            }
        } else {
            null
        },

        MenuItem(
            title = stringResource(R.string.collection_delete),
            color = FirefoxTheme.colors.textCritical,
        ) {
            onDeleteCollectionTapped(collection)
        },
    )
}

@LightDarkPreview
@Composable
private fun CollectionsPreview() {
    val expandedCollections: MutableState<Set<Long>> = remember { mutableStateOf(setOf(1L)) }
    FirefoxTheme {
        Collections(
            collections = listOf(FakeHomepagePreview.collection(tabs = listOf(FakeHomepagePreview.tab()))),
            showAddTabToCollection = true,
            expandedCollections = expandedCollections.value,
            interactor = object : CollectionInteractor {
                override fun onCollectionAddTabTapped(collection: TabCollection) { /* no op */ }

                override fun onCollectionOpenTabClicked(tab: Tab) { /* no op */ }

                override fun onCollectionOpenTabsTapped(collection: TabCollection) { /* no op */ }

                override fun onCollectionRemoveTab(
                    collection: TabCollection,
                    tab: Tab,
                ) { /* no op */ }

                override fun onCollectionShareTabsClicked(collection: TabCollection) { /* no op */ }

                override fun onDeleteCollectionTapped(collection: TabCollection) { /* no op */ }

                override fun onRenameCollectionTapped(collection: TabCollection) { /* no op */ }

                override fun onToggleCollectionExpanded(
                    collection: TabCollection,
                    expand: Boolean,
                ) {
                    expandedCollections.value = if (expand) {
                        setOf(1L)
                    } else {
                        setOf()
                    }
                }

                override fun onAddTabsToCollectionTapped() { /* no op */ }

                override fun onRemoveCollectionsPlaceholder() { /* no op */ }
            },
        )
    }
}

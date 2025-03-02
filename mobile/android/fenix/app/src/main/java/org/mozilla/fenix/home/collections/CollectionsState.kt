/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.collections

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.WallpaperState

/**
 * State object encapsulating the UI state of the collections section of the homepage.
 */
sealed class CollectionsState {

    /**
     * State in which a user has collections, and they should be displayed in the UI.
     *
     * @property collections List of [TabCollection] to display.
     * @property expandedCollections List of ids corresponding to [TabCollection]s which are currently expanded.
     * @property showSaveTabsToCollection Whether to show the "Save tabs to collection" menu item in the collections
     * menu.
     */
    data class Content(
        val collections: List<TabCollection>,
        val expandedCollections: Set<Long>,
        val showSaveTabsToCollection: Boolean,
    ) : CollectionsState()

    /**
     * State in which the placeholder should be displayed.
     *
     * @property showSaveTabsToCollection Whether to show the "Save tabs to collection" menu item in the collections
     * menu.
     * @property colors [CollectionColors] to use for collections UI.
     */
    data class Placeholder(
        val showSaveTabsToCollection: Boolean,
        val colors: CollectionColors,
    ) : CollectionsState()

    /**
     * State in which no collections section should be displayed.
     */
    data object Gone : CollectionsState()

    companion object {
        @Composable
        internal fun build(
            appState: AppState,
            browserState: BrowserState,
            browsingModeManager: BrowsingModeManager,
        ): CollectionsState =
            with(appState) {
                when {
                    collections.isNotEmpty() -> Content(
                        collections = collections,
                        expandedCollections = expandedCollections,
                        showSaveTabsToCollection = browserState.normalTabs.isNotEmpty(),
                    )

                    showCollectionPlaceholder -> {
                        val tabCount = if (browsingModeManager.mode.isPrivate) {
                            browserState.privateTabs.size
                        } else {
                            browserState.normalTabs.size
                        }

                        Placeholder(
                            showSaveTabsToCollection = tabCount > 0,
                            colors = CollectionColors.colors(wallpaperState),
                        )
                    }

                    else -> Gone
                }
            }
    }
}

/**
 * Represents the colors used by collections.
 *
 * @property buttonBackgroundColor [Color] to display for the button background.
 * @property buttonTextColor [Color] to display for the button text color.
 * @property titleTextColor [Color] to display for the title text color.
 * @property descriptionTextColor [Color] to display for the description text color.
 */
data class CollectionColors(
    val buttonBackgroundColor: Color,
    val buttonTextColor: Color,
    val titleTextColor: Color,
    val descriptionTextColor: Color,
) {
    /**
     * Companion object for [CollectionColors]
     */
    companion object {
        /**
         * Builder function used to construct an instance of [CollectionColors].
         */
        @Composable
        fun colors(
            buttonBackgroundColor: Color = FirefoxTheme.colors.actionPrimary,
            buttonTextColor: Color = FirefoxTheme.colors.iconActionPrimary,
            titleTextColor: Color = FirefoxTheme.colors.textPrimary,
            descriptionTextColor: Color = FirefoxTheme.colors.textSecondary,
        ) = CollectionColors(
            buttonBackgroundColor = buttonBackgroundColor,
            buttonTextColor = buttonTextColor,
            titleTextColor = titleTextColor,
            descriptionTextColor = descriptionTextColor,
        )

        /**
         * Builder function used to construct an instance of [CollectionColors] given a
         * [WallpaperState].
         */
        @Composable
        fun colors(wallpaperState: WallpaperState): CollectionColors {
            val textColor = wallpaperState.currentWallpaper.textColor
            val titleTextColor: Color
            val descriptionTextColor: Color
            if (textColor == null) {
                titleTextColor = FirefoxTheme.colors.textPrimary
                descriptionTextColor = FirefoxTheme.colors.textSecondary
            } else {
                val color = Color(textColor)
                titleTextColor = color
                descriptionTextColor = color
            }

            var buttonColor = FirefoxTheme.colors.actionPrimary
            var buttonTextColor = FirefoxTheme.colors.textActionPrimary

            wallpaperState.composeRunIfWallpaperCardColorsAreAvailable { _, _ ->
                buttonColor = FirefoxTheme.colors.layer1

                if (!isSystemInDarkTheme()) {
                    buttonTextColor = FirefoxTheme.colors.textActionSecondary
                }
            }

            return CollectionColors(
                buttonBackgroundColor = buttonColor,
                buttonTextColor = buttonTextColor,
                titleTextColor = titleTextColor,
                descriptionTextColor = descriptionTextColor,
            )
        }
    }
}

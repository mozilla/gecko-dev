/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import mozilla.components.service.pocket.PocketStory
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.compose.SelectableChipColors
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.Settings

/**
 * State object that describes the pocket section of the homepage.
 *
 * @property stories List of [PocketStory] to display.
 * @property categories List of [PocketRecommendedStoriesCategory] to display.
 * @property categoriesSelections List of selectable [PocketRecommendedStoriesSelectedCategory] to display.
 * @property showContentRecommendations Whether or not to show Merino content recommendations.
 * @property categoryColors Color parameters for the selectable categories.
 * @property textColor [Color] for text.
 * @property linkTextColor [Color] for link text.
 */
data class PocketState(
    val stories: List<PocketStory>,
    val categories: List<PocketRecommendedStoriesCategory>,
    val categoriesSelections: List<PocketRecommendedStoriesSelectedCategory>,
    val showContentRecommendations: Boolean,
    val categoryColors: SelectableChipColors,
    val textColor: Color,
    val linkTextColor: Color,
) {

    /**
     * Companion object for building [PocketState].
     */
    companion object {

        /**
         * Builds a new [PocketState] from the current [AppState].
         *
         * @param appState State to build the [PocketState] from.
         * @param settings [Settings] corresponding to how the homepage should be displayed.
         */
        @Composable
        internal fun build(appState: AppState, settings: Settings) = with(appState) {
            var textColor = FirefoxTheme.colors.textPrimary
            var linkTextColor = FirefoxTheme.colors.textAccent

            wallpaperState.currentWallpaper.let { currentWallpaper ->
                currentWallpaper.textColor?.let {
                    val wallpaperAdaptedTextColor = Color(it)
                    textColor = wallpaperAdaptedTextColor
                    linkTextColor = wallpaperAdaptedTextColor
                }
            }

            PocketState(
                stories = recommendationState.pocketStories,
                categories = recommendationState.pocketStoriesCategories,
                categoriesSelections = recommendationState.pocketStoriesCategoriesSelections,
                showContentRecommendations = settings.showContentRecommendations,
                categoryColors = getSelectableChipColors(),
                textColor = textColor,
                linkTextColor = linkTextColor,
            )
        }
    }
}

@Composable
private fun AppState.getSelectableChipColors(): SelectableChipColors {
    var (selectedBackgroundColor, unselectedBackgroundColor, selectedTextColor, unselectedTextColor) =
        SelectableChipColors.buildColors()

    wallpaperState.composeRunIfWallpaperCardColorsAreAvailable { cardColorLight, cardColorDark ->
        if (isSystemInDarkTheme()) {
            selectedBackgroundColor = cardColorDark
            unselectedBackgroundColor = cardColorLight
            selectedTextColor = FirefoxTheme.colors.textActionPrimary
            unselectedTextColor = FirefoxTheme.colors.textActionSecondary
        } else {
            selectedBackgroundColor = cardColorLight
            unselectedBackgroundColor = cardColorDark
            selectedTextColor = FirefoxTheme.colors.textActionSecondary
            unselectedTextColor = FirefoxTheme.colors.textActionPrimary
        }
    }

    return SelectableChipColors(
        selectedTextColor = selectedTextColor,
        unselectedTextColor = unselectedTextColor,
        selectedBackgroundColor = selectedBackgroundColor,
        unselectedBackgroundColor = unselectedBackgroundColor,
    )
}

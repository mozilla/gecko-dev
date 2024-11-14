/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.recommendations

import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.ext.recordNewImpression
import org.mozilla.fenix.components.appstate.AppAction.ContentRecommendationsAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.getFilteredStories
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory

/**
 * A [ContentRecommendationsAction] reducer for updating the content recommendations state in
 * [AppState].
 */
internal object ContentRecommendationsReducer {

    /**
     * Reduces the given [ContentRecommendationsAction] into a new [AppState].
     */
    @Suppress("LongMethod")
    fun reduce(state: AppState, action: ContentRecommendationsAction): AppState {
        return when (action) {
            is ContentRecommendationsAction.SelectPocketStoriesCategory -> {
                val updatedCategoriesState = state.copy(
                    pocketStoriesCategoriesSelections =
                    state.pocketStoriesCategoriesSelections + PocketRecommendedStoriesSelectedCategory(
                        name = action.categoryName,
                    ),
                )

                // Selecting a category means the stories to be displayed needs to also be changed.
                updatedCategoriesState.copy(
                    pocketStories = updatedCategoriesState.getFilteredStories(),
                )
            }

            is ContentRecommendationsAction.DeselectPocketStoriesCategory -> {
                val updatedCategoriesState = state.copy(
                    pocketStoriesCategoriesSelections = state.pocketStoriesCategoriesSelections.filterNot {
                        it.name == action.categoryName
                    },
                )

                // Deselecting a category means the stories to be displayed needs to also be changed.
                updatedCategoriesState.copy(
                    pocketStories = updatedCategoriesState.getFilteredStories(),
                )
            }

            is ContentRecommendationsAction.PocketStoriesCategoriesChange -> {
                val updatedCategoriesState =
                    state.copy(pocketStoriesCategories = action.storiesCategories)
                // Whenever categories change stories to be displayed needs to also be changed.
                updatedCategoriesState.copy(
                    pocketStories = updatedCategoriesState.getFilteredStories(),
                )
            }

            is ContentRecommendationsAction.PocketStoriesCategoriesSelectionsChange -> {
                val updatedCategoriesState = state.copy(
                    pocketStoriesCategories = action.storiesCategories,
                    pocketStoriesCategoriesSelections = action.categoriesSelected,
                )
                // Whenever categories change stories to be displayed needs to also be changed.
                updatedCategoriesState.copy(
                    pocketStories = updatedCategoriesState.getFilteredStories(),
                )
            }

            is ContentRecommendationsAction.PocketStoriesClean -> state.copy(
                pocketStoriesCategories = emptyList(),
                pocketStoriesCategoriesSelections = emptyList(),
                pocketStories = emptyList(),
                pocketSponsoredStories = emptyList(),
            )

            is ContentRecommendationsAction.PocketSponsoredStoriesChange -> {
                val updatedStoriesState = state.copy(
                    pocketSponsoredStories = action.sponsoredStories,
                )

                updatedStoriesState.copy(
                    pocketStories = updatedStoriesState.getFilteredStories(),
                )
            }

            is ContentRecommendationsAction.PocketStoriesShown -> {
                var updatedCategories = state.pocketStoriesCategories
                action.storiesShown.filterIsInstance<PocketRecommendedStory>()
                    .forEach { shownStory ->
                        updatedCategories = updatedCategories.map { category ->
                            when (category.name == shownStory.category) {
                                true -> {
                                    category.copy(
                                        stories = category.stories.map { story ->
                                            when (story.title == shownStory.title) {
                                                true -> story.copy(timesShown = story.timesShown.inc())
                                                false -> story
                                            }
                                        },
                                    )
                                }

                                false -> category
                            }
                        }
                    }

                var updatedSponsoredStories = state.pocketSponsoredStories
                action.storiesShown.filterIsInstance<PocketSponsoredStory>().forEach { shownStory ->
                    updatedSponsoredStories = updatedSponsoredStories.map { story ->
                        when (story.id == shownStory.id) {
                            true -> story.recordNewImpression()
                            false -> story
                        }
                    }
                }

                state.copy(
                    pocketStoriesCategories = updatedCategories,
                    pocketSponsoredStories = updatedSponsoredStories,
                )
            }
        }
    }
}

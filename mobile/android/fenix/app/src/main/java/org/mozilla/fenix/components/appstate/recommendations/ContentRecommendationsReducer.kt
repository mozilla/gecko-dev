/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.recommendations

import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketStory.ContentRecommendation
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.ext.recordNewImpression
import org.mozilla.fenix.components.appstate.AppAction.ContentRecommendationsAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.getFilteredStories
import org.mozilla.fenix.ext.getStories
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
            is ContentRecommendationsAction.ContentRecommendationsFetched -> {
                val updatedRecommendationsState = state.copyWithRecommendationsState {
                    it.copy(
                        contentRecommendations = action.recommendations,
                    )
                }

                updatedRecommendationsState.copyWithRecommendationsState {
                    it.copy(
                        pocketStories = updatedRecommendationsState.getStories(),
                    )
                }
            }

            is ContentRecommendationsAction.SelectPocketStoriesCategory -> {
                val updatedCategoriesState =
                    state.copyWithRecommendationsState {
                        it.copy(
                            pocketStoriesCategoriesSelections =
                            it.pocketStoriesCategoriesSelections + PocketRecommendedStoriesSelectedCategory(
                                name = action.categoryName,
                            ),
                        )
                    }

                // Selecting a category means the stories to be displayed needs to also be changed.
                updatedCategoriesState.copyWithRecommendationsState {
                    it.copy(pocketStories = updatedCategoriesState.getFilteredStories())
                }
            }

            is ContentRecommendationsAction.DeselectPocketStoriesCategory -> {
                val updatedCategoriesState = state.copyWithRecommendationsState {
                    it.copy(
                        pocketStoriesCategoriesSelections =
                        it.pocketStoriesCategoriesSelections.filterNot { category ->
                            category.name == action.categoryName
                        },
                    )
                }

                // Deselecting a category means the stories to be displayed needs to also be changed.
                updatedCategoriesState.copyWithRecommendationsState {
                    it.copy(pocketStories = updatedCategoriesState.getFilteredStories())
                }
            }

            is ContentRecommendationsAction.PocketStoriesCategoriesChange -> {
                val updatedCategoriesState = state.copyWithRecommendationsState {
                    it.copy(pocketStoriesCategories = action.storiesCategories)
                }

                // Whenever categories change stories to be displayed needs to also be changed.
                updatedCategoriesState.copyWithRecommendationsState {
                    it.copy(pocketStories = updatedCategoriesState.getFilteredStories())
                }
            }

            is ContentRecommendationsAction.PocketStoriesCategoriesSelectionsChange -> {
                val updatedCategoriesState = state.copyWithRecommendationsState {
                    it.copy(
                        pocketStoriesCategories = action.storiesCategories,
                        pocketStoriesCategoriesSelections = action.categoriesSelected,
                    )
                }

                // Whenever categories change stories to be displayed needs to also be changed.
                updatedCategoriesState.copyWithRecommendationsState {
                    it.copy(pocketStories = updatedCategoriesState.getFilteredStories())
                }
            }

            is ContentRecommendationsAction.PocketStoriesClean -> state.copyWithRecommendationsState {
                it.copy(
                    pocketStoriesCategories = emptyList(),
                    pocketStoriesCategoriesSelections = emptyList(),
                    pocketStories = emptyList(),
                    pocketSponsoredStories = emptyList(),
                    contentRecommendations = emptyList(),
                )
            }

            is ContentRecommendationsAction.PocketSponsoredStoriesChange -> {
                val updatedStoriesState = state.copyWithRecommendationsState {
                    it.copy(
                        pocketSponsoredStories = action.sponsoredStories,
                    )
                }

                updatedStoriesState.copyWithRecommendationsState {
                    it.copy(
                        pocketStories = updatedStoriesState.getFilteredStories(),
                    )
                }
            }

            is ContentRecommendationsAction.PocketStoriesShown -> {
                var updatedCategories = state.recommendationState.pocketStoriesCategories

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

                var updatedSponsoredStories = state.recommendationState.pocketSponsoredStories
                action.storiesShown.filterIsInstance<PocketSponsoredStory>().forEach { shownStory ->
                    updatedSponsoredStories = updatedSponsoredStories.map { story ->
                        when (story.id == shownStory.id) {
                            true -> story.recordNewImpression()
                            false -> story
                        }
                    }
                }

                state.copyWithRecommendationsState {
                    it.copy(
                        pocketStoriesCategories = updatedCategories,
                        pocketSponsoredStories = updatedSponsoredStories,
                    )
                }
            }
        }
    }
}

@VisibleForTesting
internal inline fun AppState.copyWithRecommendationsState(
    crossinline update: (ContentRecommendationsState) -> ContentRecommendationsState,
): AppState {
    return this.copy(recommendationState = update(recommendationState))
}

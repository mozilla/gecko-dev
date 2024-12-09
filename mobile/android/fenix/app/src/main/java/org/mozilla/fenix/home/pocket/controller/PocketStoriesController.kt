/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket.controller

import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.ContentRecommendation
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.ext.getCurrentFlightImpressions
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.GleanMetrics.Pocket
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.ContentRecommendationsAction
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.view.POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT
import org.mozilla.fenix.utils.Settings

/**
 * Contract for how all user interactions with the Pocket stories feature are to be handled.
 */
interface PocketStoriesController {
    /**
     * Callback to decide what should happen as an effect of a specific story being shown.
     *
     * @param storyShown The just shown [PocketStory].
     * @param storyPosition `row x column` matrix representing the grid position of the shown story.
     */
    fun handleStoryShown(storyShown: PocketStory, storyPosition: Pair<Int, Int>)

    /**
     * Callback to decide what should happen as an effect of a new list of stories being shown.
     *
     * @param storiesShown the new list of [PocketStory]es shown to the user.
     */
    fun handleStoriesShown(storiesShown: List<PocketStory>)

    /**
     * Callback allowing to handle a specific [PocketRecommendedStoriesCategory] being clicked by the user.
     *
     * @param categoryClicked the just clicked [PocketRecommendedStoriesCategory].
     */
    fun handleCategoryClick(categoryClicked: PocketRecommendedStoriesCategory)

    /**
     * Callback for when the user clicks on a specific story.
     *
     * @param storyClicked The just clicked [PocketStory].
     * @param storyPosition `row x column` matrix representing the grid position of the clicked story.
     */
    fun handleStoryClicked(storyClicked: PocketStory, storyPosition: Pair<Int, Int>)

    /**
     * Callback for when the "Learn more" link is clicked.
     *
     * @param link URL clicked.
     */
    fun handleLearnMoreClicked(link: String)

    /**
     * Callback for when the "Discover more" link is clicked.
     *
     * @param link URL clicked.
     */
    fun handleDiscoverMoreClicked(link: String)
}

/**
 * Default behavior for handling all user interactions with the Pocket recommended stories feature.
 *
 * @param homeActivity [HomeActivity] used to open URLs in a new tab.
 * @param appStore [AppStore] from which to read the current Pocket recommendations and dispatch new actions on.
 * @param settings [Settings] used to check the application shared preferences.
 */
internal class DefaultPocketStoriesController(
    private val homeActivity: HomeActivity,
    private val appStore: AppStore,
    private val settings: Settings,
) : PocketStoriesController {
    override fun handleStoryShown(
        storyShown: PocketStory,
        storyPosition: Pair<Int, Int>,
    ) {
        appStore.dispatch(ContentRecommendationsAction.PocketStoriesShown(listOf(storyShown)))

        when (storyShown) {
            is PocketSponsoredStory -> {
                Pocket.homeRecsSpocShown.record(
                    Pocket.HomeRecsSpocShownExtra(
                        spocId = storyShown.id.toString(),
                        position = "${storyPosition.first}x${storyPosition.second}",
                        timesShown = storyShown.getCurrentFlightImpressions().size.inc().toString(),
                    ),
                )
                Pocket.spocShim.set(storyShown.shim.impression)
                Pings.spoc.submit(Pings.spocReasonCodes.impression)
            }
            else -> {
                // no-op
                // The telemetry for PocketRecommendedStory is sent separately for bulk updates.
            }
        }
    }

    override fun handleStoriesShown(storiesShown: List<PocketStory>) {
        // Only report here the impressions for recommended stories.
        // Sponsored stories use a different API for more accurate tracking.
        appStore.dispatch(
            ContentRecommendationsAction.PocketStoriesShown(
                storiesShown = storiesShown.filter { it is ContentRecommendation || it is PocketRecommendedStory },
            ),
        )

        Pocket.homeRecsShown.record(NoExtras())
    }

    override fun handleCategoryClick(categoryClicked: PocketRecommendedStoriesCategory) {
        val initialCategoriesSelections =
            appStore.state.recommendationState.pocketStoriesCategoriesSelections

        // First check whether the category is clicked to be deselected.
        if (initialCategoriesSelections.map { it.name }.contains(categoryClicked.name)) {
            appStore.dispatch(ContentRecommendationsAction.DeselectPocketStoriesCategory(categoryClicked.name))
            Pocket.homeRecsCategoryClicked.record(
                Pocket.HomeRecsCategoryClickedExtra(
                    categoryName = categoryClicked.name,
                    newState = "deselected",
                    selectedTotal = initialCategoriesSelections.size.toString(),
                ),
            )
            return
        }

        // If a new category is clicked to be selected:
        // Ensure the number of categories selected at a time is capped.
        val oldestCategoryToDeselect =
            if (initialCategoriesSelections.size == POCKET_CATEGORIES_SELECTED_AT_A_TIME_COUNT) {
                initialCategoriesSelections.minByOrNull { it.selectionTimestamp }
            } else {
                null
            }
        oldestCategoryToDeselect?.let {
            appStore.dispatch(ContentRecommendationsAction.DeselectPocketStoriesCategory(it.name))
        }

        // Finally update the selection.
        appStore.dispatch(ContentRecommendationsAction.SelectPocketStoriesCategory(categoryClicked.name))

        Pocket.homeRecsCategoryClicked.record(
            Pocket.HomeRecsCategoryClickedExtra(
                categoryName = categoryClicked.name,
                newState = "selected",
                selectedTotal = initialCategoriesSelections.size.toString(),
            ),
        )
    }

    override fun handleStoryClicked(
        storyClicked: PocketStory,
        storyPosition: Pair<Int, Int>,
    ) {
        val newTab = homeActivity.settings().enableHomepageAsNewTab.not()
        homeActivity.openToBrowserAndLoad(
            searchTermOrURL = storyClicked.url,
            newTab = newTab,
            from = BrowserDirection.FromHome,
        )

        when (storyClicked) {
            is PocketRecommendedStory -> {
                Pocket.homeRecsStoryClicked.record(
                    Pocket.HomeRecsStoryClickedExtra(
                        position = "${storyPosition.first}x${storyPosition.second}",
                        timesShown = storyClicked.timesShown.inc().toString(),
                    ),
                )
            }
            is PocketSponsoredStory -> {
                Pocket.homeRecsSpocClicked.record(
                    Pocket.HomeRecsSpocClickedExtra(
                        spocId = storyClicked.id.toString(),
                        position = "${storyPosition.first}x${storyPosition.second}",
                        timesShown = storyClicked.getCurrentFlightImpressions().size.inc().toString(),
                    ),
                )
                Pocket.spocShim.set(storyClicked.shim.click)
                Pings.spoc.submit(Pings.spocReasonCodes.click)
            }

            is PocketStory.ContentRecommendation -> {
                // no-op
            }
        }
    }

    override fun handleLearnMoreClicked(link: String) {
        homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        Pocket.homeRecsLearnMoreClicked.record(NoExtras())
    }

    override fun handleDiscoverMoreClicked(link: String) {
        homeActivity.openToBrowserAndLoad(link, true, BrowserDirection.FromHome)
        Pocket.homeRecsDiscoverClicked.record(NoExtras())
    }
}

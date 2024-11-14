/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.recommendations

import mozilla.components.service.pocket.PocketStory
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesCategory
import org.mozilla.fenix.home.pocket.PocketRecommendedStoriesSelectedCategory

/**
 * The state of the content recommendations to display.
 *
 * @property pocketStories The list of currently shown [PocketRecommendedStory]s.
 * @property pocketStoriesCategories All [PocketRecommendedStory] categories.
 * @property pocketStoriesCategoriesSelections Current Pocket recommended stories categories selected by the user.
 * @property pocketSponsoredStories All [PocketSponsoredStory]s.
 */
data class ContentRecommendationsState(
    val pocketStories: List<PocketStory> = emptyList(),
    val pocketStoriesCategories: List<PocketRecommendedStoriesCategory> = emptyList(),
    val pocketStoriesCategoriesSelections: List<PocketRecommendedStoriesSelectedCategory> = emptyList(),
    val pocketSponsoredStories: List<PocketSponsoredStory> = emptyList(),
)

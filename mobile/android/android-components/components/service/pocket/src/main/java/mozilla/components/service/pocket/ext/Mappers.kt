/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.ext

import androidx.annotation.VisibleForTesting
import mozilla.components.service.pocket.PocketStory.ContentRecommendation
import mozilla.components.service.pocket.PocketStory.PocketRecommendedStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryShim
import mozilla.components.service.pocket.PocketStory.SponsoredContent
import mozilla.components.service.pocket.PocketStory.SponsoredContentCallbacks
import mozilla.components.service.pocket.PocketStory.SponsoredContentFrequencyCaps
import mozilla.components.service.pocket.mars.api.MarsSpocsResponseItem
import mozilla.components.service.pocket.mars.db.SponsoredContentEntity
import mozilla.components.service.pocket.recommendations.api.ContentRecommendationResponseItem
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationEntity
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationImpression
import mozilla.components.service.pocket.spocs.api.ApiSpoc
import mozilla.components.service.pocket.spocs.db.SpocEntity
import mozilla.components.service.pocket.stories.api.PocketApiStory
import mozilla.components.service.pocket.stories.db.PocketLocalStoryTimesShown
import mozilla.components.service.pocket.stories.db.PocketStoryEntity

@VisibleForTesting
internal const val DEFAULT_CATEGORY = "general"

@VisibleForTesting
internal const val DEFAULT_TIMES_SHOWN = 0L

@VisibleForTesting
internal const val DEFAULT_FLIGHT_CAP_PERIOD_IN_SECONDS = 24 * 60 * 60 // 1 Day

/**
 * Map Pocket API objects to the object type that we persist locally.
 */
internal fun PocketApiStory.toPocketLocalStory(): PocketStoryEntity =
    PocketStoryEntity(
        url,
        title,
        imageUrl,
        publisher,
        category,
        timeToRead,
        DEFAULT_TIMES_SHOWN,
    )

/**
 * Map Room entities to the object type that we expose to service clients.
 */
internal fun PocketStoryEntity.toPocketRecommendedStory(): PocketRecommendedStory =
    PocketRecommendedStory(
        url = url,
        title = title,
        imageUrl = imageUrl,
        publisher = publisher,
        category = if (category.isNotBlank()) category else DEFAULT_CATEGORY,
        timeToRead = timeToRead,
        timesShown = timesShown,
    )

/**
 * Maps an object of the type exposed to clients to one that can partially update only the "timesShown"
 * property of the type we persist locally.
 */
internal fun PocketRecommendedStory.toPartialTimeShownUpdate(): PocketLocalStoryTimesShown =
    PocketLocalStoryTimesShown(url, timesShown)

/**
 * Map sponsored Pocket stories to the object type that we persist locally.
 */
internal fun ApiSpoc.toLocalSpoc(): SpocEntity =
    SpocEntity(
        id = id,
        url = url,
        title = title,
        imageUrl = imageSrc,
        sponsor = sponsor,
        clickShim = shim.click,
        impressionShim = shim.impression,
        priority = priority,
        lifetimeCapCount = caps.lifetimeCount,
        flightCapCount = caps.flightCount,
        flightCapPeriod = caps.flightPeriod,
    )

/**
 * Map Room entities to the object type that we expose to service clients.
 */
internal fun SpocEntity.toPocketSponsoredStory(
    impressions: List<Long> = emptyList(),
) = PocketSponsoredStory(
    id = id,
    title = title,
    url = url,
    imageUrl = imageUrl,
    sponsor = sponsor,
    shim = PocketSponsoredStoryShim(
        click = clickShim,
        impression = impressionShim,
    ),
    priority = priority,
    caps = PocketSponsoredStoryCaps(
        currentImpressions = impressions,
        lifetimeCount = lifetimeCapCount,
        flightCount = flightCapCount,
        flightPeriod = flightCapPeriod,
    ),
)

/**
 * Maps the sponsored content Room entities to the object type we expose to service clients.
 */
internal fun SponsoredContentEntity.toSponsoredContent(
    impressions: List<Long> = emptyList(),
) = SponsoredContent(
    url = url,
    title = title,
    callbacks = SponsoredContentCallbacks(
        clickUrl = clickUrl,
        impressionUrl = impressionUrl,
    ),
    imageUrl = imageUrl,
    domain = domain,
    excerpt = excerpt,
    sponsor = sponsor,
    blockKey = blockKey,
    caps = SponsoredContentFrequencyCaps(
        currentImpressions = impressions,
        flightCount = flightCapCount,
        flightPeriod = flightCapPeriod,
    ),
    priority = priority,
)

/**
 * Maps the sponsored content response item to the object type that is persisted locally.
 */
internal fun MarsSpocsResponseItem.toSponsoredContentEntity() =
    SponsoredContentEntity(
        url = url,
        title = title,
        clickUrl = callbacks.clickUrl,
        impressionUrl = callbacks.impressionUrl,
        imageUrl = imageUrl,
        domain = domain,
        excerpt = excerpt,
        sponsor = sponsor,
        blockKey = blockKey,
        flightCapCount = caps.day,
        flightCapPeriod = DEFAULT_FLIGHT_CAP_PERIOD_IN_SECONDS,
        priority = ranking.priority,
    )

/**
 * Maps the Room entities to the object type that we expose to service clients.
 */
internal fun ContentRecommendationEntity.toContentRecommendation() =
    ContentRecommendation(
        corpusItemId = corpusItemId,
        scheduledCorpusItemId = scheduledCorpusItemId,
        url = url,
        title = title,
        excerpt = excerpt,
        topic = topic,
        publisher = publisher,
        isTimeSensitive = isTimeSensitive,
        imageUrl = imageUrl,
        tileId = tileId,
        receivedRank = receivedRank,
        recommendedAt = recommendedAt,
        impressions = impressions,
    )

/**
 * Maps the content recommendation response item to the object type that is persisted locally.
 *
 * @param recommendedAt A timestamp indicating when the content recommendations was recommended.
 */
internal fun ContentRecommendationResponseItem.toContentRecommendationEntity(recommendedAt: Long) =
    ContentRecommendationEntity(
        corpusItemId = corpusItemId,
        scheduledCorpusItemId = scheduledCorpusItemId,
        url = url,
        title = title,
        excerpt = excerpt,
        topic = topic,
        publisher = publisher,
        isTimeSensitive = isTimeSensitive,
        imageUrl = imageUrl,
        tileId = tileId,
        receivedRank = receivedRank,
        recommendedAt = recommendedAt,
        impressions = DEFAULT_TIMES_SHOWN,
    )

/**
 * Maps the content recommendation client object to an object that can facilitate updating the
 * [ContentRecommendation.impressions] property that is persisted locally.
 */
internal fun ContentRecommendation.toImpressions() =
    ContentRecommendationImpression(
        corpusItemId = corpusItemId,
        impressions = impressions,
    )

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.ext

import mozilla.components.service.pocket.PocketStory.PocketSponsoredStory
import mozilla.components.service.pocket.PocketStory.PocketSponsoredStoryCaps
import mozilla.components.service.pocket.PocketStory.SponsoredContent
import mozilla.components.service.pocket.PocketStory.SponsoredContentFrequencyCaps
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.doReturn

class PocketStoryKtTest {
    private val nowInSeconds = System.currentTimeMillis() / 1000
    private val flightPeriod = 100
    private val flightImpression1 = nowInSeconds - flightPeriod / 2
    private val flightImpression2 = nowInSeconds - flightPeriod / 3
    private val currentImpressions = listOf(
        nowInSeconds - flightPeriod * 2, // older impression that doesn't fit the flight period
        flightImpression1,
        flightImpression2,
    )

    @Test
    fun `GIVEN sponsored story impressions recorded WHEN asking for the current flight impression THEN return all impressions in flight period`() {
        val storyCaps = PocketSponsoredStoryCaps(
            currentImpressions = currentImpressions,
            lifetimeCount = 10,
            flightCount = 5,
            flightPeriod = flightPeriod,
        )
        val story: PocketSponsoredStory = mock()
        doReturn(storyCaps).`when`(story).caps

        val result = story.getCurrentFlightImpressions()

        assertEquals(listOf(flightImpression1, flightImpression2), result)
    }

    @Test
    fun `GIVEN sponsored story impressions recorded WHEN asking if lifetime impressions reached THEN return false if not`() {
        val storyCaps = PocketSponsoredStoryCaps(
            currentImpressions = currentImpressions,
            lifetimeCount = 10,
            flightCount = 5,
            flightPeriod = flightPeriod,
        )
        val story: PocketSponsoredStory = mock()
        doReturn(storyCaps).`when`(story).caps

        val result = story.hasLifetimeImpressionsLimitReached()

        assertFalse(result)
    }

    @Test
    fun `GIVEN sponsored story impressions recorded WHEN asking if lifetime impressions reached THEN return true if so`() {
        val storyCaps = PocketSponsoredStoryCaps(
            currentImpressions = currentImpressions,
            lifetimeCount = 3,
            flightCount = 3,
            flightPeriod = flightPeriod,
        )
        val story: PocketSponsoredStory = mock()
        doReturn(storyCaps).`when`(story).caps

        val result = story.hasLifetimeImpressionsLimitReached()

        assertTrue(result)
    }

    @Test
    fun `GIVEN sponsored story impressions recorded WHEN asking if flight impressions reached THEN return false if not`() {
        val storyCaps = PocketSponsoredStoryCaps(
            currentImpressions = currentImpressions,
            lifetimeCount = 10,
            flightCount = 5,
            flightPeriod = flightPeriod,
        )
        val story: PocketSponsoredStory = mock()
        doReturn(storyCaps).`when`(story).caps

        val result = story.hasFlightImpressionsLimitReached()

        assertFalse(result)
    }

    @Test
    fun `GIVEN sponsored story impressions recorded WHEN asking if flight impressions reached THEN return true if so`() {
        val storyCaps = PocketSponsoredStoryCaps(
            currentImpressions = currentImpressions,
            lifetimeCount = 3,
            flightCount = 2,
            flightPeriod = flightPeriod,
        )
        val story: PocketSponsoredStory = mock()
        doReturn(storyCaps).`when`(story).caps

        val result = story.hasFlightImpressionsLimitReached()

        assertTrue(result)
    }

    @Test
    fun `GIVEN a sponsored story WHEN recording a new impression THEN update the same story to contain a new impression recorded in seconds`() {
        val story = PocketTestResources.dbExpectedPocketSpoc.toPocketSponsoredStory(currentImpressions)

        assertEquals(3, story.caps.currentImpressions.size)
        val result = story.recordNewImpression()

        assertEquals(story.id, result.id)
        assertSame(story.title, result.title)
        assertSame(story.url, result.url)
        assertSame(story.imageUrl, result.imageUrl)
        assertSame(story.sponsor, result.sponsor)
        assertSame(story.shim, result.shim)
        assertEquals(story.priority, result.priority)
        assertEquals(story.caps.lifetimeCount, result.caps.lifetimeCount)
        assertEquals(story.caps.flightCount, result.caps.flightCount)
        assertEquals(story.caps.flightPeriod, result.caps.flightPeriod)

        assertEquals(4, result.caps.currentImpressions.size)
        assertEquals(currentImpressions, result.caps.currentImpressions.take(3))
        // Check if a new impression has been added for around this current time.
        assertTrue(
            LongRange(nowInSeconds - 5, nowInSeconds + 5)
                .contains(result.caps.currentImpressions[3]),
        )
    }

    @Test
    fun `GIVEN sponsored content impressions are recorded WHEN asking for the current flight impressions THEN return all impressions in the flight period`() {
        val frequencyCaps = SponsoredContentFrequencyCaps(
            currentImpressions = currentImpressions,
            flightCount = 5,
            flightPeriod = flightPeriod,
        )
        val sponsoredContent: SponsoredContent = mock()

        doReturn(frequencyCaps).`when`(sponsoredContent).caps

        assertEquals(
            listOf(flightImpression1, flightImpression2),
            sponsoredContent.getCurrentFlightImpressions(),
        )
    }

    @Test
    fun `GIVEN 3 recorded sponsored content impressions and 5 flight count WHEN asking if flight impressions limit has been reached THEN return false`() {
        val frequencyCaps = SponsoredContentFrequencyCaps(
            currentImpressions = currentImpressions,
            flightCount = 5,
            flightPeriod = flightPeriod,
        )
        val sponsoredContent: SponsoredContent = mock()

        doReturn(frequencyCaps).`when`(sponsoredContent).caps

        assertFalse(sponsoredContent.hasFlightImpressionsLimitReached())
    }

    @Test
    fun `GIVEN 3 recorded sponsored content impressions and 2 flight count WHEN asking if flight impressions limit has been reached THEN return true`() {
        val frequencyCaps = SponsoredContentFrequencyCaps(
            currentImpressions = currentImpressions,
            flightCount = 2,
            flightPeriod = flightPeriod,
        )
        val sponsoredContent: SponsoredContent = mock()

        doReturn(frequencyCaps).`when`(sponsoredContent).caps

        assertTrue(sponsoredContent.hasFlightImpressionsLimitReached())
    }

    @Test
    fun `WHEN recording a new impression for a sponsored content THEN add a new impression entry to the current impressions`() {
        val sponsoredContent = PocketTestResources.sponsoredContentEntity.toSponsoredContent(currentImpressions)

        assertEquals(3, sponsoredContent.caps.currentImpressions.size)

        val result = sponsoredContent.recordNewImpression()

        assertSame(sponsoredContent.url, result.url)
        assertSame(sponsoredContent.title, result.title)
        assertSame(sponsoredContent.callbacks, result.callbacks)
        assertSame(sponsoredContent.imageUrl, result.imageUrl)
        assertSame(sponsoredContent.domain, result.domain)
        assertSame(sponsoredContent.excerpt, result.excerpt)
        assertSame(sponsoredContent.sponsor, result.sponsor)
        assertSame(sponsoredContent.blockKey, result.blockKey)
        assertEquals(sponsoredContent.priority, result.priority)
        assertEquals(sponsoredContent.caps.flightCount, result.caps.flightCount)
        assertEquals(sponsoredContent.caps.flightPeriod, result.caps.flightPeriod)

        assertEquals(4, result.caps.currentImpressions.size)
        assertEquals(currentImpressions, result.caps.currentImpressions.take(3))
        // Check if a new impression has been added around the current time.
        assertTrue(
            LongRange(nowInSeconds - 5, nowInSeconds + 5)
                .contains(result.caps.currentImpressions[3]),
        )
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.ext.toContentRecommendation
import mozilla.components.service.pocket.ext.toContentRecommendationEntity
import mozilla.components.service.pocket.ext.toImpressions
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.recommendations.db.ContentRecommendationsDao
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class ContentRecommendationsRepositoryTest {

    private val repository = spy(ContentRecommendationsRepository(testContext))
    private lateinit var dao: ContentRecommendationsDao

    @Before
    fun setUp() {
        dao = mock(ContentRecommendationsDao::class.java)
        `when`(repository.contentRecommendationsDao).thenReturn(dao)
    }

    @Test
    fun `WHEN content recommendations are fetched THEN return storage entries of content recommendations`() = runTest {
        val entity = PocketTestResources.contentRecommendationEntity
        `when`(dao.getContentRecommendations()).thenReturn(listOf(entity))

        val result = repository.getContentRecommendations()

        verify(dao).getContentRecommendations()
        assertEquals(1, result.size)
        assertEquals(entity.toContentRecommendation(), result.first())
    }

    @Test
    fun `WHEN content recommendations impressions are updated THEN persist the updates in storage`() = runTest {
        val recommendations = listOf(PocketTestResources.contentRecommendation)
        val recommendationsShown = recommendations.map { it.toImpressions() }

        repository.updateContentRecommendationsImpressions(recommendations)

        verify(dao).updateRecommendationsImpressions(recommendationsShown)
    }

    @Test
    fun `GIVEN a content recommendations response WHEN content recommendations are updated THEN persist the provided content recommendations in storage`() = runTest {
        val response = PocketTestResources.contentRecommendationsResponse
        val recommendationEntities =
            response.data.map { it.toContentRecommendationEntity(response.recommendedAt) }

        repository.updateContentRecommendations(response)

        verify(dao).cleanAndUpdateContentRecommendations(recommendationEntities)
    }
}

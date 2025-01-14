/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.recommendations

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.ContentRecommendationsRequestConfig
import mozilla.components.service.pocket.PocketStory.ContentRecommendation
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.recommendations.api.ContentRecommendationsEndpoint
import mozilla.components.service.pocket.stories.api.PocketResponse
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi // for runTest
@RunWith(AndroidJUnit4::class)
class ContentRecommendationsUseCasesTest {

    private val client: Client = mock()
    private val useCases = spy(
        ContentRecommendationsUseCases(
            appContext = testContext,
            client = client,
            config = ContentRecommendationsRequestConfig(),
        ),
    )
    private val repository: ContentRecommendationsRepository = mock()
    private val endPoint: ContentRecommendationsEndpoint = mock()

    @Before
    fun setup() {
        doReturn(endPoint).`when`(useCases).getContentRecommendationsEndpoint(any(), any())
        doReturn(repository).`when`(useCases).getContentRecommendationsRepository(any())
    }

    @Test
    fun `WHEN content recommendations getter is called THEN return the list of recommendations from the repository`() = runTest {
        val recommendations = listOf(PocketTestResources.contentRecommendation)
        doReturn(recommendations).`when`(repository).getContentRecommendations()

        val result = useCases.GetContentRecommendations().invoke()

        verify(repository).getContentRecommendations()
        assertEquals(result, recommendations)
    }

    @Test
    fun `GIVEN a successful response WHEN content recommendations are fetched THEN return the list of recommendations from the repository`() = runTest {
        val fetchUseCase = useCases.FetchContentRecommendations()
        val response = getSuccessContentRecommendationsResponse()
        doReturn(response).`when`(endPoint).getContentRecommendations()

        val result = fetchUseCase.invoke()

        assertTrue(result)
        verify(endPoint).getContentRecommendations()
        verify(repository).updateContentRecommendations((response as PocketResponse.Success).data)
    }

    @Test
    fun `GIVEN a failed response WHEN content recommendations are fetched THEN return the list of recommendations from the repository`() = runTest {
        val fetchUseCase = useCases.FetchContentRecommendations()
        val response = getFailResponse()
        doReturn(response).`when`(endPoint).getContentRecommendations()

        val result = fetchUseCase.invoke()

        assertFalse(result)
        verify(endPoint).getContentRecommendations()
        verify(repository, never()).updateContentRecommendations(any())
    }

    @Test
    fun `WHEN content recommendations impressions are updated THEN delegate to the repository to update the recommendations impressions`() = runTest {
        val updateRecommendationsImpressionsUseCase = useCases.UpdateRecommendationsImpressions()
        val recommendationsShown: List<ContentRecommendation> = mock()

        updateRecommendationsImpressionsUseCase.invoke(recommendationsShown)

        verify(repository).updateContentRecommendationsImpressions(recommendationsShown)
    }

    private fun getSuccessContentRecommendationsResponse() =
        PocketResponse.wrap(PocketTestResources.contentRecommendationsResponse)

    private fun getFailResponse() = PocketResponse.wrap(null)
}

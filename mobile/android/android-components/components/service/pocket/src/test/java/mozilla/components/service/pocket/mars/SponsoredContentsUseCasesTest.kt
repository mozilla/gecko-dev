/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.fetch.Client
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.mars.api.MarsSpocsEndpoint
import mozilla.components.service.pocket.mars.api.MarsSpocsRequestConfig
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
class SponsoredContentsUseCasesTest {

    private val client: Client = mock()
    private val useCases = spy(
        SponsoredContentsUseCases(
            appContext = testContext,
            client = client,
            config = MarsSpocsRequestConfig(),
        ),
    )
    private val repository: SponsoredContentsRepository = mock()
    private val endPoint: MarsSpocsEndpoint = mock()

    @Before
    fun setup() {
        doReturn(endPoint).`when`(useCases).getSponsoredContentsProvider(any(), any())
        doReturn(repository).`when`(useCases).getSponsoredContentsRepository(any())
    }

    @Test
    fun `WHEN sponsored contents getter is called THEN return the list of sponsored contents from the repository`() = runTest {
        val sponsoredContents = listOf(PocketTestResources.sponsoredContent)
        doReturn(sponsoredContents).`when`(repository).getAllSponsoredContent()

        val result = useCases.GetSponsoredContents().invoke()

        verify(repository).getAllSponsoredContent()
        assertEquals(result, sponsoredContents)
    }

    @Test
    fun `GIVEN a successful response WHEN sponsored contents are refreshed THEN update the repository with the fetched sponsored contents`() = runTest {
        val response = getSuccessSponsoredContentResponse()
        doReturn(response).`when`(endPoint).getSponsoredStories()

        val result = useCases.RefreshSponsoredContents().invoke()

        assertTrue(result)
        verify(endPoint).getSponsoredStories()
        verify(repository).addSponsoredContents((response as PocketResponse.Success).data)
    }

    @Test
    fun `GIVEN a failed response WHEN sponsored contents are refreshed THEN do not update the sponsored content repository`() = runTest {
        val response = getFailResponse()
        doReturn(response).`when`(endPoint).getSponsoredStories()

        val result = useCases.RefreshSponsoredContents().invoke()

        assertFalse(result)
        verify(endPoint).getSponsoredStories()
        verify(repository, never()).addSponsoredContents(any())
    }

    @Test
    fun `WHEN sponsored content impressions are recorded THEN delegate to the repository to update the impressions`() = runTest {
        val impressions: List<String> = mock()

        useCases.RecordImpressions().invoke(impressions)

        verify(repository).recordImpressions(impressions)
    }

    @Test
    fun `GIVEN a successful response WHEN deleting an user THEN delete all sponsored contents from the repository`() = runTest {
        val response = PocketResponse.wrap(true)
        doReturn(response).`when`(endPoint).deleteUser()

        val result = useCases.DeleteUser().invoke()

        assertTrue(result)
        verify(endPoint).deleteUser()
        verify(repository).deleteAllSponsoredContents()
    }

    @Test
    fun `GIVEN a failed response WHEN deleting an user THEN do not update the sponsored content repository`() = runTest {
        val response = getFailResponse()
        doReturn(response).`when`(endPoint).deleteUser()

        val result = useCases.DeleteUser().invoke()

        assertFalse(result)
        verify(endPoint).deleteUser()
        verify(repository, never()).addSponsoredContents(any())
    }

    private fun getSuccessSponsoredContentResponse() =
        PocketResponse.wrap(PocketTestResources.marsSpocsResponse)

    private fun getFailResponse() = PocketResponse.wrap(null)
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.service.pocket.ext.toSponsoredContent
import mozilla.components.service.pocket.ext.toSponsoredContentEntity
import mozilla.components.service.pocket.helpers.PocketTestResources
import mozilla.components.service.pocket.mars.db.SponsoredContentImpressionEntity
import mozilla.components.service.pocket.mars.db.SponsoredContentsDao
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
class SponsoredContentsRepositoryTest {

    private val repository = spy(SponsoredContentsRepository(testContext))
    private lateinit var dao: SponsoredContentsDao

    @Before
    fun setUp() {
        dao = mock(SponsoredContentsDao::class.java)
        `when`(repository.dao).thenReturn(dao)
    }

    @Test
    fun `WHEN sponsored contents are fetched THEN return storage entries of sponsored contents`() = runTest {
        val entity = PocketTestResources.sponsoredContentEntity
        val impression = SponsoredContentImpressionEntity(url = entity.url)

        `when`(dao.getSponsoredContents()).thenReturn(listOf(entity))
        `when`(dao.getSponsoredContentImpressions()).thenReturn(listOf(impression))

        val result = repository.getAllSponsoredContent()

        verify(dao).getSponsoredContents()
        assertEquals(1, result.size)
        assertEquals(
            entity.toSponsoredContent(impressions = listOf(impression.impressionDateInSeconds)),
            result.first(),
        )
    }

    @Test
    fun `WHEN deleting all sponsored contents THEN delete all from the database`() = runTest {
        repository.deleteAllSponsoredContents()

        verify(dao).deleteAllSponsoredContents()
    }

    @Test
    fun `GIVEN a sponsored contents response WHEN sponsored contents are added THEN persist the provided sponsored contents in storage`() = runTest {
        val response = PocketTestResources.marsSpocsResponse
        val entities = response.spocs.map { it.toSponsoredContentEntity() }

        repository.addSponsoredContents(response)

        verify(dao).cleanOldAndInsertNewSponsoredContents(sponsoredContents = entities)
    }

    @Test
    fun `WHEN sponsored content impressions are recorded THEN persist the impressions in storage`() = runTest {
        val sponsoredContents = listOf(PocketTestResources.marsSpocsResponseItem)
        val impressions = sponsoredContents.map { SponsoredContentImpressionEntity(it.url) }

        repository.recordImpressions(impressions = sponsoredContents.map { it.url })

        verify(dao).recordImpressions(impressions)
    }
}

package mozilla.components.feature.search

import mozilla.appservices.remotesettings.RemoteSettingsClient
import mozilla.appservices.remotesettings.RemoteSettingsRecord
import mozilla.appservices.remotesettings.RemoteSettingsService
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner
import mozilla.components.support.remotesettings.RemoteSettingsService as MozillaRemoteSettingsService

@RunWith(RobolectricTestRunner::class)
class RemoteSettingsRepositoryTest {

    private lateinit var mockClient: RemoteSettingsClient
    private lateinit var mockRemoteSettingsService: RemoteSettingsService
    private lateinit var mockMozillaService: MozillaRemoteSettingsService
    private val testCollectionName = "test-collection"

    @Before
    fun setup() {
        mockClient = mock()
        mockRemoteSettingsService = mock()
        mockMozillaService = mock()

        `when`(mockMozillaService.remoteSettingsService).thenReturn(mockRemoteSettingsService)
        `when`(mockRemoteSettingsService.makeClient(testCollectionName)).thenReturn(mockClient)
    }

    @Test
    fun `GIVEN a successful response WHEN fetchRemoteResponse is called THEN records are returned`() {
        val mockRecords = listOf<RemoteSettingsRecord>(mock(), mock())
        `when`(mockClient.getRecords()).thenReturn(mockRecords)

        val result = RemoteSettingsRepository.fetchRemoteResponse(
            mockMozillaService,
            testCollectionName,
        )

        verify(mockClient).getRecords()
        assertEquals(mockRecords, result)
    }

    @Test
    fun `GIVEN a successful empty response WHEN fetchRemoteResponse is called THEN empty list is returned`() {
        `when`(mockClient.getRecords()).thenReturn(emptyList())

        val result = RemoteSettingsRepository.fetchRemoteResponse(
            mockMozillaService,
            testCollectionName,
        )

        verify(mockClient).getRecords()
        assertTrue("Result should be an empty list", result!!.isEmpty())
    }

    @Test
    fun `GIVEN an IllegalStateException occurs WHEN fetchRemoteResponse is called THEN empty list is returned`() {
        val specificException = IllegalStateException("Test exception")
        doThrow(specificException).`when`(mockClient).getRecords()

        val result = RemoteSettingsRepository.fetchRemoteResponse(
            mockMozillaService,
            testCollectionName,
        )

        verify(mockClient).getRecords()
        assertTrue("Result should be an empty list due to caught IllegalStateException", result!!.isEmpty())
    }
}

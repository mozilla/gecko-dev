/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.mars

import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.MutableHeaders
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.any
import mozilla.components.support.test.helpers.MockResponses
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import java.io.IOException

class MARSUseCasesTest {

    private lateinit var client: Client
    private lateinit var useCases: MARSUseCases

    @Before
    fun setUp() {
        client = mock()
        useCases = MARSUseCases(client)
    }

    @Test
    fun `WHEN sending a click or impression callback THEN ensure the correct request parameters are used`() {
        val url = "https://firefox.com/click"

        assertRequestParams(
            client = client,
            makeRequest = {
                useCases.recordInteraction(url)
            },
            assertParams = { request ->
                assertEquals(url, request.url)
                assertEquals(Request.Method.GET, request.method)
                assertTrue(request.conservative)
            },
        )
    }

    @Test
    fun `WHEN sending a click or impression callback and the client throws an IOException THEN false is returned`() {
        val url = "https://firefox.com/click"
        whenever(client.fetch(any())).thenThrow(IOException::class.java)
        assertFalse(useCases.recordInteraction(url))
    }

    @Test
    fun `WHEN sending a click or impression callback and the response is null THEN false is returned`() {
        val url = "https://firefox.com/click"
        whenever(client.fetch(any())).thenReturn(null)
        assertFalse(useCases.recordInteraction(url))
    }

    @Test
    fun `WHEN sending a click or impression callback and the response is a failure THEN false is returned`() {
        val url = "https://firefox.com/click"
        val errorResponse = MockResponses.getError()

        whenever(client.fetch(any())).thenReturn(errorResponse)

        assertFalse(useCases.recordInteraction(url))
        verify(errorResponse, times(1)).close()
    }

    @Test
    fun `WHEN sending a click or impression callback and the response is success THEN true is returned`() {
        val url = "https://firefox.com/click"
        val successResponse = MockResponses.getSuccess()

        whenever(client.fetch(any())).thenReturn(successResponse)

        assertTrue(useCases.recordInteraction(url))
        verify(successResponse, times(1)).close()
    }
}

private fun assertRequestParams(
    client: Client,
    makeRequest: () -> Unit,
    assertParams: (Request) -> Unit,
) {
    whenever(client.fetch(any())).thenAnswer {
        val request = it.arguments[0] as Request
        assertParams(request)
        Response("https://mozilla.org", 200, MutableHeaders(), Response.Body("".byteInputStream()))
    }

    makeRequest()

    // Ensure fetch is called so that the assertions in assertParams are called.
    verify(client, times(1)).fetch(any())
}

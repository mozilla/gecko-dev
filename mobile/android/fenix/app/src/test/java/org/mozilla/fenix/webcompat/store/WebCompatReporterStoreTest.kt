/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class WebCompatReporterStoreTest {

    private val store: WebCompatReporterStore = WebCompatReporterStore()

    @Test
    fun `WHEN the broken URL is updated THEN the state should be updated`() {
        val expectedUrl = "https://www.mozilla.org/"

        store.dispatch(WebCompatReporterAction.BrokenSiteChanged(newUrl = expectedUrl))
        assertEquals(expectedUrl, store.state.enteredUrl)
    }

    @Test
    fun `WHEN the broken URL is updated with a valid URL THEN the state should not have an input error`() {
        store.dispatch(WebCompatReporterAction.BrokenSiteChanged(newUrl = "https://www.mozilla.org/"))
        assertFalse(store.state.hasUrlTextError)
    }

    @Test
    fun `WHEN the broken URL is updated with an invalid URL THEN the state should have an input error`() {
        store.dispatch(WebCompatReporterAction.BrokenSiteChanged(newUrl = ""))
        assertTrue(store.state.hasUrlTextError)
    }

    @Test
    fun `WHEN the reason is updated THEN the state should be updated`() {
        val expected = WebCompatReporterState.BrokenSiteReason.Slow

        store.dispatch(WebCompatReporterAction.ReasonChanged(newReason = expected))
        assertEquals(expected, store.state.reason)
    }

    @Test
    fun `WHEN the problem description is updated THEN the state is updated`() {
        val expected = "Test description"

        store.dispatch(WebCompatReporterAction.ProblemDescriptionChanged(newProblemDescription = expected))
        assertEquals(expected, store.state.problemDescription)
    }

    @Test
    fun `WHEN the report is sent THEN the state remains the same`() {
        val expected = store.state

        store.dispatch(WebCompatReporterAction.SendReportClicked)
        assertEquals(expected, store.state)
    }

    @Test
    fun `WHEN the send more info button is clicked THEN the state remains the same`() {
        val expected = store.state

        store.dispatch(WebCompatReporterAction.SendMoreInfoClicked)
        assertEquals(expected, store.state)
    }

    @Test
    fun `WHEN the cancel button is clicked THEN the state remains the same`() {
        val expected = store.state

        store.dispatch(WebCompatReporterAction.CancelClicked)
        assertEquals(expected, store.state)
    }

    @Test
    fun `WHEN the back button is clicked THEN the state remains the same`() {
        val expected = store.state

        store.dispatch(WebCompatReporterAction.BackPressed)
        assertEquals(expected, store.state)
    }
}

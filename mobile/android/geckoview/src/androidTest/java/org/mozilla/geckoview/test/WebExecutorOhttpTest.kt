/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.* // ktlint-disable no-wildcard-imports
import org.mozilla.geckoview.test.util.RuntimeCreator

@RunWith(AndroidJUnit4::class)
@MediumTest
class WebExecutorOhttpTest : BaseSessionTest() {
    // We just want to make sure we don't crash when trying to use ohttp.
    // We test the actual functionality in toolkit/components/resistfingerprinting/tests/xpcshell/test_ohttp_client.js.
    @Test(expected = WebRequestError::class)
    fun testOhttp() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                // Don't make external requests.
                "network.ohttp.configURL" to "https://example.com",
                "network.ohttp.relayURL" to "https://example.com",
            ),
        )

        GeckoWebExecutor(RuntimeCreator.getRuntime()).fetch(WebRequest.Builder("https://example.com").build(), GeckoWebExecutor.FETCH_FLAGS_OHTTP).poll(5 * 100)
    }
}

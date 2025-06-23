/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.gecko

import android.content.Context
import io.mockk.mockk
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.storage.CreditCardsAddressesStorage
import mozilla.components.concept.storage.LoginsStorage
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.helpers.TestHelper

class CrashPullDelegateTest {
    private lateinit var context: Context
    private lateinit var mockPolicy: EngineSession.TrackingProtectionPolicy
    private lateinit var mockAutofill: Lazy<CreditCardsAddressesStorage>
    private lateinit var mockLogin: Lazy<LoginsStorage>
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Main)

    @Before
    fun setUp() {
        context = TestHelper.appContext
        mockPolicy = mockk<EngineSession.TrackingProtectionPolicy>()
        mockAutofill = mockk<Lazy<CreditCardsAddressesStorage>>()
        mockLogin = mockk<Lazy<LoginsStorage>>()
    }

    @Test
    fun test_crash_pull_delegate_exists() {
        // scope.launch required to run in the correct thread for GeckoRuntime
        // but the test needs to wait on its completion to ensure assert is
        // verified, hence runBlocking.
        //
        // We cannot use runTestOnMain here because it looks not to be available.
        runBlocking {
            scope.launch {
                val runtime = GeckoProvider.getOrCreateRuntime(context, mockAutofill, mockLogin, mockPolicy)
                assertNotNull(runtime.crashPullDelegate)
                runtime.crashPullDelegate?.onCrashPull(arrayOf("1", "2"))
            }.join()
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.helpers

import android.Manifest
import android.os.Build
import android.util.Log
import androidx.test.rule.GrantPermissionRule
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.store.BrowserStore
import okhttp3.mockwebserver.MockWebServer
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.AppAndSystemHelper.allowOrPreventSystemUIFromReadingTheClipboard
import org.mozilla.fenix.helpers.AppAndSystemHelper.enableOrDisableBackGestureNavigationOnDevice
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.ui.robots.notificationShade

/**
 * Standard Test setup and tear down methods to run before each test.
 * Some extra clean-up is required when we're using the org.mozilla.fenix.helpers.RetryTestRule (the instrumentation does not do that in this case).
 *
 */
open class TestSetup {
    lateinit var mockWebServer: MockWebServer
    lateinit var browserStore: BrowserStore

    @get:Rule
    val generalPermissionRule: GrantPermissionRule =
        if (Build.VERSION.SDK_INT >= 33) {
            GrantPermissionRule.grant(
                Manifest.permission.POST_NOTIFICATIONS,
            )
        } else {
            GrantPermissionRule.grant()
        }

    @Before
    open fun setUp() {
        Log.i(TAG, "TestSetup: Starting the @Before setup")

        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            allowOrPreventSystemUIFromReadingTheClipboard(allowToReadClipboard = false)
        }

        // Enable the back gesture from the edge of the screen on the device.
        enableOrDisableBackGestureNavigationOnDevice(backGestureNavigationEnabled = true)

        runBlocking {
            // Check and clear the downloads folder, in case the tearDown method is not executed.
            // This will only work in case of a RetryTestRule execution.
            AppAndSystemHelper.clearDownloadsFolder()
            // Make sure the Wifi and Mobile Data connections are on.
            AppAndSystemHelper.setNetworkEnabled(true)
            // Clear bookmarks left after a failed test, before a retry.
            AppAndSystemHelper.deleteBookmarksStorage()
            // Clear history left after a failed test, before a retry.
            AppAndSystemHelper.deleteHistoryStorage()
            // Clear permissions left after a failed test, before a retry.
            AppAndSystemHelper.deletePermissionsStorage()
        }

        // Initializing this as part of class construction, below the rule would throw a NPE.
        // So we are initializing this here instead of in all related tests.
        Log.i(TAG, "TestSetup: Trying to initialize the browserStore instance")
        browserStore = TestHelper.appContext.components.core.store
        Log.i(TAG, "TestSetup: Initialized the browserStore instance")
        // Clear pre-existing notifications.
        notificationShade {
            cancelAllShownNotifications()
            // Closes the notification tray if it's open, otherwise it's a no-op.
            Log.i(TAG, "TestSetup: Trying to close the notification tray, in case it's open.")
            mDevice.executeShellCommand("cmd statusbar collapse")
        }

        mockWebServer = MockWebServer().apply {
            dispatcher = AndroidAssetDispatcher()
        }
        try {
            Log.i(TAG, "Try starting mockWebServer")
            mockWebServer.start()
        } catch (e: Exception) {
            Log.i(TAG, "Exception caught. Re-starting mockWebServer")
            mockWebServer.shutdown()
            mockWebServer.start()
        }
    }

    @After
    open fun tearDown() {
        Log.i(TAG, "TestSetup: Starting the @After tearDown methods.")
        runBlocking {
            // Clear the downloads folder after each test even if the test fails.
            AppAndSystemHelper.clearDownloadsFolder()
        }
    }
}

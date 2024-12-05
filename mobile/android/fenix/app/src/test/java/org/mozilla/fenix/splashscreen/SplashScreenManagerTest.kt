/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.splashscreen

import androidx.core.splashscreen.SplashScreen
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SplashScreenManagerTest {
    private val coroutineScope = TestScope()

    @Before
    fun setup() {
        Dispatchers.setMain(StandardTestDispatcher(coroutineScope.testScheduler))
    }

    @Test
    fun `GIVEN a device that does not support a splash screen WHEN maybeShowSplashScreen is called THEN we should get a did not show the splash screen result`() {
        var splashScreenShown = false
        var result: SplashScreenManagerResult? = null
        val splashScreenManager = buildSplashScreen(
            showSplashScreen = { _ -> splashScreenShown = true },
            isDeviceSupported = { false },
            onSplashScreenFinished = { result = it },
        )

        Assert.assertNull(result)
        splashScreenManager.showSplashScreen()

        Assert.assertFalse(splashScreenShown)
        Assert.assertEquals(SplashScreenManagerResult.DidNotPresentSplashScreen, result)
    }

    @Test
    fun `WHEN a user has already seen the splash screen THEN do not show splash screen`() {
        val storage = object : SplashScreenStorage {
            override var isFirstSplashScreenShown = true
        }
        var result: SplashScreenManagerResult? = null
        val splashScreenManager = buildSplashScreen(
            storage = storage,
            onSplashScreenFinished = { result = it },
        )

        Assert.assertNull(result)
        splashScreenManager.showSplashScreen()

        Assert.assertEquals(SplashScreenManagerResult.DidNotPresentSplashScreen, result)
    }

    @Test
    fun `WHEN a device is supported and the splash screen has not been shown yet THEN show the splash screen`() {
        var splashScreenShown = false
        val splashScreenManager = buildSplashScreen(
            showSplashScreen = { _ -> splashScreenShown = true },
        )

        splashScreenManager.showSplashScreen()

        Assert.assertTrue(splashScreenShown)
    }

    @Test
    fun `WHEN a user has not seen the splash screen THEN show splash screen and update storage`() {
        var splashScreenShown = false
        val storage = object : SplashScreenStorage {
            override var isFirstSplashScreenShown = false
        }
        val splashScreenManager = buildSplashScreen(
            storage = storage,
            showSplashScreen = { _ -> splashScreenShown = true },
        )

        Assert.assertFalse(splashScreenShown)
        Assert.assertFalse(storage.isFirstSplashScreenShown)
        splashScreenManager.showSplashScreen()

        Assert.assertTrue(splashScreenShown)
        Assert.assertTrue(storage.isFirstSplashScreenShown)
    }

    @Test
    fun `GIVEN an operation shorter than the splashscreen timeout WHEN splash screen is shown THEN we should get an operation completed result`() = runTest {
        val operationTime = 400L
        val splashScreenTimeout = 2_500L
        val fastOperation = object : SplashScreenOperation {
            override val type: String
                get() = "so operation much fast"
            override val dataFetched: Boolean
                get() = false

            override suspend fun run() {
                delay(operationTime)
            }
        }
        var result: SplashScreenManagerResult? = null
        val splashScreenManager = buildSplashScreen(
            splashScreenOperation = fastOperation,
            splashScreenTimeout = splashScreenTimeout,
            onSplashScreenFinished = { result = it },
        )

        splashScreenManager.showSplashScreen()

        Assert.assertNull(result)
        coroutineScope.advanceUntilIdle()
        Assert.assertTrue(result is SplashScreenManagerResult.OperationFinished)
    }

    @Test
    fun `GIVEN a splash manager with an operation running longer than the splashscreen timeout WHEN splash screen is shown THEN we should get an timeout exceeded result`() = runTest {
        val operationTime = 11_000L
        val splashScreenTimeout = 2_500L
        val slowOperation = object : SplashScreenOperation {
            override val type: String
                get() = "so slow much trouble"
            override val dataFetched: Boolean
                get() = false

            override suspend fun run() {
                delay(operationTime)
            }
        }
        var result: SplashScreenManagerResult? = null
        val splashScreenManager = buildSplashScreen(
            splashScreenOperation = slowOperation,
            splashScreenTimeout = splashScreenTimeout,
            onSplashScreenFinished = { result = it },
        )

        splashScreenManager.showSplashScreen()

        Assert.assertNull(result)
        coroutineScope.advanceUntilIdle()
        Assert.assertTrue(result is SplashScreenManagerResult.TimeoutExceeded)
    }

    private fun buildSplashScreen(
        splashScreenOperation: SplashScreenOperation = object : SplashScreenOperation {
            override val type: String
                get() = "test"
            override val dataFetched: Boolean
                get() = false

            override suspend fun run() {
                delay(2_400)
            }
        },
        splashScreenTimeout: Long = 2_500,
        showSplashScreen: (SplashScreen.KeepOnScreenCondition) -> Unit = { _ -> },
        onSplashScreenFinished: (SplashScreenManagerResult) -> Unit = { _ -> },
        storage: SplashScreenStorage = object : SplashScreenStorage {
            override var isFirstSplashScreenShown = false
        },
        isDeviceSupported: () -> Boolean = { true },
    ): SplashScreenManager {
        return SplashScreenManager(
            splashScreenTimeout = splashScreenTimeout,
            splashScreenOperation = splashScreenOperation,
            showSplashScreen = showSplashScreen,
            onSplashScreenFinished = onSplashScreenFinished,
            storage = storage,
            isDeviceSupported = isDeviceSupported,
            scope = coroutineScope,
        )
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.splashscreen

import androidx.core.splashscreen.SplashScreen.KeepOnScreenCondition
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.MainScope
import kotlinx.coroutines.async
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.selects.select
import kotlinx.coroutines.withContext
import org.mozilla.fenix.utils.Settings

/**
 * An interface to persis the state of the splash screen.
 */
interface SplashScreenStorage {
    /**
     * Indicates whether the first splash screen has already been shown.
     */
    var isFirstSplashScreenShown: Boolean
}

/**
 * A default implementation of `SplashScreenStorage`.
 *
 * @property settings The settings object used to persist the splash screen state.
 */
class DefaultSplashScreenStorage(
    val settings: Settings,
) : SplashScreenStorage {
    override var isFirstSplashScreenShown
        get() = settings.isFirstSplashScreenShown
        set(value) { settings.isFirstSplashScreenShown = value }
}

/**
 * Possible results of showing the splash screen operation.
 */
sealed class SplashScreenManagerResult {

    /**
     * Indicates the operation completed successfully.
     *
     * @property type The type of the operation.
     * @property dataFetched Whether data was fetched during the operation.
     */
    data class OperationFinished(val type: String, val dataFetched: Boolean) : SplashScreenManagerResult()

    /**
     * Indicates the operation exceeded its timeout.
     *
     * @property type The type of the operation.
     * @property dataFetched Whether data was fetched during the operation.
     */
    data class TimeoutExceeded(val type: String, val dataFetched: Boolean) : SplashScreenManagerResult()

    /**
     * Indicates the splash screen was not presented.
     */
    data object DidNotPresentSplashScreen : SplashScreenManagerResult()

    val sendTelemetry: Boolean
        get() = when (this) {
            is TimeoutExceeded, is OperationFinished -> true
            DidNotPresentSplashScreen -> false
        }

    val wasDataFetched: Boolean
        get() = when (this) {
            is OperationFinished -> this.dataFetched
            is TimeoutExceeded -> this.dataFetched
            else -> false
        }
}

/**
 * Manages the splash screen logic, including conditions for displaying the splash screen
 * and handling experiment data application and fetching.
 *
 * @param splashScreenOperation The operation to execute during the splash screen.
 * @param splashScreenTimeout The timeout for the operation.
 * @param storage Interface to persist and retrieve splash screen state.
 * @param isDeviceSupported Determines whether the device supports the splash screen.
 * @param scope The coroutine scope.
 * @param showSplashScreen Callback to display the splash screen.
 * @param onSplashScreenFinished Callback after the splash screen finishes.
 */
class SplashScreenManager(
    private val splashScreenOperation: SplashScreenOperation,
    private val splashScreenTimeout: Long,
    private val storage: SplashScreenStorage,
    private val isDeviceSupported: () -> Boolean,
    private val scope: CoroutineScope = MainScope(),
    private val showSplashScreen: (KeepOnScreenCondition) -> Unit,
    private val onSplashScreenFinished: (SplashScreenManagerResult) -> Unit,
) : KeepOnScreenCondition {

    private var isSplashScreenFinished = false
    override fun shouldKeepOnScreen(): Boolean {
        return !isSplashScreenFinished
    }

    /**
     * If conditions are met, this function delays the system splash screen while
     * trying to complete [splashScreenOperation] before reaching [splashScreenTimeout].
     */
    fun showSplashScreen() {
        if (!isDeviceSupported() || storage.isFirstSplashScreenShown) {
            onSplashScreenFinished(SplashScreenManagerResult.DidNotPresentSplashScreen)
            return
        }

        storage.isFirstSplashScreenShown = true
        showSplashScreen(this)

        scope.launch {
            val result = select {
                async {
                    splashScreenOperation.run()
                    isSplashScreenFinished = true
                    return@async SplashScreenManagerResult.OperationFinished(
                        type = splashScreenOperation.type,
                        dataFetched = splashScreenOperation.dataFetched,
                    )
                }.onAwait { it }

                async {
                    delay(splashScreenTimeout)
                    return@async SplashScreenManagerResult.TimeoutExceeded(
                        type = splashScreenOperation.type,
                        dataFetched = splashScreenOperation.dataFetched,
                    )
                }.onAwait { it }
            }

            withContext(Dispatchers.Main) {
                isSplashScreenFinished = true
                onSplashScreenFinished(result)
            }
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.screendetection

import android.app.Activity.ScreenCaptureCallback
import android.os.Build
import android.view.WindowManager
import androidx.annotation.ChecksSdkIntAtLeast
import androidx.annotation.RequiresApi
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import mozilla.components.support.base.log.logger.Logger
import java.util.function.Consumer

/**
 * Represents the different modes for detecting screen capture events.
 */
enum class DetectionMode {
    /**
     * Detects a screen capture.
     */
    SCREEN_CAPTURE,

    /**
     * Detects when the screen is being recorded.
     */
    SCREEN_RECORDING,

    /**
     * Detects both screenshots and screen recordings.
     */
    ALL,
}

/**
 * Default callback for screen recording state changes.
 * Logs whether the app window is being recorded.
 */
private val defaultScreenRecordingCallback = Consumer<Int> { state ->
    if (state == WindowManager.SCREEN_RECORDING_STATE_VISIBLE) {
        Logger.info("ScreenDetectionFeature: App window is being recorded")
    } else {
        Logger.info("ScreenDetectionFeature: App window is not being recorded")
    }
}

/**
 * Default callback for screen capture events.
 * Logs when a screenshot is taken.
 */
private val defaultScreenCaptureCallback = ScreenCaptureCallback {
    Logger.info("ScreenDetectionFeature: A screenshot was taken")
}

/**
 * Helper object to check the SDK version for feature support.
 */
object ScreenDetectionSdkVersionHelper {
    /**
     * Checks if the device supports screen recording detection.
     *
     * @return `true` if the device's SDK version is at least Vanilla Ice Cream, `false` otherwise.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    fun isScreenRecordingDetectionSupported(): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM

    /**
     * Checks if the device supports screenshot detection.
     *
     * @return `true` if the device's SDK version is at least Upside Down Cake, `false` otherwise.
     */
    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    fun isScreenCaptureDetectionSupported(): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
}

/**
 * Handles the detection of screen recordings and screenshots in private tabs.
 *
 * @param activity The activity where the detection is to be managed.
 * @param screenRecordingCallback Callback to be invoked when the screen recording state changes.
 * @param screenCaptureCallback Callback to be invoked when a screenshot is taken.
 * @param detectionMode The mode of detection, can be SCREENSHOT, SCREEN_RECORDING, or ALL.
 */
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
class ScreenDetectionFeature(
    private val activity: AppCompatActivity,
    private val screenRecordingCallback: Consumer<Int> = defaultScreenRecordingCallback,
    private val screenCaptureCallback: ScreenCaptureCallback = defaultScreenCaptureCallback,
    private val detectionMode: DetectionMode = DetectionMode.ALL,
) : DefaultLifecycleObserver {

    @VisibleForTesting
    internal var screenDetectionSdkVersionHelper: ScreenDetectionSdkVersionHelper = ScreenDetectionSdkVersionHelper

    override fun onStart(owner: LifecycleOwner) {
        if (!screenDetectionSdkVersionHelper.isScreenCaptureDetectionSupported()) {
            return
        }

        if (screenDetectionSdkVersionHelper.isScreenRecordingDetectionSupported() &&
            (detectionMode == DetectionMode.SCREEN_RECORDING || detectionMode == DetectionMode.ALL)
        ) {
            registerScreenRecordingCallback()
        }

        if (detectionMode == DetectionMode.SCREEN_CAPTURE || detectionMode == DetectionMode.ALL) {
            registerScreenCaptureCallback()
        }
    }

    override fun onStop(owner: LifecycleOwner) {
        if (!screenDetectionSdkVersionHelper.isScreenCaptureDetectionSupported()) {
            return
        }

        if (screenDetectionSdkVersionHelper.isScreenRecordingDetectionSupported()) {
            activity.windowManager.removeScreenRecordingCallback(
                screenRecordingCallback,
            )
        }

        activity.unregisterScreenCaptureCallback(screenCaptureCallback)
    }

    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    internal fun registerScreenRecordingCallback() {
        val initialRecordingState =
            activity.windowManager.addScreenRecordingCallback(
                activity.mainExecutor,
                screenRecordingCallback,
            )
        screenRecordingCallback.accept(initialRecordingState)
    }

    @VisibleForTesting
    @RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    internal fun registerScreenCaptureCallback() {
        activity.registerScreenCaptureCallback(
            activity.mainExecutor,
            screenCaptureCallback,
        )
    }
}

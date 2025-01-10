/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.screendetection

import android.view.WindowManager
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.LifecycleOwner
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

class ScreenDetectionFeatureTest {

    private lateinit var activity: AppCompatActivity
    private lateinit var windowManager: WindowManager
    private lateinit var lifecycleOwner: LifecycleOwner
    private lateinit var sdkVersionHelperMock: ScreenDetectionSdkVersionHelper

    @Before
    fun setup() {
        activity = mock()
        windowManager = mock()
        lifecycleOwner = mock()
        sdkVersionHelperMock = mock()

        `when`(activity.windowManager).thenReturn(windowManager)
        `when`(activity.mainExecutor).thenReturn(mock())
    }

    @Test
    fun `should add screen recording callback when detection mode is SCREEN_RECORDING`() {
        `when`(sdkVersionHelperMock.isScreenRecordingDetectionSupported()).thenReturn(true)
        `when`(sdkVersionHelperMock.isScreenCaptureDetectionSupported()).thenReturn(true)

        val manager = spy(
            ScreenDetectionFeature(
                activity,
                detectionMode = DetectionMode.SCREEN_RECORDING,
            ).apply {
                screenDetectionSdkVersionHelper = sdkVersionHelperMock
            },
        )

        manager.onStart(lifecycleOwner)

        verify(manager).registerScreenRecordingCallback()
        verify(windowManager).addScreenRecordingCallback(any(), any())

        verify(manager, never()).registerScreenCaptureCallback()
        verify(activity, never()).registerScreenCaptureCallback(any(), any())
    }

    @Test
    fun `should add screen capture callback when detection mode is SCREENSHOT`() {
        `when`(sdkVersionHelperMock.isScreenRecordingDetectionSupported()).thenReturn(true)
        `when`(sdkVersionHelperMock.isScreenCaptureDetectionSupported()).thenReturn(true)

        val manager = spy(
            ScreenDetectionFeature(
                activity,
                detectionMode = DetectionMode.SCREEN_CAPTURE,
            ).apply {
                screenDetectionSdkVersionHelper = sdkVersionHelperMock
            },
        )

        manager.onStart(lifecycleOwner)

        verify(manager).registerScreenCaptureCallback()
        verify(activity).registerScreenCaptureCallback(any(), any())

        verify(manager, never()).registerScreenRecordingCallback()
        verify(windowManager, never()).addScreenRecordingCallback(any(), any())
    }

    @Test
    fun `should add both callbacks when detection mode is ALL`() {
        `when`(sdkVersionHelperMock.isScreenRecordingDetectionSupported()).thenReturn(true)
        `when`(sdkVersionHelperMock.isScreenCaptureDetectionSupported()).thenReturn(true)

        val manager = spy(
            ScreenDetectionFeature(
                activity,
                detectionMode = DetectionMode.ALL,
            ).apply {
                screenDetectionSdkVersionHelper = sdkVersionHelperMock
            },
        )

        manager.onStart(lifecycleOwner)

        verify(manager).registerScreenCaptureCallback()
        verify(manager).registerScreenRecordingCallback()
        verify(activity).registerScreenCaptureCallback(any(), any())
        verify(windowManager).addScreenRecordingCallback(any(), any())
    }

    @Test
    fun `should not add callbacks if screen capture detection is not supported`() {
        `when`(sdkVersionHelperMock.isScreenCaptureDetectionSupported()).thenReturn(false)

        val manager = spy(
            ScreenDetectionFeature(
                activity,
                detectionMode = DetectionMode.ALL,
            ).apply {
                screenDetectionSdkVersionHelper = sdkVersionHelperMock
            },
        )

        manager.onCreate(lifecycleOwner)

        verify(manager, never()).registerScreenCaptureCallback()
        verify(manager, never()).registerScreenRecordingCallback()

        verify(activity, never()).registerScreenCaptureCallback(any(), any())
        verify(windowManager, never()).addScreenRecordingCallback(any(), any())
    }

    @Test
    fun `should not remove callbacks if screen capture detection is not supported`() {
        `when`(sdkVersionHelperMock.isScreenCaptureDetectionSupported()).thenReturn(false)

        val manager = spy(
            ScreenDetectionFeature(
                activity,
                detectionMode = DetectionMode.ALL,
            ).apply {
                screenDetectionSdkVersionHelper = sdkVersionHelperMock
            },
        )

        manager.onStop(lifecycleOwner)

        verify(windowManager, never()).removeScreenRecordingCallback(any())
        verify(activity, never()).unregisterScreenCaptureCallback(any())
    }

    @Test
    fun `should remove both callbacks on stop`() {
        `when`(sdkVersionHelperMock.isScreenRecordingDetectionSupported()).thenReturn(true)
        `when`(sdkVersionHelperMock.isScreenCaptureDetectionSupported()).thenReturn(true)

        val manager = spy(
            ScreenDetectionFeature(
                activity,
                detectionMode = DetectionMode.SCREEN_RECORDING,
            ).apply {
                screenDetectionSdkVersionHelper = sdkVersionHelperMock
            },
        )

        manager.onStop(lifecycleOwner)

        verify(windowManager).removeScreenRecordingCallback(any())
        verify(activity).unregisterScreenCaptureCallback(any())
    }
}

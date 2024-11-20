/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.qr

import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CameraMetadata
import android.os.Build.VERSION_CODES
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class CameraManagerTest {
    private lateinit var cameraManager: CameraManager
    private val cameraId = "0"
    private val characteristics: CameraCharacteristics = mock()

    @Before
    fun setUp() {
        cameraManager = mock()
        whenever(cameraManager.getCameraCharacteristics(cameraId)).thenReturn(
            characteristics,
        )
    }

    @Test
    @Config(sdk = [VERSION_CODES.UPSIDE_DOWN_CAKE])
    fun `isLowLightBoostSupported returns false if SDK version is below VANILLA_ICE_CREAM`() {
        assertFalse(cameraManager.isLowLightBoostSupported(cameraId))
    }

    @Test
    fun `isLowLightBoostSupported returns false if availableAeModes is null`() {
        whenever(characteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_MODES)).thenReturn(
            null,
        )
        assertFalse(cameraManager.isLowLightBoostSupported(cameraId))
    }

    @Test
    fun `isLowLightBoostSupported returns false if low light boost mode is not available`() {
        whenever(characteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_MODES)).thenReturn(
            intArrayOf(CameraMetadata.CONTROL_AE_MODE_ON),
        )
        assertFalse(cameraManager.isLowLightBoostSupported(cameraId))
    }

    @Test
    fun `isLowLightBoostSupported returns true if low light boost mode is available`() {
        whenever(characteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_MODES)).thenReturn(
            intArrayOf(CameraMetadata.CONTROL_AE_MODE_ON_LOW_LIGHT_BOOST_BRIGHTNESS_PRIORITY),
        )
        assertTrue(cameraManager.isLowLightBoostSupported(cameraId))
    }
}

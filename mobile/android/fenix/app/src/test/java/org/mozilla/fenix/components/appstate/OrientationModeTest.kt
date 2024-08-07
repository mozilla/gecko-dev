/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.components.appstate

import android.content.res.Configuration
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.security.InvalidParameterException

class OrientationModeTest {
    @Test
    fun `WHEN screen configuration is undefined THEN orientation mode is portrait as well`() {
        val screenConfiguration = Configuration.ORIENTATION_UNDEFINED
        val orientationMode = OrientationMode.Companion.fromInteger(screenConfiguration)
        val expectedOrientation = OrientationMode.Undefined

        assertEquals(expectedOrientation, orientationMode)
    }

    @Test
    fun `WHEN screen configuration is portrait THEN orientation mode is portrait as well`() {
        val screenConfiguration = Configuration.ORIENTATION_PORTRAIT
        val orientationMode = OrientationMode.Companion.fromInteger(screenConfiguration)
        val expectedOrientation = OrientationMode.Portrait

        assertEquals(expectedOrientation, orientationMode)
    }

    @Test
    fun `WHEN screen configuration is landscape THEN orientation mode is landscape as well`() {
        val screenConfiguration = Configuration.ORIENTATION_LANDSCAPE
        val orientationMode = OrientationMode.Companion.fromInteger(screenConfiguration)
        val expectedOrientation = OrientationMode.Landscape

        assertEquals(expectedOrientation, orientationMode)
    }

    @Test
    fun `WHEN screen configuration is of unknown value THEN an error is thrown`() {
        val screenConfiguration = 10

        assertThrows(InvalidParameterException::class.java) {
            OrientationMode.Companion.fromInteger(screenConfiguration)
        }
    }
}

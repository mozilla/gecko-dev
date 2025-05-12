/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.menu

import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.unit.IntRect
import org.junit.Assert.assertEquals
import org.junit.Test

private const val ANIMATION_ORIGIN_START_EDGE = 0f
private const val ANIMATION_ORIGIN_END_EDGE = 1f
private const val ANIMATION_ORIGIN_TOP_EDGE = 0f
private const val ANIMATION_ORIGIN_BOTTOM_EDGE = 1f
private const val ANIMATION_ORIGIN_CENTER = 0.5f

class CalculateTransformOriginTest {
    private val anchorBounds = IntRect(100, 100, 200, 200)

    @Test
    fun `GIVEN a popup aligned to center-right of anchor WHEN calculating the expand animation start point THEN return start center of popup`() {
        val popupBounds = IntRect(200, 125, 250, 175)
        val expectedOrigin = TransformOrigin(ANIMATION_ORIGIN_START_EDGE, ANIMATION_ORIGIN_CENTER)

        val result = calculateTransformOrigin(anchorBounds, popupBounds)

        assertEquals(expectedOrigin.pivotFractionX, result.pivotFractionX)
        assertEquals(expectedOrigin.pivotFractionY, result.pivotFractionY)
    }

    @Test
    fun `GIVEN a popup aligned to center-left of anchor WHEN calculating the expand animation start point THEN return right center of popup`() {
        val popupBounds = IntRect(50, 125, 100, 175)
        val expectedOrigin = TransformOrigin(ANIMATION_ORIGIN_END_EDGE, ANIMATION_ORIGIN_CENTER)

        val result = calculateTransformOrigin(anchorBounds, popupBounds)

        assertEquals(expectedOrigin.pivotFractionX, result.pivotFractionX)
        assertEquals(expectedOrigin.pivotFractionY, result.pivotFractionY)
    }

    @Test
    fun `GIVEN a popup aligned to bottom-center of anchor WHEN calculating the expand animation start point THEN return center top of popup`() {
        val popupBounds = IntRect(125, 200, 175, 250)
        val expectedOrigin = TransformOrigin(ANIMATION_ORIGIN_CENTER, ANIMATION_ORIGIN_TOP_EDGE)

        val result = calculateTransformOrigin(anchorBounds, popupBounds)

        assertEquals(expectedOrigin.pivotFractionX, result.pivotFractionX)
        assertEquals(expectedOrigin.pivotFractionY, result.pivotFractionY)
    }

    @Test
    fun `GIVEN a popup aligned to top-center of anchor WHEN calculating the expand animation start point THEN return center bottom of popup`() {
        val popupBounds = IntRect(125, 50, 175, 100)
        val expectedOrigin = TransformOrigin(ANIMATION_ORIGIN_CENTER, ANIMATION_ORIGIN_BOTTOM_EDGE)

        val result = calculateTransformOrigin(anchorBounds, popupBounds)

        assertEquals(expectedOrigin.pivotFractionX, result.pivotFractionX)
        assertEquals(expectedOrigin.pivotFractionY, result.pivotFractionY)
    }

    @Test
    fun `GIVEN a popup that overlaps top-left of anchor WHEN calculating the expand animation start point THEN return the bottom right corner of popup`() {
        val popupBounds = IntRect(50, 50, 150, 150)
        val expectedOrigin = TransformOrigin(0.75f, 0.75f)

        val result = calculateTransformOrigin(anchorBounds, popupBounds)

        assertEquals(expectedOrigin.pivotFractionX, result.pivotFractionX)
        assertEquals(expectedOrigin.pivotFractionY, result.pivotFractionY)
    }

    @Test
    fun `GIVEN a popup shown fully inside it's anchor WHEN calculating the expand animation start point THEN return the popup center`() {
        val anchorBounds = IntRect(100, 100, 300, 300)
        val popupBounds = IntRect(150, 150, 250, 250)
        val expectedOrigin = TransformOrigin(ANIMATION_ORIGIN_CENTER, ANIMATION_ORIGIN_CENTER)

        val result = calculateTransformOrigin(anchorBounds, popupBounds)

        assertEquals(expectedOrigin.pivotFractionX, result.pivotFractionX)
        assertEquals(expectedOrigin.pivotFractionY, result.pivotFractionY)
    }
}

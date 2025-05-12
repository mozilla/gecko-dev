/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.menu

import androidx.compose.ui.Alignment
import androidx.compose.ui.Alignment.Companion.CenterHorizontally
import androidx.compose.ui.Alignment.Companion.CenterVertically
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntRect
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import org.junit.Assert.assertEquals
import org.junit.Test
import kotlin.math.roundToInt

class CustomPopupPlacementPositionProviderTest {

    private val density = Density(density = 1f, fontScale = 1f)
    private var previouslyCalculatedPosition: Pair<IntRect, IntRect> = IntRect(0, 0, 0, 0) to IntRect(0, 0, 0, 0)
    private val onPositionsCalculated: (IntRect, IntRect) -> Unit = { anchorBounds, popupBounds ->
        previouslyCalculatedPosition = anchorBounds to popupBounds
    }
    private val windowSize = IntSize(width = 1000, height = 2000)
    private val topAnchorBounds = IntRect(left = 100, top = 200, right = 200, bottom = 300)
    private val bottomAnchorBounds = IntRect(left = 800, top = 1800, right = 900, bottom = 1900)
    private val popupSize = IntSize(width = 80, height = 50)
    private val largePopupSize = IntSize(width = 900, height = 1000)
    private val offset = DpOffset(4.dp, 5.dp)

    private fun createProvider(
        verticalAlignment: Alignment.Vertical = CenterVertically,
        horizontalAlignment: Alignment.Horizontal = CenterHorizontally,
        offset: DpOffset = this.offset,
        density: Density = this.density,
        onPositionCalculated: (IntRect, IntRect) -> Unit = onPositionsCalculated,
    ): CustomPopupPlacementPositionProvider {
        return CustomPopupPlacementPositionProvider(
            horizontalAlignment = horizontalAlignment,
            verticalAlignment = verticalAlignment,
            contentOffset = offset,
            density = density,
            onPositionCalculated = onPositionCalculated,
        )
    }

    @Test
    fun `GIVEN center alignment of a popup that fits in anchor WHEN placing the popup THEN show it inside the anchor`() {
        val provider = createProvider(
            verticalAlignment = CenterVertically,
            horizontalAlignment = CenterHorizontally,
        )
        val expectedX = topAnchorBounds.left + (topAnchorBounds.width - popupSize.width) / 2
        val expectedY = topAnchorBounds.top + (topAnchorBounds.height - popupSize.height) / 2
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = topAnchorBounds to IntRect(expectedOffset, popupSize)

        val result = provider.calculatePosition(topAnchorBounds, windowSize, LayoutDirection.Ltr, popupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }

    @Test
    fun `GIVEN TopStart alignment WHEN placing the popup THEN show it at the top-start of the anchor plus offset`() {
        val provider = createProvider(
            verticalAlignment = Alignment.Top,
            horizontalAlignment = Alignment.Start,
        )
        val expectedX = topAnchorBounds.left + offset.x.value.roundToInt()
        val expectedY = topAnchorBounds.top - popupSize.height + offset.y.value.roundToInt()
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = topAnchorBounds to IntRect(expectedOffset, popupSize)

        val result = provider.calculatePosition(topAnchorBounds, windowSize, LayoutDirection.Ltr, popupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }

    @Test
    fun `GIVEN TopEnd alignment WHEN placing the popup THEN show it at the top-end of the anchor plus offset`() {
        val provider = createProvider(
            verticalAlignment = Alignment.Top,
            horizontalAlignment = Alignment.End,
        )
        val expectedX = topAnchorBounds.right + this.offset.x.value.roundToInt()
        val expectedY = topAnchorBounds.top - popupSize.height + offset.y.value.roundToInt()
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = topAnchorBounds to IntRect(expectedOffset, popupSize)

        val result = provider.calculatePosition(topAnchorBounds, windowSize, LayoutDirection.Ltr, popupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }

    @Test
    fun `GIVEN BottomStart alignment WHEN placing the popup THEN position it at anchor's bottom-right plus offset`() {
        val provider = createProvider(
            verticalAlignment = Alignment.Bottom,
            horizontalAlignment = Alignment.Start,
        )
        val expectedX = topAnchorBounds.left + offset.x.value.roundToInt()
        val expectedY = topAnchorBounds.bottom + this.offset.y.value.roundToInt()
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = topAnchorBounds to IntRect(expectedOffset, popupSize)

        val result = provider.calculatePosition(topAnchorBounds, windowSize, LayoutDirection.Ltr, popupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }

    @Test
    fun `GIVEN BottomEnd alignment WHEN placing the popup THEN position it at anchor's bottom-right plus offset`() {
        val provider = createProvider(
            verticalAlignment = Alignment.Bottom,
            horizontalAlignment = Alignment.End,
        )
        val expectedX = topAnchorBounds.right + this.offset.x.value.roundToInt()
        val expectedY = topAnchorBounds.bottom + this.offset.y.value.roundToInt()
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = topAnchorBounds to IntRect(expectedOffset, popupSize)

        val result = provider.calculatePosition(topAnchorBounds, windowSize, LayoutDirection.Ltr, popupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }

    @Test
    fun `GIVEN center alignment of a popup that does not fit in anchor WHEN placing the popup THEN align it to anchor's top-start`() {
        val provider = createProvider(
            verticalAlignment = CenterVertically,
            horizontalAlignment = CenterHorizontally,
        )
        val expectedX = topAnchorBounds.left
        val expectedY = topAnchorBounds.top
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = topAnchorBounds to IntRect(expectedOffset, largePopupSize)

        val result = provider.calculatePosition(topAnchorBounds, windowSize, LayoutDirection.Ltr, largePopupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }

    @Test
    fun `GIVEN TopEnd alignment of a popup that does not fit the screen WHEN placing the popup THEN align it to the start of the screen and anchor bottom`() {
        val provider = createProvider(
            verticalAlignment = Alignment.Top,
            horizontalAlignment = Alignment.End,
        )
        val expectedX = 0
        val expectedY = topAnchorBounds.bottom + offset.y.value.roundToInt()
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = topAnchorBounds to IntRect(expectedOffset, largePopupSize)

        val result = provider.calculatePosition(topAnchorBounds, windowSize, LayoutDirection.Ltr, largePopupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }

    @Test
    fun `GIVEN BottomStart alignment of a popup that does not fit the screen WHEN placing the popup THEN align the right and bottom to anchor top`() {
        val provider = createProvider(
            verticalAlignment = Alignment.Bottom,
            horizontalAlignment = Alignment.Start,
        )
        val expectedX = bottomAnchorBounds.right - largePopupSize.width + offset.x.value.roundToInt()
        val expectedY = bottomAnchorBounds.top - largePopupSize.height + offset.y.value.roundToInt()
        val expectedOffset = IntOffset(expectedX, expectedY)
        val expectedPositions = bottomAnchorBounds to IntRect(expectedOffset, largePopupSize)

        val result = provider.calculatePosition(bottomAnchorBounds, windowSize, LayoutDirection.Ltr, largePopupSize)

        assertEquals(expectedOffset, result)
        assertEquals(expectedPositions, previouslyCalculatedPosition)
    }
}

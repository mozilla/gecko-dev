/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.menu

import androidx.annotation.VisibleForTesting
import androidx.compose.animation.core.MutableTransitionState
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.rememberTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.LayoutScopeMarker
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Immutable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Alignment.Companion.CenterHorizontally
import androidx.compose.ui.Alignment.Companion.CenterVertically
import androidx.compose.ui.Alignment.Companion.Start
import androidx.compose.ui.Alignment.Companion.Top
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.GraphicsLayerScope
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntRect
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.LayoutDirection.Ltr
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Popup
import androidx.compose.ui.window.PopupPositionProvider
import androidx.compose.ui.window.PopupProperties
import kotlin.math.max
import kotlin.math.min

private const val CORNER_RADIUS = 8
private const val ELEVATION = 6
private const val ANIMATION_DURATION_MS = 200
private const val ANIMATION_ORIGIN_START_EDGE = 0f
private const val ANIMATION_ORIGIN_END_EDGE = 1f
private const val ANIMATION_ORIGIN_TOP_EDGE = 0f
private const val ANIMATION_ORIGIN_BOTTOM_EDGE = 1f
private const val FULLY_VISIBLE_POPUP_ANIMATION_RATIO = 1f
private const val FULLY_HIDDEN_POPUP_ANIMATION_RATIO = 0f

/**
 * Layout scope for content that can be shown inside of [CustomPlacementPopup].
 */
@LayoutScopeMarker
@Immutable
object CustomPlacementPopup

/**
 * Composable for displaying a custom popup.
 *
 * @param isVisible Whether the popup is currently visible.
 * @param onDismissRequest Callback to be invoked when the popup is dismissed.
 * @param horizontalAlignment 1D horizontal alignment bias of the popup's content.
 * @param verticalAlignment 1D vertical alignment bias of the popup's content.
 * @param offset The [DpOffset] to apply to the popup's position.
 * This is not used when based on [horizontalAlignment] or [verticalAlignment] the popup should be centered.
 * @param properties The [PopupProperties] used to configure the popup.
 * @param content The composable to be shown inside this popup.
 */
@Composable
fun CustomPlacementPopup(
    isVisible: Boolean,
    onDismissRequest: () -> Unit,
    horizontalAlignment: Alignment.Horizontal = CenterHorizontally,
    verticalAlignment: Alignment.Vertical = CenterVertically,
    offset: DpOffset = DpOffset(0.dp, 0.dp),
    properties: PopupProperties = PopupProperties(focusable = true),
    content: @Composable CustomPlacementPopup.() -> Unit,
) {
    val visibilityStates = remember { MutableTransitionState(false) }
    visibilityStates.targetState = isVisible

    if (visibilityStates.currentState || visibilityStates.targetState) {
        val density = LocalDensity.current

        val transformOriginState = remember { mutableStateOf(TransformOrigin.Center) }
        val popupPositionProvider = CustomPopupPlacementPositionProvider(
            horizontalAlignment = horizontalAlignment,
            verticalAlignment = verticalAlignment,
            contentOffset = offset,
            density = density,
            onPositionCalculated = { parentBounds, menuBounds ->
                transformOriginState.value = calculateTransformOrigin(parentBounds, menuBounds)
            },
        )

        Popup(
            onDismissRequest = onDismissRequest,
            popupPositionProvider = popupPositionProvider,
            properties = properties,
        ) {
            CustomPlacementPopupContent(
                expandedStates = visibilityStates,
                transformOrigin = transformOriginState.value,
                content = content,
            )
        }
    }
}

/**
 * Compute where the popup should appear to expand from, relative to its parent or anchor.
 *
 * @param parentBounds The window relative bounds of the layout which this popup is anchored to.
 * @param menuBounds The window relative bounds of the the actual popup.
 */
@VisibleForTesting
internal fun calculateTransformOrigin(
    parentBounds: IntRect,
    menuBounds: IntRect,
): TransformOrigin {
    val pivotX = when {
        menuBounds.left >= parentBounds.right -> ANIMATION_ORIGIN_START_EDGE
        menuBounds.right <= parentBounds.left -> ANIMATION_ORIGIN_END_EDGE
        menuBounds.width == 0 -> ANIMATION_ORIGIN_START_EDGE
        else -> {
            val intersectionCenter = (
                max(parentBounds.left, menuBounds.left) +
                    min(parentBounds.right, menuBounds.right)
                ) / 2
            (intersectionCenter - menuBounds.left).toFloat() / menuBounds.width
        }
    }
    val pivotY = when {
        menuBounds.top >= parentBounds.bottom -> ANIMATION_ORIGIN_TOP_EDGE
        menuBounds.bottom <= parentBounds.top -> ANIMATION_ORIGIN_BOTTOM_EDGE
        menuBounds.height == 0 -> ANIMATION_ORIGIN_TOP_EDGE
        else -> {
            val intersectionCenter = (
                max(parentBounds.top, menuBounds.top) +
                    min(parentBounds.bottom, menuBounds.bottom)
                ) / 2
            (intersectionCenter - menuBounds.top).toFloat() / menuBounds.height
        }
    }
    return TransformOrigin(pivotX, pivotY)
}

/**
 * Popup with custom animations for appearance and disappearance.
 *
 * @param expandedStates [MutableTransitionState] that determines whether the popup is expanding or shrinking.
 * @param transformOrigin [TransformOrigin] determining from which position the popup (dis)appears from.
 * @param content Content that will be displayed within the popup.
 */
@Composable
private fun CustomPlacementPopupContent(
    expandedStates: MutableTransitionState<Boolean>,
    transformOrigin: TransformOrigin,
    content: @Composable CustomPlacementPopup.() -> Unit,
) {
    val expandingMenuState = rememberTransition(expandedStates)

    val scale by expandingMenuState.animateFloat(
        transitionSpec = {
            if (false isTransitioningTo true) {
                tween(durationMillis = ANIMATION_DURATION_MS)
            } else {
                tween(durationMillis = ANIMATION_DURATION_MS)
            }
        },
    ) { isExpanding ->
        when (isExpanding) {
            true -> FULLY_VISIBLE_POPUP_ANIMATION_RATIO
            false -> FULLY_HIDDEN_POPUP_ANIMATION_RATIO
        }
    }

    val alpha by expandingMenuState.animateFloat(
        transitionSpec = {
            if (false isTransitioningTo true) {
                tween(durationMillis = ANIMATION_DURATION_MS)
            } else {
                tween(durationMillis = ANIMATION_DURATION_MS)
            }
        },
    ) { isExpanding ->
        when (isExpanding) {
            true -> FULLY_VISIBLE_POPUP_ANIMATION_RATIO
            false -> FULLY_HIDDEN_POPUP_ANIMATION_RATIO
        }
    }

    fun GraphicsLayerScope.graphicsLayerAnim() {
        scaleX = scale
        scaleY = scale
        this.alpha = alpha
        this.transformOrigin = transformOrigin
    }

    Card(
        shape = RoundedCornerShape(CORNER_RADIUS.dp),
        modifier = Modifier.graphicsLayer { graphicsLayerAnim() },
        elevation = CardDefaults.cardElevation(defaultElevation = ELEVATION.dp),
    ) {
        CustomPlacementPopup.content()
    }
}

/**
 * Custom [PopupPositionProvider] implementation for positioning a popup relative to an anchor.
 *
 * This class provides a flexible way to calculate the position of a popup menu, allowing to specify
 * an alignment bias and taking into account the anchor's position and size, the window's size, and a content offset.
 *
 * @property horizontalAlignment The horizontal alignment bias of the popup's content.
 * The popup might not always respect this if it cannot fit in the available space.
 * @property verticalAlignment The vertical alignment bias of the popup's content.
 * The popup might not always respect this if it cannot fit in the available space.
 * @property contentOffset The offset to apply to the popup's position relative to the anchor.
 * @property density The screen density used for converting between DP and pixel values.
 * @property onPositionCalculated A callback for when a suitable popup position is found through which
 * the integrator will be informed about the anchor and the popup bounds in the window's coordinate system.
 */
@Immutable
@VisibleForTesting
internal data class CustomPopupPlacementPositionProvider(
    val horizontalAlignment: Alignment.Horizontal = CenterHorizontally,
    val verticalAlignment: Alignment.Vertical = CenterVertically,
    val contentOffset: DpOffset = DpOffset.Zero,
    val density: Density,
    val onPositionCalculated: (IntRect, IntRect) -> Unit = { _, _ -> },
) : PopupPositionProvider {
    override fun calculatePosition(
        anchorBounds: IntRect,
        windowSize: IntSize,
        layoutDirection: LayoutDirection,
        popupContentSize: IntSize,
    ): IntOffset {
        val popupOffsetPx = with(density) {
            IntOffset(contentOffset.x.roundToPx(), contentOffset.y.roundToPx())
        }
        val x = computePopupXCoord(
            horizontalAlignment,
            windowSize,
            anchorBounds,
            popupContentSize,
            popupOffsetPx,
            layoutDirection,
        )
        val y = computePopupYCoord(
            verticalAlignment,
            windowSize,
            anchorBounds,
            popupContentSize,
            popupOffsetPx,
        )

        onPositionCalculated(
            anchorBounds,
            IntRect(x, y, x + popupContentSize.width, y + popupContentSize.height),
        )
        return IntOffset(x, y)
    }
}

/**
 * Compute the Y coordinate of the popup.
 *
 * @param wantedAlignment The vertical alignment bias of the popup's content.
 * The result might not always respect this if it cannot fit in the available space.
 * @param windowSize The size of the window containing the anchor layout.
 * @param anchorBounds The window relative bounds of the layout which this popup is anchored to.
 * @param popupContentSize The size of the popup's content.
 * @param contentOffset The vertical offset of the popup from the anchor.
 */
@Suppress("ReturnCount")
private fun computePopupYCoord(
    wantedAlignment: Alignment.Vertical,
    windowSize: IntSize,
    anchorBounds: IntRect,
    popupContentSize: IntSize,
    contentOffset: IntOffset,
): Int {
    // Center in anchor a popup that completely fits
    if (wantedAlignment == CenterVertically && anchorBounds.height > popupContentSize.height) {
        return anchorBounds.top + (anchorBounds.height - popupContentSize.height) / 2
    }

    // Or center just it's start inside a smaller anchor
    val maxWindowBottom = maxOf(windowSize.height, anchorBounds.bottom)
    if (wantedAlignment == CenterVertically) {
        val bottomToAnchorCenter = anchorBounds.bottom - popupContentSize.height
        if (bottomToAnchorCenter >= 0) {
            return bottomToAnchorCenter
        }

        val topToAnchorCenter = anchorBounds.top
        if (topToAnchorCenter <= maxWindowBottom) {
            return topToAnchorCenter
        }
    }

    val topToAnchorBottom = anchorBounds.bottom + contentOffset.y
    val bottomToAnchorTop = anchorBounds.top - popupContentSize.height + contentOffset.y
    val centerToAnchorTop = anchorBounds.bottom - anchorBounds.height / 2 - popupContentSize.height + contentOffset.y
    val bottomToWindowBottom = maxWindowBottom - popupContentSize.height
    val topToWindowTop = 0

    // Or anchor it in the available space with bias towards the wanted alignment
    return if (wantedAlignment == CenterVertically) {
        sequenceOf(
            bottomToAnchorTop,
            topToAnchorBottom,
            topToWindowTop,
        ).firstOrNull {
            it >= 0 && it + popupContentSize.height <= maxWindowBottom
        } ?: bottomToWindowBottom
    } else if (wantedAlignment == Top) {
        sequenceOf(
            bottomToAnchorTop,
            topToAnchorBottom,
            centerToAnchorTop,
            topToWindowTop,
        ).firstOrNull {
            it >= 0 && it + popupContentSize.height <= maxWindowBottom
        } ?: bottomToWindowBottom
    } else {
        sequenceOf(
            topToAnchorBottom,
            bottomToAnchorTop,
            centerToAnchorTop,
            bottomToWindowBottom,
        ).firstOrNull {
            it >= 0 && it + popupContentSize.height <= maxWindowBottom
        } ?: topToWindowTop
    }
}

/**
 * Compute the X coordinate of the popup.
 *
 * @param wantedAlignment The horizontal alignment bias of the popup's content.
 * The result might not always respect this if it cannot fit in the available space.
 * @param windowSize The size of the window containing the anchor layout.
 * @param anchorBounds The window relative bounds of the layout which this popup is anchored to.
 * @param popupContentSize The size of the popup's content.
 * @param contentOffset The vertical offset of the popup from the anchor.
 * @param layoutDirection The layout direction of the anchor layout.
 */
@Suppress("ComplexMethod", "ReturnCount")
private fun computePopupXCoord(
    wantedAlignment: Alignment.Horizontal,
    windowSize: IntSize,
    anchorBounds: IntRect,
    popupContentSize: IntSize,
    contentOffset: IntOffset,
    layoutDirection: LayoutDirection,
): Int {
    // Center in anchor a popup that completely fits
    if (wantedAlignment == CenterHorizontally && anchorBounds.width > popupContentSize.width) {
        return anchorBounds.left + (anchorBounds.width - popupContentSize.width) / 2
    }

    // Or center just it's start inside a smaller anchor
    if (wantedAlignment == CenterHorizontally) {
        val leftToAnchorCenter = anchorBounds.left
        if (leftToAnchorCenter + popupContentSize.width <= windowSize.width) {
            return leftToAnchorCenter
        }

        val rightToAnchorCenter = anchorBounds.right - popupContentSize.width
        if (rightToAnchorCenter >= 0) {
            return rightToAnchorCenter
        }
    }

    val leftToAnchorLeft = anchorBounds.left + contentOffset.x
    val leftToAnchorRight = anchorBounds.right + contentOffset.x
    val rightToAnchorRight = anchorBounds.right - popupContentSize.width + contentOffset.x
    val rightToAnchorLeft = anchorBounds.left - popupContentSize.width + contentOffset.x
    val rightToWindowRight = windowSize.width - popupContentSize.width
    val leftToWindowLeft = 0

    // Or anchor it in the available space with bias towards the wanted alignment
    return if (wantedAlignment == Start || wantedAlignment == CenterHorizontally) {
        if (layoutDirection == Ltr) {
            sequenceOf(
                leftToAnchorLeft,
                rightToAnchorRight,
                if (anchorBounds.left >= 0) rightToWindowRight else leftToWindowLeft,
            )
        } else {
            sequenceOf(
                rightToAnchorRight,
                leftToAnchorLeft,
                if (anchorBounds.right <= windowSize.width) leftToWindowLeft else rightToWindowRight,
            )
        }.firstOrNull {
            it >= 0 && it + popupContentSize.width <= windowSize.width
        } ?: leftToWindowLeft
    } else {
        if (layoutDirection == Ltr) {
            sequenceOf(
                leftToAnchorRight,
                leftToAnchorLeft,
                if (anchorBounds.right <= windowSize.width) leftToWindowLeft else rightToWindowRight,
            )
        } else {
            sequenceOf(
                rightToAnchorLeft,
                leftToAnchorRight,
                if (anchorBounds.left >= 0) rightToWindowRight else leftToWindowLeft,
            )
        }.firstOrNull {
            it >= 0 && it + popupContentSize.width <= windowSize.width
        } ?: rightToWindowRight
    }
}

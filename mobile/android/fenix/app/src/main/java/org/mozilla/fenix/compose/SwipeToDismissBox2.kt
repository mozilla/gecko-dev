/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.animation.core.DecayAnimationSpec
import androidx.compose.animation.core.tween
import androidx.compose.animation.rememberSplineBasedDecay
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.AnchoredDraggableState
import androidx.compose.foundation.gestures.DraggableAnchors
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.awaitHorizontalTouchSlopOrCancellation
import androidx.compose.foundation.gestures.horizontalDrag
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.input.pointer.positionChangeIgnoreConsumed
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.compose.snackbar.AcornSnackbarHostState
import org.mozilla.fenix.compose.snackbar.SnackbarHost
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.abs
import kotlin.math.roundToInt

/**
 * The distance an item has to be swiped before it is considered dismissed.
 */
private val DISMISS_THRESHOLD_DP = 90.dp

/**
 * The velocity (in DP per second) the item has to exceed in order to animate to the next state,
 * even if the [AnchoredDraggableState.positionalThreshold] has not been reached.
 */
private val VELOCITY_THRESHOLD_DP = 125.dp

/**
 * The length of time the swipe gesture will animate for after being initiated by the user.
 */
private const val SWIPE_ANIMATION_DURATION_MS = 230

/**
 * The swipe gesture directions.
 */
enum class SwipeToDismissDirections {
    /**
     * Can be dismissed by swiping in the reading direction.
     */
    StartToEnd,

    /**
     * Can be dismissed by swiping in the reverse of the reading direction.
     */
    EndToStart,

    /**
     *  Cannot currently be dismissed.
     */
    Settled,
}

/**
 * The UI state for [SwipeToDismissBox2].
 *
 * @param density [Density] used to derive the underlying [AnchoredDraggableState.velocityThreshold].
 * @param decayAnimationSpec [DecayAnimationSpec] used to specify the animation parameters.
 * @property enabled Whether the swipe gesture is enabled.
 */
@OptIn(ExperimentalFoundationApi::class)
class SwipeToDismissState2(
    density: Density,
    decayAnimationSpec: DecayAnimationSpec<Float>,
    val enabled: Boolean = true,
) {

    /**
     * [AnchoredDraggableState] for the underlying [Modifier.anchoredHorizontalDraggable].
     */
    val anchoredDraggableState: AnchoredDraggableState<SwipeToDismissDirections> = AnchoredDraggableState(
        initialValue = SwipeToDismissDirections.Settled,
        positionalThreshold = with(density) { { DISMISS_THRESHOLD_DP.toPx() } },
        velocityThreshold = { with(density) { VELOCITY_THRESHOLD_DP.toPx() } },
        snapAnimationSpec = tween(
            durationMillis = SWIPE_ANIMATION_DURATION_MS,
        ),
        decayAnimationSpec = decayAnimationSpec,
    )

    /**
     * Whether there is a swipe gesture in-progress.
     */
    val swipingActive: Boolean
        get() = !anchoredDraggableState.offset.isNaN() && anchoredDraggableState.offset != 0f

    /**
     * The [SwipeToDismissAnchor] the swipe gesture is targeting.
     */
    private val swipeDestination: SwipeToDismissDirections
        get() = anchoredDraggableState.anchors.closestAnchor(
            position = anchoredDraggableState.offset,
            searchUpwards = anchoredDraggableState.offset > 0,
        ) ?: SwipeToDismissDirections.Settled

    /**
     * Whether the swipe gesture is in the start direction.
     */
    val isSwipingToStart: Boolean
        get() = swipeDestination == SwipeToDismissDirections.EndToStart

    /**
     * The current [IntOffset] of the swipe. If the X-offset is currently [Float.NaN], it will return 0.
     */
    val safeSwipeOffset: IntOffset
        get() {
            val xOffset = if (anchoredDraggableState.offset.isNaN()) {
                0
            } else {
                anchoredDraggableState.offset.roundToInt()
            }

            return IntOffset(x = xOffset, y = 0)
        }
}

@OptIn(ExperimentalFoundationApi::class)
private fun Modifier.anchoredHorizontalDraggable(
    state: SwipeToDismissState2,
    scope: CoroutineScope,
) = pointerInput(Unit) {
    if (state.enabled) {
        awaitEachGesture {
            val down = awaitFirstDown(requireUnconsumed = false)
            var overSlop = 0f
            var validDrag = false
            val drag =
                awaitHorizontalTouchSlopOrCancellation(down.id) { change, over ->
                    val posChange = change.positionChangeIgnoreConsumed()
                    validDrag = isReallyHorizontal(posChange.x, posChange.y)
                    if (validDrag) {
                        change.consume()
                        overSlop = over
                    }
                }
            if (drag != null && validDrag) {
                state.anchoredDraggableState.dispatchRawDelta(overSlop)
                horizontalDrag(drag.id) {
                    state.anchoredDraggableState.dispatchRawDelta(it.positionChange().x)
                    it.consume()
                }
                scope.launch {
                    state.anchoredDraggableState.settle(state.anchoredDraggableState.lastVelocity)
                }
            }
        }
    }
}

@Suppress("MagicNumber")
private fun isReallyHorizontal(x: Float, y: Float) =
    abs(x) > 3 * abs(y) // max ~18 degrees from horizontal axis

/**
 * A container that can be dismissed by swiping left or right.
 *
 * @param state [SwipeToDismissState] containing the UI state of [SwipeToDismissBox].
 * @param modifier Optional [Modifier] for this component.
 * @param enableDismissFromStartToEnd Whether SwipeToDismissBox can be dismissed from start to end.
 * @param enableDismissFromEndToStart Whether SwipeToDismissBox can be dismissed from end to start.
 * @param onItemDismiss Invoked when the item is dismissed.
 * @param backgroundContent A composable that is stacked behind the primary content and is exposed
 * when the content is swiped. You can/should use the [state] to have different backgrounds on each side.
 * @param dismissContent The content that can be dismissed.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun SwipeToDismissBox2(
    state: SwipeToDismissState2,
    modifier: Modifier = Modifier,
    enableDismissFromStartToEnd: Boolean = true,
    enableDismissFromEndToStart: Boolean = true,
    onItemDismiss: () -> Unit,
    backgroundContent: @Composable BoxScope.() -> Unit,
    dismissContent: @Composable BoxScope.() -> Unit,
) {
    val isRtl = LocalLayoutDirection.current == LayoutDirection.Rtl
    var width by remember { mutableFloatStateOf(0f) }
    val anchors = remember(width) {
        DraggableAnchors {
            if (enableDismissFromEndToStart) {
                SwipeToDismissDirections.EndToStart at (if (isRtl) width else -width)
            }
            if (enableDismissFromStartToEnd) {
                SwipeToDismissDirections.StartToEnd at (if (isRtl) -width else width)
            }
            SwipeToDismissDirections.Settled at 0f
        }
    }

    LaunchedEffect(state.anchoredDraggableState.currentValue) {
        val value = state.anchoredDraggableState.currentValue
        when (value) {
            SwipeToDismissDirections.StartToEnd, SwipeToDismissDirections.EndToStart -> {
                onItemDismiss()
            }
            SwipeToDismissDirections.Settled -> {} // no-op
        }
    }

    SideEffect {
        state.anchoredDraggableState.updateAnchors(newAnchors = anchors)
    }

    Box(
        modifier = Modifier
            .anchoredHorizontalDraggable(
                state = state,
                scope = rememberCoroutineScope(),
            )
            .onSizeChanged { size ->
                width = size.width.toFloat()
            }
            .then(modifier),
    ) {
        Box(
            modifier = Modifier.matchParentSize(),
            content = backgroundContent,
        )

        Box(
            modifier = Modifier.offset { state.safeSwipeOffset },
            content = dismissContent,
        )
    }
}

@Composable
@Preview
@Preview(locale = "ar", name = "RTL")
private fun SwipeToDismissBoxPreview() {
    val snackbarState = remember { AcornSnackbarHostState() }
    val coroutineScope = rememberCoroutineScope()

    FirefoxTheme {
        Box(
            modifier = Modifier.fillMaxSize(),
        ) {
            Column {
                SwipeableItem(
                    text = "Swipe to right ->",
                    enableDismissFromEndToStart = false,
                    onSwipeToEnd = {
                        coroutineScope.launch {
                            snackbarState.showSnackbar(SnackbarState(message = "Dismiss"))
                        }
                    },
                )

                Spacer(Modifier.height(30.dp))

                SwipeableItem(
                    enableDismissFromStartToEnd = false,
                    text = "<- Swipe to left",
                    onSwipeToStart = {
                        coroutineScope.launch {
                            snackbarState.showSnackbar(SnackbarState(message = "Dismiss"))
                        }
                    },
                )

                Spacer(Modifier.height(30.dp))

                SwipeableItem(
                    text = "<- Swipe both ways ->",
                    onSwipeToStart = {
                        coroutineScope.launch {
                            snackbarState.showSnackbar(SnackbarState(message = "Dismiss"))
                        }
                    },
                    onSwipeToEnd = {
                        coroutineScope.launch {
                            snackbarState.showSnackbar(SnackbarState(message = "Dismiss"))
                        }
                    },
                )
            }

            SnackbarHost(
                snackbarHostState = snackbarState,
                modifier = Modifier.align(Alignment.BottomCenter),
            )
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun SwipeableItem(
    text: String,
    enableDismissFromStartToEnd: Boolean = true,
    enableDismissFromEndToStart: Boolean = true,
    onSwipeToStart: () -> Unit = {},
    onSwipeToEnd: () -> Unit = {},
) {
    val density = LocalDensity.current
    val decayAnimationSpec: DecayAnimationSpec<Float> = rememberSplineBasedDecay()

    val swipeState = remember {
        SwipeToDismissState2(
            density = density,
            decayAnimationSpec = decayAnimationSpec,
        )
    }

    SwipeToDismissBox2(
        state = swipeState,
        modifier = Modifier
            .height(30.dp)
            .fillMaxWidth(),
        enableDismissFromStartToEnd = enableDismissFromStartToEnd,
        enableDismissFromEndToStart = enableDismissFromEndToStart,
        onItemDismiss = {
            if (swipeState.anchoredDraggableState.currentValue == SwipeToDismissDirections.EndToStart) {
                onSwipeToStart()
            } else {
                onSwipeToEnd()
            }
        },
        backgroundContent = {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(FirefoxTheme.colors.layerAccent),
            )
        },
    ) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .background(FirefoxTheme.colors.layer1),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(text)
        }
    }
}

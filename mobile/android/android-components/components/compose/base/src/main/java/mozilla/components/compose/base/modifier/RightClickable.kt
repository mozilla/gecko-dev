/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.modifier

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Indication
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.indication
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.PressInteraction
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.isSecondaryPressed
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.util.fastAny
import androidx.compose.ui.util.fastForEach

/**
 * Configure component to receive right/secondary-button clicks.
 *
 * @param interactionSource [MutableInteractionSource] that will be used to emit
 * [PressInteraction.Press] when this clickable is pressed.
 * @param indication indication to be shown when modified element is pressed. Pass `null` to show no indication, or
 * current value from [LocalIndication] to show theme default
 * @param enabled Whether or not the component will react to mouse right clicks.
 * @param onRightClick Invoked when the user right clicks on the component.
 */
fun Modifier.rightClickable(
    interactionSource: MutableInteractionSource,
    indication: Indication?,
    enabled: Boolean = true,
    onRightClick: () -> Unit,
): Modifier = this.then(
    Modifier
        .indication(
            interactionSource = interactionSource,
            indication = indication,
        )
        .pointerInput(enabled) {
            if (enabled) {
                awaitEachGesture {
                    val event = awaitPointerEvent()
                    if (event.buttons.isSecondaryPressed) {
                        onRightClick.invoke()
                        event.changes.fastForEach { it.consume() }

                        val pressInteraction = PressInteraction.Press(event.changes[0].position)
                        interactionSource.tryEmit(pressInteraction)

                        do {
                            val event2 = awaitPointerEvent()
                            event2.changes.fastForEach { it.consume() }
                        } while (event2.changes.fastAny { it.pressed })

                        interactionSource.tryEmit(PressInteraction.Release(pressInteraction))
                    }
                }
            }
        },
)

@OptIn(ExperimentalFoundationApi::class)
@Preview
@Composable
private fun RightClickablePreview() {
    val interactionSource = remember { MutableInteractionSource() }
    var leftClickCount by remember { mutableIntStateOf(0) }
    var rightClickCount by remember { mutableIntStateOf(0) }
    var longLeftClickCount by remember { mutableIntStateOf(0) }

    MaterialTheme {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(color = Color.White),
        ) {
            Box(
                modifier = Modifier
                    .background(color = Color.Yellow)
                    .size(size = 200.dp)
                    .combinedClickable(
                        interactionSource = interactionSource,
                        indication = LocalIndication.current,
                        onClick = {
                            leftClickCount++
                        },
                        onLongClick = {
                            longLeftClickCount++
                        },
                    )
                    .rightClickable(
                        interactionSource = interactionSource,
                        indication = LocalIndication.current,
                        onRightClick = {
                            rightClickCount++
                        },
                    ),
                contentAlignment = Alignment.Center,
            ) {
                Text(text = "Click me")
            }

            Spacer(modifier = Modifier.size(20.dp))

            Text(text = "Right clicks: $rightClickCount")

            Spacer(modifier = Modifier.size(20.dp))

            Text(text = "Left clicks: $leftClickCount")

            Spacer(modifier = Modifier.size(20.dp))

            Text(text = "Long left clicks: $longLeftClickCount")
        }
    }
}

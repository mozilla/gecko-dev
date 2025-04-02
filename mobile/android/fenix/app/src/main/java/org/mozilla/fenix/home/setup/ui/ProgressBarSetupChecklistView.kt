/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.ui.colors.PhotonColors
import org.mozilla.fenix.theme.FirefoxTheme

private val heightProgressBarChecklist = 12.dp
private val shapeProgressBarChecklist = RoundedCornerShape(
    topStartPercent = 50,
    topEndPercent = 50,
    bottomEndPercent = 50,
    bottomStartPercent = 50,
)

/**
 * The progress bar for checklist tasks.
 *
 * @param numberOfTasks The total of tasks that can be completed.
 * @param numberOfTasksCompleted The number of tasks completed.
 */
@Composable
fun ProgressBarSetupChecklistView(numberOfTasks: Int, numberOfTasksCompleted: Int) {
    Box(modifier = Modifier.background(FirefoxTheme.colors.layer1)) {
        ProgressBarBackground()

        ProgressBarCompleted(numberOfTasks, numberOfTasksCompleted)

        ProgressBarSegmentation(numberOfTasks)
    }
}

/**
 * The gray background for progressbar.
 */
@Composable
private fun ProgressBarBackground() {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(heightProgressBarChecklist)
            .clip(shapeProgressBarChecklist)
            .background(FirefoxTheme.colors.borderDisabled),
    ) {}
}

/**
 * The progress bar with the colored gradient for checklist tasks.
 */
@Composable
private fun ProgressBarCompleted(numberOfTasks: Int, numberOfTasksCompleted: Int) {
    val progress by remember(numberOfTasksCompleted) {
        mutableFloatStateOf(numberOfTasksCompleted.toFloat() / numberOfTasks.toFloat())
    }

    val gradient = Brush.horizontalGradient(
        colors = listOf(
            Color(PhotonColors.Violet50.value),
            Color(PhotonColors.Pink40.value),
            Color(PhotonColors.Yellow40.value),
        ),
    )

    var shape = shapeProgressBarChecklist

    if (numberOfTasksCompleted < numberOfTasks) {
        shape = RoundedCornerShape(
            topStartPercent = 50,
            bottomStartPercent = 50,
        )
    }

    Row(
        modifier = Modifier
            .fillMaxWidth(progress)
            .height(heightProgressBarChecklist)
            .clip(shape)
            .background(brush = gradient),
    ) {}
}

/**
 * The layer to create the segmented visual.
 */
@Composable
private fun ProgressBarSegmentation(numberOfTasks: Int) {
    Row(modifier = Modifier.height(heightProgressBarChecklist)) {
        for (task in 1..numberOfTasks) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(heightProgressBarChecklist)
                    .background(Color.Transparent)
                    .weight(1f),
            ) {}

            if (task != numberOfTasks) {
                Row(
                    modifier = Modifier
                        .height(heightProgressBarChecklist)
                        .width(4.dp)
                        .background(FirefoxTheme.colors.layer1),
                ) {}
            }
        }
    }
}

@Suppress("MagicNumber")
@FlexibleWindowLightDarkPreview
@Composable
private fun PreviewProgressIndicatorSetupChecklist() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(16.dp),
        ) {
            ProgressBarSetupChecklistView(6, 3)
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:OptIn(ExperimentalMaterialApi::class)

package org.mozilla.fenix.compose

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.ExperimentalMaterialApi
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults.filterChipColors
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Default layout of a selectable chip.
 *
 * @param text [String] displayed in this chip.
 * @param isSelected Whether this should be shown as selected.
 * @param modifier [Modifier] used to be applied to the layout of the chip.
 * @param selectableChipColors The color set defined by [SelectableChipColors] used to style the chip.
 * @param onClick Callback for when the user taps this chip.
 */
@Composable
fun SelectableChip(
    text: String,
    isSelected: Boolean,
    modifier: Modifier = Modifier,
    selectableChipColors: SelectableChipColors = SelectableChipColors.buildColors(),
    onClick: () -> Unit,
) {
    FilterChip(
        selected = isSelected,
        modifier = modifier,
        onClick = onClick,
        label = {
            Text(
                text = text,
                color = FirefoxTheme.colors.textPrimary,
                style = if (isSelected) FirefoxTheme.typography.headline8 else FirefoxTheme.typography.body2,
            )
        },
        leadingIcon = if (isSelected) {
            {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_checkmark_16),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        } else {
            null
        },
        colors = selectableChipColors.toMaterialChipColors(),
        shape = RoundedCornerShape(16.dp),
        border = if (isSelected) {
            null
        } else {
            BorderStroke(width = 1.dp, color = selectableChipColors.borderColor)
        },
    )
}

/**
 * Wrapper for the color parameters of [SelectableChip].
 *
 * @property selectedContainerColor Background [Color] when the chip is selected.
 * @property containerColor Background [Color] when the chip is not selected.
 * @property selectedLabelColor Text [Color] when the chip is selected.
 * @property labelColor Text [Color] when the chip is not selected.
 * @property borderColor Border [Color] for the chip.
 */
data class SelectableChipColors(
    val selectedContainerColor: Color,
    val containerColor: Color,
    val selectedLabelColor: Color,
    val labelColor: Color,
    val borderColor: Color,
) {
    companion object {

        /**
         * Builder function used to construct an instance of [SelectableChipColors].
         */
        @Composable
        fun buildColors(
            selectedContainerColor: Color = FirefoxTheme.colors.actionChipSelected,
            containerColor: Color = FirefoxTheme.colors.layer1,
            selectedLabelColor: Color = FirefoxTheme.colors.textPrimary,
            labelColor: Color = FirefoxTheme.colors.textPrimary,
            borderColor: Color = FirefoxTheme.colors.borderPrimary,
        ) = SelectableChipColors(
            selectedContainerColor = selectedContainerColor,
            containerColor = containerColor,
            selectedLabelColor = selectedLabelColor,
            labelColor = labelColor,
            borderColor = borderColor,
        )
    }
}

/**
 * Map applications' colors for selectable chips to the platform type.
 */
@Composable
private fun SelectableChipColors.toMaterialChipColors() = filterChipColors(
    selectedContainerColor = selectedContainerColor,
    containerColor = containerColor,
    selectedLabelColor = selectedLabelColor,
    labelColor = labelColor,
)

@Composable
@PreviewLightDark
private fun SelectableChipPreview() {
    FirefoxTheme {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(FirefoxTheme.colors.layer1),
            horizontalArrangement = Arrangement.SpaceEvenly,
        ) {
            SelectableChip(text = "ChirpOne", isSelected = false) {}
            SelectableChip(text = "ChirpTwo", isSelected = true) {}
        }
    }
}

@Composable
@PreviewLightDark
private fun SelectableChipWithCustomColorsPreview() {
    FirefoxTheme {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(FirefoxTheme.colors.layer1),
            horizontalArrangement = Arrangement.SpaceEvenly,
        ) {
            SelectableChip(
                text = "Yellow",
                isSelected = false,
                selectableChipColors = SelectableChipColors(
                    selectedContainerColor = Color.Yellow,
                    containerColor = Color.DarkGray,
                    selectedLabelColor = Color.Black,
                    labelColor = Color.Gray,
                    borderColor = Color.Red,
                ),
            ) {}

            SelectableChip(
                text = "Cyan",
                isSelected = true,
                selectableChipColors = SelectableChipColors(
                    selectedContainerColor = Color.Cyan,
                    containerColor = Color.DarkGray,
                    selectedLabelColor = Color.Red,
                    labelColor = Color.Gray,
                    borderColor = Color.Red,
                ),
            ) {}
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.theme

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.LocalTextStyle
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp

private class ColorParameterProvider : PreviewParameterProvider<Pair<AcornColors, ColorScheme>> {
    override val values: Sequence<Pair<AcornColors, ColorScheme>>
        get() = sequenceOf(
            lightColorPalette to acornLightColorScheme(),
            darkColorPalette to acornDarkColorScheme(),
            privateColorPalette to acornPrivateColorScheme(),
        )
}

@Suppress("LongMethod", "MagicNumber")
@Preview(widthDp = CONTAINER_STACK_WIDTH * 4 + CONTAINER_GUTTER * 3 + 16)
@Composable
private fun AcornColorGrid(
    @PreviewParameter(ColorParameterProvider::class) colors: Pair<AcornColors, ColorScheme>,
) {
    val colorScheme = colors.second
    AcornTheme(
        colors = colors.first,
        colorScheme = colorScheme,
    ) {
        CompositionLocalProvider(LocalTextStyle provides MaterialTheme.typography.labelSmall) {
            Column(
                modifier = Modifier
                    .background(color = colorScheme.background)
                    .padding(all = 8.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(CONTAINER_GUTTER.dp),
                ) {
                    ContainerColorStack(
                        color1 = colorScheme.primary,
                        color2 = colorScheme.onPrimary,
                        color3 = colorScheme.primaryContainer,
                        color4 = colorScheme.onPrimaryContainer,
                        color1Name = colorScheme::primary.name,
                        color2Name = colorScheme::onPrimary.name,
                        color3Name = colorScheme::primaryContainer.name,
                        color4Name = colorScheme::onPrimaryContainer.name,
                    )

                    ContainerColorStack(
                        color1 = colorScheme.secondary,
                        color2 = colorScheme.onSecondary,
                        color3 = colorScheme.secondaryContainer,
                        color4 = colorScheme.onSecondaryContainer,
                        color1Name = colorScheme::secondary.name,
                        color2Name = colorScheme::onSecondary.name,
                        color3Name = colorScheme::secondaryContainer.name,
                        color4Name = colorScheme::onSecondaryContainer.name,
                    )

                    ContainerColorStack(
                        color1 = colorScheme.tertiary,
                        color2 = colorScheme.onTertiary,
                        color3 = colorScheme.tertiaryContainer,
                        color4 = colorScheme.onTertiaryContainer,
                        color1Name = colorScheme::tertiary.name,
                        color2Name = colorScheme::onTertiary.name,
                        color3Name = colorScheme::tertiaryContainer.name,
                        color4Name = colorScheme::onTertiaryContainer.name,
                    )

                    ContainerColorStack(
                        color1 = colorScheme.error,
                        color2 = colorScheme.onError,
                        color3 = colorScheme.errorContainer,
                        color4 = colorScheme.onErrorContainer,
                        color1Name = colorScheme::error.name,
                        color2Name = colorScheme::onError.name,
                        color3Name = colorScheme::errorContainer.name,
                        color4Name = colorScheme::onErrorContainer.name,
                    )
                }

                Column {
                    Row {
                        Text(
                            text = colorScheme::surfaceTint.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceTint),
                            color = colorScheme.onSurface,
                        )

                        Text(
                            text = colorScheme::surfaceDim.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceDim),
                            color = colorScheme.onSurface,
                        )

                        Text(
                            text = colorScheme::surface.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surface),
                            color = colorScheme.onSurface,
                        )

                        Text(
                            text = colorScheme::surfaceBright.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceBright),
                            color = colorScheme.onSurface,
                        )
                    }

                    Spacer(modifier = Modifier.height(4.dp))

                    Row {
                        Text(
                            text = colorScheme::surfaceContainerLowest.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceContainerLowest),
                            color = colorScheme.onSurface,
                        )

                        Text(
                            text = colorScheme::surfaceContainerLow.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceContainerLow),
                            color = colorScheme.onSurface,
                        )

                        Text(
                            text = colorScheme::surfaceContainer.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceContainer),
                            color = colorScheme.onSurface,
                        )

                        Text(
                            text = colorScheme::surfaceContainerHigh.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceContainerHigh),
                            color = colorScheme.onSurface,
                        )

                        Text(
                            text = colorScheme::surfaceContainerHighest.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.surfaceContainerHighest),
                            color = colorScheme.onSurface,
                        )
                    }

                    Spacer(modifier = Modifier.height(4.dp))

                    Row {
                        Text(
                            text = colorScheme::onSurface.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.onSurface),
                            color = colorScheme.onPrimary,
                        )

                        Text(
                            text = colorScheme::onSurfaceVariant.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.onSurfaceVariant),
                            color = colorScheme.onPrimary,
                        )

                        Text(
                            text = colorScheme::outline.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.outline),
                            color = colorScheme.onPrimary,
                        )

                        Text(
                            text = colorScheme::outlineVariant.name,
                            modifier = Modifier
                                .weight(1f)
                                .colorGridItem(color = colorScheme.outlineVariant),
                            color = colorScheme.onPrimaryContainer,
                        )
                    }

                    Spacer(Modifier.height(16.dp))

                    Row {
                        Column(
                            modifier = Modifier.width(CONTAINER_STACK_WIDTH.dp),
                        ) {
                            Text(
                                text = colorScheme::inverseSurface.name,
                                modifier = Modifier.colorGridItemShort(color = colorScheme.inverseSurface),
                                color = colorScheme.inverseOnSurface,
                            )

                            Text(
                                text = colorScheme::inverseOnSurface.name,
                                modifier = Modifier.colorGridItemShort(color = colorScheme.inverseOnSurface),
                                color = colorScheme.inverseSurface,
                            )

                            Text(
                                text = colorScheme::inversePrimary.name,
                                modifier = Modifier.colorGridItemShort(color = colorScheme.inversePrimary),
                                color = colorScheme.inverseSurface,
                            )
                        }

                        Spacer(Modifier.width(16.dp))

                        Row(
                            modifier = Modifier.width(CONTAINER_STACK_WIDTH.dp),
                        ) {
                            Text(
                                text = colorScheme::scrim.name,
                                modifier = Modifier
                                    .fillMaxWidth(0.5f)
                                    .colorGridItemShort(color = colorScheme.scrim),
                                color = colorScheme.onSurface,
                            )
                        }
                    }
                }

                Text(
                    text = "Extended palette",
                    style = MaterialTheme.typography.displayMedium,
                    color = colorScheme.onSurface,
                )

                Column(
                    modifier = Modifier.width(CONTAINER_STACK_WIDTH.dp),
                ) {
                    Text(
                        text = colorScheme::surfaceDimVariant.name,
                        modifier = Modifier.colorGridItemShort(color = colorScheme.surfaceDimVariant),
                        color = colorScheme.onSurface,
                    )

                    Text(
                        text = colorScheme::information.name,
                        modifier = Modifier.colorGridItemShort(color = colorScheme.information),
                        color = colorScheme.onPrimary,
                    )
                }
            }
        }
    }
}

private const val CONTAINER_STACK_WIDTH = 200
private const val CONTAINER_GUTTER = 4

@Suppress("LongParameterList", "LongMethod", "MagicNumber")
@Composable
private fun ContainerColorStack(
    color1: Color,
    color2: Color,
    color3: Color,
    color4: Color,
    color1Name: String,
    color2Name: String,
    color3Name: String,
    color4Name: String,
) {
    Column(
        modifier = Modifier.width(CONTAINER_STACK_WIDTH.dp),
    ) {
        Text(
            text = color1Name,
            modifier = Modifier.colorGridItemShort(color = color1),
            color = color2,
        )

        Text(
            text = color2Name,
            modifier = Modifier.colorGridItemShort(color = color2),
            color = color1,
        )

        Spacer(modifier = Modifier.height(4.dp))

        Text(
            text = color3Name,
            modifier = Modifier.colorGridItemShort(color = color3),
            color = color4,
        )

        Text(
            text = color4Name,
            modifier = Modifier.colorGridItemShort(color = color4),
            color = color3,
        )
    }
}

private fun Modifier.colorGridItemShort(color: Color) = this.then(
    other = Modifier.background(color = color)
        .fillMaxWidth()
        .height(50.dp)
        .wrapContentHeight()
        .padding(all = 12.dp),
)

private fun Modifier.colorGridItem(
    color: Color,
) = this.then(
    other = Modifier
        .fillMaxWidth()
        .background(color = color)
        .height(70.dp)
        .wrapContentHeight()
        .padding(all = 12.dp),
)

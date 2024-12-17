/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.ext.dashedBorder
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Card for presenting placeholder information or CTAs.
 *
 * @param title Composable for the title slot in the component.
 * @param description Composable for the description slot in the component.
 * @param modifier Modifier to apply to the card.
 */
@Composable
fun PlaceholderCard(
    title: @Composable () -> Unit,
    description: @Composable () -> Unit,
    modifier: Modifier = Modifier,
) {
    FirefoxTheme {
        Card(
            modifier = Modifier
                .dashedBorder(
                    color = FirefoxTheme.colors.borderPrimary,
                    cornerRadius = 8.dp,
                    dashHeight = 2.dp,
                    dashWidth = 4.dp,
                )
                .then(modifier),
            backgroundColor = FirefoxTheme.colors.layer1,
        ) {
            Column(
                Modifier
                    .padding(16.dp)
                    .fillMaxWidth(),
            ) {
                title()
                Spacer(modifier = Modifier.height(4.dp))
                description()
            }
        }
    }
}

@PreviewLightDark
@Composable
private fun PlaceholderCardPreview() {
    FirefoxTheme {
        Surface {
            Box(modifier = Modifier.padding(8.dp)) {
                PlaceholderCard(
                    title = {
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .wrapContentHeight(),
                            horizontalArrangement = Arrangement.Absolute.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Text(
                                text = stringResource(R.string.collections_header),
                                color = FirefoxTheme.colors.textPrimary,
                                style = FirefoxTheme.typography.headline7,
                            )

                            IconButton(
                                onClick = {},
                                modifier = Modifier.size(20.dp),
                            ) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_cross_20),
                                    contentDescription = stringResource(
                                        R.string.remove_home_collection_placeholder_content_description,
                                    ),
                                    tint = FirefoxTheme.colors.textPrimary,
                                )
                            }
                        }
                    },
                    description = {
                        Text(
                            text = stringResource(R.string.no_collections_description2),
                            color = FirefoxTheme.colors.textSecondary,
                            style = FirefoxTheme.typography.body2,
                        )

                        Spacer(modifier = Modifier.height(16.dp))

                        PrimaryButton(
                            text = stringResource(R.string.tabs_menu_save_to_collection1),
                            modifier = Modifier
                                .fillMaxWidth()
                                .wrapContentHeight(),
                            icon = painterResource(R.drawable.ic_tab_collection),
                            onClick = {},
                        )
                    },
                )
            }
        }
    }
}

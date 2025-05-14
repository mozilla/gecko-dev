/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.region

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import androidx.core.content.edit
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewmodel.compose.viewModel
import mozilla.components.browser.state.action.SearchAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.base.button.PrimaryButton
import mozilla.components.compose.base.textfield.TextField
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.Config
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

private const val DEFAULT_REGION = "XX"
private const val MAX_REGION_LENGTH = 2

// This is copied from Region manager
private const val PREFERENCE_FILE = "mozac_feature_search_region"
private const val PREFERENCE_KEY_HOME_REGION = "region.home"

/**
 * Region UI that display region related tools.
 */
@Composable
@Suppress("LongMethod")
fun RegionTools(
    browserStore: BrowserStore,
) {
    val region by browserStore.observeAsState(initialValue = RegionState.Default) { state ->
        state.search.region ?: RegionState.Default
    }
    val viewModel: RegionToolsViewModel = viewModel()
    val homeRegion = viewModel.homeRegion
    val currentRegion = viewModel.currentRegion

    Column(
        modifier = Modifier
            .padding(all = 16.dp)
            .verticalScroll(state = rememberScrollState()),
    ) {
        Text(
            text = stringResource(R.string.debug_drawer_regin_tools_description),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline8,
        )

        Spacer(modifier = Modifier.height(16.dp))

        Text(
            text = stringResource(R.string.debug_drawer_home_region_label),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
            modifier = Modifier.padding(4.dp),
        )

        Text(
            text = region.home,
            color = FirefoxTheme.colors.textPrimary,
            modifier = Modifier.padding(4.dp),
            style = FirefoxTheme.typography.body1,
        )

        Text(
            text = stringResource(R.string.debug_drawer_current_region_label),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
            modifier = Modifier.padding(4.dp),
        )

        Text(
            text = region.current,
            color = FirefoxTheme.colors.textPrimary,
            modifier = Modifier.padding(4.dp),
            style = FirefoxTheme.typography.body1,
        )

        Spacer(modifier = Modifier.height(16.dp))

        TextField(
            value = viewModel.homeRegion,
            onValueChange = {
                it.validRegionInput(MAX_REGION_LENGTH)?.let { verifiedInput ->
                    viewModel.homeRegion = verifiedInput
                }
            },
            placeholder = "",
            errorText = "",
            modifier = Modifier.fillMaxWidth().padding(4.dp),
            label = stringResource(R.string.debug_drawer_override_home_region_label),
        )

        TextField(
            value = viewModel.currentRegion,
            onValueChange = {
                it.validRegionInput(MAX_REGION_LENGTH)?.let { verifiedInput ->
                    viewModel.currentRegion = verifiedInput
                }
            },
            placeholder = "",
            errorText = "",
            modifier = Modifier.fillMaxWidth().padding(4.dp),
            label = stringResource(R.string.debug_drawer_override_current_region_label),
        )

        Spacer(modifier = Modifier.height(16.dp))

        PrimaryButton(
            text = stringResource(R.string.debug_drawer_override_region),
            modifier = Modifier.fillMaxWidth(),
            onClick = {
                browserStore.dispatch(
                    SearchAction.SetRegionAction(
                        regionState = RegionState(
                            home = homeRegion.ifBlank { DEFAULT_REGION },
                            current = currentRegion.ifBlank { DEFAULT_REGION },
                        ),
                        distribution = null,
                    ),
                )
            },
        )

        if (Config.channel.isNightlyOrDebug) {
            val preferences = LocalContext.current.getSharedPreferences(
                    PREFERENCE_FILE,
                    Context.MODE_PRIVATE,
                )

            PrimaryButton(
                text = stringResource(R.string.debug_drawer_override_home_region_permanently),
                modifier = Modifier.fillMaxWidth(),
                onClick = {
                    preferences.edit { putString(PREFERENCE_KEY_HOME_REGION, homeRegion.ifBlank { DEFAULT_REGION }) }
                },
            )
        }
    }
}

private fun String.validRegionInput(maxLength: Int): String? {
    return if (length <= maxLength && all { it.isLetter() }) {
        uppercase()
    } else {
        null
    }
}

/**
 * Holds user input for overriding home and current regions in the debug UI.
 */
class RegionToolsViewModel : ViewModel() {
    var homeRegion by mutableStateOf("")
    var currentRegion by mutableStateOf("")
}

@Composable
@PreviewLightDark
private fun RegionScreenPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
        ) {
            RegionTools(
                browserStore = BrowserStore(),
            )
        }
    }
}

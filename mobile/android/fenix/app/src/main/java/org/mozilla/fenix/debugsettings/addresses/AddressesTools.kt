/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.addresses

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Addresses UI for the debug drawer that displays various addresses related tools.
 */
@Composable
fun AddressesTools(debugLocalesRepository: AddressesDebugLocalesRepository) {
    var possibleDebugLocales by remember {
        mutableStateOf(debugLocalesRepository.initialEnabledState())
    }
    val onLocaleToggled = { locale: DebugLocale, isEnabled: Boolean ->
        debugLocalesRepository.setLocaleEnabled(locale, isEnabled)
        possibleDebugLocales = possibleDebugLocales.updateLocaleEnabled(locale, isEnabled)
    }

    AddressesContent(
        debugLocaleStates = possibleDebugLocales,
        onLocaleToggled = onLocaleToggled,
    )
}

@Composable
private fun AddressesContent(
    debugLocaleStates: List<DebugLocaleEnabledState>,
    onLocaleToggled: (DebugLocale, Boolean) -> Unit,
) {
    Column(modifier = Modifier.padding(16.dp)) {
        Text(
            text = stringResource(R.string.debug_drawer_addresses_title),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.headline5,
        )

        Spacer(Modifier.height(16.dp))

        DebugLocalesToEnableSection(
            debugLocaleStates = debugLocaleStates,
            onLocaleToggled = onLocaleToggled,
        )
    }
}

@Composable
private fun DebugLocalesToEnableSection(
    debugLocaleStates: List<DebugLocaleEnabledState>,
    onLocaleToggled: (DebugLocale, Boolean) -> Unit,
) {
    Text(
        text = stringResource(R.string.debug_drawer_addresses_debug_locales_header),
        color = FirefoxTheme.colors.textSecondary,
        style = FirefoxTheme.typography.headline7,
    )

    LazyColumn {
        items(
            items = debugLocaleStates,
            key = { state -> state.locale.name },
        ) { debugLocaleState ->
            SwitchWithLabel(
                label = debugLocaleState.locale.name,
                checked = debugLocaleState.enabled,
                onCheckedChange = { onLocaleToggled(debugLocaleState.locale, it) },
            )
        }
    }
}

private data class DebugLocaleEnabledState(
    val locale: DebugLocale,
    val enabled: Boolean,
)

private fun AddressesDebugLocalesRepository.initialEnabledState(): List<DebugLocaleEnabledState> =
    DebugLocale.entries.map { debugLocale ->
        DebugLocaleEnabledState(
            locale = debugLocale,
            enabled = isLocaleEnabled(debugLocale),
        )
    }

private fun List<DebugLocaleEnabledState>.updateLocaleEnabled(localeToUpdate: DebugLocale, isEnabled: Boolean) =
    this.map { localeState ->
        if (localeState.locale == localeToUpdate) {
            localeState.copy(enabled = isEnabled)
        } else {
            localeState
        }
    }

@Composable
@LightDarkPreview
private fun AddressesScreenPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
        ) {
            AddressesTools(FakeAddressesDebugLocalesRepository())
        }
    }
}

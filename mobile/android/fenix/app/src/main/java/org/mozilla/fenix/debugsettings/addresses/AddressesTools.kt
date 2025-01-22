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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCardsAddressesStorage
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.list.RadioButtonListItem
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Addresses UI for the debug drawer that displays various addresses related tools.
 */
@Composable
fun AddressesTools(
    debugLocalesRepository: AddressesDebugLocalesRepository,
    creditCardsAddressesStorage: CreditCardsAddressesStorage,
) {
    var possibleDebugLocales by remember {
        mutableStateOf(debugLocalesRepository.initialEnabledState())
    }
    val onLocaleToggled = { locale: DebugLocale, isEnabled: Boolean ->
        debugLocalesRepository.setLocaleEnabled(locale, isEnabled)
        possibleDebugLocales = possibleDebugLocales.updateLocaleEnabled(locale, isEnabled)
    }

    val scope = rememberCoroutineScope()
    var addresses by remember { mutableStateOf(listOf<Address>()) }
    LaunchedEffect(Unit) {
        addresses = creditCardsAddressesStorage.getAllAddresses()
    }
    val onAddAddress: (String) -> Unit = { selectedLangTag ->
        scope.launch {
            creditCardsAddressesStorage.addAddress(selectedLangTag.generateFakeAddressForLangTag())
            addresses = creditCardsAddressesStorage.getAllAddresses()
        }
    }
    val onDeleteAddress: (Address) -> Unit = { address ->
        scope.launch {
            creditCardsAddressesStorage.deleteAddress(address.guid)
            addresses = creditCardsAddressesStorage.getAllAddresses()
        }
    }
    val onDeleteAllAddresses: () -> Unit = {
        scope.launch {
            creditCardsAddressesStorage.getAllAddresses().forEach { address ->
                creditCardsAddressesStorage.deleteAddress(address.guid)
            }
            addresses = creditCardsAddressesStorage.getAllAddresses()
        }
    }

    AddressesContent(
        debugLocaleStates = possibleDebugLocales,
        onLocaleToggled = onLocaleToggled,
        addresses = addresses,
        onAddAddressClick = onAddAddress,
        onDeleteAddressClick = onDeleteAddress,
        onDeleteAllAddressesClick = onDeleteAllAddresses,
    )
}

@Composable
private fun AddressesContent(
    debugLocaleStates: List<DebugLocaleEnabledState>,
    onLocaleToggled: (DebugLocale, Boolean) -> Unit,
    addresses: List<Address>,
    onAddAddressClick: (locale: String) -> Unit,
    onDeleteAddressClick: (Address) -> Unit,
    onDeleteAllAddressesClick: () -> Unit,
) {
    Column(
        modifier = Modifier
            .padding(all = 16.dp)
            .verticalScroll(state = rememberScrollState()),
    ) {
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

        Spacer(Modifier.height(16.dp))

        AddressesManagementSection(
            addresses = addresses,
            onAddAddressClick = onAddAddressClick,
            onDeleteAddressClick = onDeleteAddressClick,
            onDeleteAllAddressesClick = onDeleteAllAddressesClick,
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

    Column {
        debugLocaleStates.forEach { debugLocaleState ->
            SwitchWithLabel(
                label = debugLocaleState.locale.name,
                checked = debugLocaleState.enabled,
                onCheckedChange = { onLocaleToggled(debugLocaleState.locale, it) },
            )
        }
    }
}

@Composable
private fun AddressesManagementSection(
    addresses: List<Address>,
    onAddAddressClick: (locale: String) -> Unit,
    onDeleteAddressClick: (Address) -> Unit,
    onDeleteAllAddressesClick: () -> Unit,
) {
    val possibleLocales = remember { FakeCreditCardsAddressesStorage.getAllPossibleLocaleLangTags() }
    var selectedLocaleLangTagForAddingAddress by remember { mutableStateOf(possibleLocales.first()) }

    Column {
        Text(
            text = stringResource(R.string.debug_drawer_addresses_management_header),
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.headline7,
        )

        Column {
            possibleLocales.forEach { langTag ->
                RadioButtonListItem(
                    label = langTag,
                    selected = langTag == selectedLocaleLangTagForAddingAddress,
                    onClick = { selectedLocaleLangTagForAddingAddress = langTag },
                )
            }
        }

        Spacer(Modifier.height(8.dp))

        PrimaryButton(
            text = stringResource(R.string.debug_drawer_add_new_address),
            onClick = { onAddAddressClick(selectedLocaleLangTagForAddingAddress) },
        )

        PrimaryButton(
            text = stringResource(R.string.debug_drawer_delete_all_addresses),
            onClick = onDeleteAllAddressesClick,
        )

        Spacer(Modifier.height(8.dp))

        Column {
            addresses.forEach { address ->
                TextListItem(
                    label = address.name,
                    description = address.addressLabel,
                    iconPainter = painterResource(R.drawable.ic_delete),
                    onIconClick = { onDeleteAddressClick(address) },
                )
            }
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
            AddressesTools(
                debugLocalesRepository = FakeAddressesDebugLocalesRepository(),
                creditCardsAddressesStorage = FakeCreditCardsAddressesStorage(),
            )
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.addons.ui

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.button.PrimaryButton
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.support.webextensions.WebExtensionSupport.installedExtensions
import org.mozilla.fenix.R
import org.mozilla.fenix.components.components
import org.mozilla.fenix.debugsettings.ui.DebugDrawer
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Add-ons Debug Tools UI for [DebugDrawer].
 *
 * @param addonUpdater [AddonUpdater] used to check for add-on updates.
 */
@Composable
fun AddonsDebugToolsScreen(addonUpdater: AddonUpdater = components.addonUpdater) {
    Column(modifier = Modifier.padding(16.dp)) {
        PrimaryButton(
            text = stringResource(R.string.addons_debug_tools_check_for_updates_button),
            modifier = Modifier.fillMaxWidth(),
            onClick = {
                // Trigger the update logic for each installed add-on.
                installedExtensions
                    .filterValues { !it.isBuiltIn() }
                    .forEach {
                        addonUpdater.update(addonId = it.value.id)
                    }
            },
        )
    }
}

@Composable
@PreviewLightDark
private fun AddonsDebugToolsScreenPreview() {
    FirefoxTheme {
        AddonsDebugToolsScreen()
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ComponentName
import android.content.pm.PackageManager
import org.mozilla.fenix.nimbus.FxNimbus

/**
 * Required to implement the [FxNimbus.features] alternativeAppLauncherIcon feature.
 *
 * Will set the app launcher icon based on the [resetToDefault] flag.
 * Checks whether the icon needs updating to prevent unnecessary icon updates.
 *
 * @param packageManager The application [PackageManager].
 * @param appAlias The 'default' app alias defined in AndroidManifest.xml.
 * @param alternativeAppAlias The 'alternative' app alias defined in AndroidManifest.xml.
 * @param resetToDefault True to reset the icon to default, otherwise false to use the alternative icon.
 */
fun changeAppLauncherIconBackgroundColor(
    packageManager: PackageManager,
    appAlias: ComponentName,
    alternativeAppAlias: ComponentName,
    resetToDefault: Boolean,
) {
    val userHasAlternativeAppIconSet =
        userHasAlternativeAppIconSet(packageManager, appAlias, alternativeAppAlias)

    if (resetToDefault && userHasAlternativeAppIconSet) {
        resetAppIconsToDefault(packageManager, appAlias, alternativeAppAlias)
        return
    }

    if (!resetToDefault && !userHasAlternativeAppIconSet) {
        setAppIconsToAlternative(packageManager, appAlias, alternativeAppAlias)
    }
}

private fun userHasAlternativeAppIconSet(
    packageManager: PackageManager,
    appAlias: ComponentName,
    alternativeAppAlias: ComponentName,
): Boolean {
    val appAliasState = packageManager.getComponentEnabledSetting(appAlias)
    val alternativeAppAliasState =
        packageManager.getComponentEnabledSetting(alternativeAppAlias)

    // If the default App alias was explicitly disabled AND the experiment AppAlternative alias
    // has been explicitly enabled, then the user already has the alternative app launcher icon set.
    return appAliasState == PackageManager.COMPONENT_ENABLED_STATE_DISABLED &&
        alternativeAppAliasState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED
}

private fun setAppIconsToAlternative(
    packageManager: PackageManager,
    appAlias: ComponentName,
    alternativeAppAlias: ComponentName,
) {
    appAlias.setEnabledStateTo(packageManager, false)
    alternativeAppAlias.setEnabledStateTo(packageManager, true)
}

private fun resetAppIconsToDefault(
    packageManager: PackageManager,
    appAlias: ComponentName,
    alternativeAppAlias: ComponentName,
) {
    alternativeAppAlias.setEnabledStateToDefault(packageManager)
    appAlias.setEnabledStateToDefault(packageManager)
}

private fun ComponentName.setEnabledStateToDefault(packageManager: PackageManager) {
    packageManager.setComponentEnabledSetting(
        this,
        PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
        PackageManager.DONT_KILL_APP,
    )
}

private fun ComponentName.setEnabledStateTo(packageManager: PackageManager, enabled: Boolean) {
    val newState = if (enabled) {
        PackageManager.COMPONENT_ENABLED_STATE_ENABLED
    } else {
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED
    }

    packageManager.setComponentEnabledSetting(
        this,
        newState,
        PackageManager.DONT_KILL_APP,
    )
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.nimbus.FxNimbus

private val logger = Logger("ChangeAppLauncherIcon")

/**
 * Required to implement the [FxNimbus.features] alternativeAppLauncherIcon feature.
 *
 * Will set the app launcher icon based on the [resetToDefault] flag.
 * Checks whether the icon needs updating to prevent unnecessary icon updates.
 *
 * @param context The application [Context].
 * @param shortcutManager [ShortcutManagerWrapper] to safely access some features in [ShortcutInfoCompat].
 * @param appAlias The 'default' app alias defined in AndroidManifest.xml.
 * @param alternativeAppAlias The 'alternative' app alias defined in AndroidManifest.xml.
 * @param resetToDefault True to reset the icon to default, otherwise false to use the alternative icon.
 */
fun changeAppLauncherIcon(
    context: Context,
    shortcutManager: ShortcutManagerWrapper = ShortcutManagerWrapperDefault(context),
    appAlias: ComponentName,
    alternativeAppAlias: ComponentName,
    resetToDefault: Boolean,
) {
    val userHasAlternativeAppIconSet =
        userHasAlternativeAppIconSet(context.packageManager, appAlias, alternativeAppAlias)

    if (resetToDefault && userHasAlternativeAppIconSet) {
        resetAppIconsToDefault(context, shortcutManager, appAlias, alternativeAppAlias)
        return
    }

    if (!resetToDefault && !userHasAlternativeAppIconSet) {
        setAppIconsToAlternative(context, shortcutManager, appAlias, alternativeAppAlias)
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
    context: Context,
    shortcutManager: ShortcutManagerWrapper,
    appAlias: ComponentName,
    alternativeAppAlias: ComponentName,
) {
    val packageManager = context.packageManager
    alternativeAppAlias.setEnabledStateTo(packageManager, true)

    if (updateShortcutsComponentName(context, shortcutManager, alternativeAppAlias)) {
        logger.info("Successfully attempted to update the app icon to the alternative.")
        appAlias.setEnabledStateTo(packageManager, false)
    } else {
        logger.warn("Failed to attempt to update the app icon.")
        // If we can't successfully update the user shortcuts, then re-disable the app alias component.
        alternativeAppAlias.setEnabledStateTo(packageManager, false)
    }
}

private fun resetAppIconsToDefault(
    context: Context,
    shortcutManager: ShortcutManagerWrapper,
    appAlias: ComponentName,
    alternativeAppAlias: ComponentName,
) {
    val packageManager = context.packageManager
    appAlias.setEnabledStateToDefault(packageManager)

    if (updateShortcutsComponentName(context, shortcutManager, appAlias)) {
        logger.info("Successfully attempted to reset the app icon to default.")
        alternativeAppAlias.setEnabledStateToDefault(packageManager)
    } else {
        logger.warn("Failed to attempt to reset the app icon.")
        // If we can't successfully update the user shortcuts, then re-disable the app alias component.
        appAlias.setEnabledStateTo(packageManager, false)
    }
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

/**
 * Attempt to update the device's existing shortcuts to the given [targetAlias] state.
 *
 * @return whether updating the shortcuts to the app alias was successful.
 */
private fun updateShortcutsComponentName(
    context: Context,
    shortcutManager: ShortcutManagerWrapper,
    targetAlias: ComponentName,
): Boolean {
    val currentPinnedShortcuts = try {
        shortcutManager.getPinnedShortcuts()
    } catch (e: IllegalStateException) {
        logger.warn("Failed to retrieve the current Firefox pinned shortcuts", e)
        return false
    }

    val updatedPinnedShortcuts =
        updatedShortcuts(context, currentPinnedShortcuts, targetAlias)

    return try {
        shortcutManager.updateShortcuts(updatedPinnedShortcuts)
        true
    } catch (e: IllegalArgumentException) {
        logger.warn("Failed to update the given Firefox shortcuts: $updatedPinnedShortcuts", e)
        false
    }
}

private fun updatedShortcuts(
    context: Context,
    shortcuts: List<ShortcutInfoCompat>,
    alternativeAppAlias: ComponentName,
) = shortcuts.map { updateShortcutComponentName(context, it, alternativeAppAlias) }

private fun updateShortcutComponentName(
    context: Context,
    originalShortcut: ShortcutInfoCompat,
    alternativeAppAlias: ComponentName,
): ShortcutInfoCompat {
    with(originalShortcut) {
        val builder = ShortcutInfoCompat.Builder(context, id)
            .setShortLabel(shortLabel)
            .setIntent(intent)
            .setActivity(alternativeAppAlias) // this links the shortcut to the new component name.

        longLabel?.let { builder.setLongLabel(it) }

        return builder.build()
    }
}

/**
 * Wrapper to safely access some features in [ShortcutManagerCompat].
 */
interface ShortcutManagerWrapper {
    /**
     * @return a list of the current pinned shortcuts for Firefox.
     *
     * @throws IllegalStateException when the user is locked
     */
    fun getPinnedShortcuts(): List<ShortcutInfoCompat>

    /**
     * Update all existing Firefox shortcuts to the given [updatedShortcuts].
     *
     * @throws IllegalArgumentException if trying to update immutable shortcuts.
     */
    fun updateShortcuts(updatedShortcuts: List<ShortcutInfoCompat>)
}

/**
 * Implementation of [ShortcutManagerWrapper] that uses [ShortcutManagerCompat].
 */
class ShortcutManagerWrapperDefault(private val context: Context) : ShortcutManagerWrapper {
    override fun getPinnedShortcuts(): List<ShortcutInfoCompat> =
        ShortcutManagerCompat.getShortcuts(context, ShortcutManagerCompat.FLAG_MATCH_PINNED)

    override fun updateShortcuts(updatedShortcuts: List<ShortcutInfoCompat>) {
        ShortcutManagerCompat.updateShortcuts(context, updatedShortcuts)
    }
}

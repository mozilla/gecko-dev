/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.ComponentName
import android.content.pm.PackageManager
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ChangeAppLauncherIconTest {
    @Test
    fun `reset to default and user has default icon set changeAppLauncherIconBackgroundColor makes no changes`() {
        val packageManager = testContext.packageManager
        val appAlias = ComponentName("test", "App")
        packageManager.setComponentEnabledSetting(
            appAlias,
            PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
            PackageManager.DONT_KILL_APP,
        )
        val alternativeAppAlias = ComponentName("test", "AppAlternative")
        packageManager.setComponentEnabledSetting(
            alternativeAppAlias,
            PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
            PackageManager.DONT_KILL_APP,
        )

        changeAppLauncherIconBackgroundColor(
            packageManager,
            appAlias,
            alternativeAppAlias,
            true,
        )

        val appAliasState = packageManager.getComponentEnabledSetting(appAlias)
        assertTrue(appAliasState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED)

        val alternativeAppAliasState =
            packageManager.getComponentEnabledSetting(alternativeAppAlias)
        assertTrue(alternativeAppAliasState == PackageManager.COMPONENT_ENABLED_STATE_DISABLED)
    }

    @Test
    fun `reset to default and user has alternative icon set changeAppLauncherIconBackgroundColor resets states to default config`() {
        val packageManager = testContext.packageManager
        val appAlias = ComponentName("test", "App")
        packageManager.setComponentEnabledSetting(
            appAlias,
            PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
            PackageManager.DONT_KILL_APP,
        )
        val alternativeAppAlias = ComponentName("test", "AppAlternative")
        packageManager.setComponentEnabledSetting(
            alternativeAppAlias,
            PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
            PackageManager.DONT_KILL_APP,
        )

        changeAppLauncherIconBackgroundColor(
            packageManager,
            appAlias,
            alternativeAppAlias,
            true,
        )

        val appAliasState = packageManager.getComponentEnabledSetting(appAlias)
        assertTrue(appAliasState == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT)

        val alternativeAppAliasState =
            packageManager.getComponentEnabledSetting(alternativeAppAlias)
        assertTrue(alternativeAppAliasState == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT)
    }

    @Test
    fun `use alternative icon and user has default icon set changeAppLauncherIconBackgroundColor updates states to alternative config`() {
        val packageManager = testContext.packageManager
        val appAlias = ComponentName("test", "App")
        packageManager.setComponentEnabledSetting(
            appAlias,
            PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
            PackageManager.DONT_KILL_APP,
        )
        val alternativeAppAlias = ComponentName("test", "AppAlternative")
        packageManager.setComponentEnabledSetting(
            alternativeAppAlias,
            PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
            PackageManager.DONT_KILL_APP,
        )

        changeAppLauncherIconBackgroundColor(
            packageManager,
            appAlias,
            alternativeAppAlias,
            false,
        )

        val appAliasState = packageManager.getComponentEnabledSetting(appAlias)
        assertTrue(appAliasState == PackageManager.COMPONENT_ENABLED_STATE_DISABLED)

        val alternativeAppAliasState =
            packageManager.getComponentEnabledSetting(alternativeAppAlias)
        assertTrue(alternativeAppAliasState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED)
    }

    @Test
    fun `use alternative icon to default and user has alternative icon set changeAppLauncherIconBackgroundColor makes no changes`() {
        val packageManager = testContext.packageManager
        val appAlias = ComponentName("test", "App")
        packageManager.setComponentEnabledSetting(
            appAlias,
            PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
            PackageManager.DONT_KILL_APP,
        )
        val alternativeAppAlias = ComponentName("test", "AppAlternative")
        packageManager.setComponentEnabledSetting(
            alternativeAppAlias,
            PackageManager.COMPONENT_ENABLED_STATE_ENABLED,
            PackageManager.DONT_KILL_APP,
        )

        changeAppLauncherIconBackgroundColor(
            packageManager,
            appAlias,
            alternativeAppAlias,
            false,
        )

        val appAliasState = packageManager.getComponentEnabledSetting(appAlias)
        assertTrue(appAliasState == PackageManager.COMPONENT_ENABLED_STATE_DISABLED)

        val alternativeAppAliasState =
            packageManager.getComponentEnabledSetting(alternativeAppAlias)
        assertTrue(alternativeAppAliasState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED)
    }
}

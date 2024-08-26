/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import androidx.annotation.RequiresApi
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.FirstSession
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.ext.application

class FirstSessionPing(private val context: Context) {

    private val prefs: SharedPreferences by lazy {
        context.getSharedPreferences(
            "${this.javaClass.canonicalName}.prefs",
            Context.MODE_PRIVATE,
        )
    }

    /**
     * Checks whether or not the installation ping was already
     * triggered by the application.
     *
     * Note that this only tells us that Fenix triggered the
     * ping and then delegated the transmission to Glean. We
     * have no way to tell if it was actually sent or not.
     *
     * @return true if it was already triggered, false otherwise.
     */
    internal fun wasAlreadyTriggered(): Boolean {
        return prefs.getBoolean("ping_sent", false)
    }

    /**
     * Marks the "installation" ping as triggered by the application.
     * This ensures the ping is not triggered again at the next app
     * start.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun markAsTriggered() {
        prefs.edit().putBoolean("ping_sent", true).apply()
    }

    /**
     * Fills the metrics and triggers the 'installation' ping.
     * This is a separate function to simplify unit-testing.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun triggerPing() {
        FirstSession.distributionId.set(
            when (Config.channel.isMozillaOnline) {
                true -> "MozillaOnline"
                false -> "Mozilla"
            },
        )
        FirstSession.timestamp.set()
        FirstSession.installSource.set(installSourcePackage())

        CoroutineScope(Dispatchers.IO).launch {
            Pings.firstSession.submit()
            markAsTriggered()
        }
    }

    @SuppressLint("NewApi") // Lint cannot resolve 'sdk' as 'SDK_INT' as it's not referenced directly.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun installSourcePackage(sdk: Int = SDK_INT) = with(context.application) {
        if (sdk >= Build.VERSION_CODES.R) {
            installSourcePackageForBuildMinR(packageManager, packageName)
        } else {
            installSourcePackageForBuildMaxQ(packageManager, packageName)
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun installSourcePackageForBuildMinR(
        packageManager: PackageManager,
        packageName: String,
    ) = try {
        packageManager.getInstallSourceInfo(packageName).installingPackageName
    } catch (e: PackageManager.NameNotFoundException) {
        Logger.debug("$packageName is not available to the caller")
        null
    }.orEmpty()

    private fun installSourcePackageForBuildMaxQ(
        packageManager: PackageManager,
        packageName: String,
    ) = try {
        @Suppress("DEPRECATION")
        packageManager.getInstallerPackageName(packageName)
    } catch (e: IllegalArgumentException) {
        Logger.debug("$packageName is not installed")
        null
    }.orEmpty()

    /**
     * Trigger sending the `installation` ping if it wasn't sent already.
     * Then, mark it so that it doesn't get triggered next time Fenix
     * starts.
     */
    fun checkAndSend() {
        if (wasAlreadyTriggered()) {
            Logger.debug("InstallationPing - already generated")
            return
        }
        triggerPing()
    }
}

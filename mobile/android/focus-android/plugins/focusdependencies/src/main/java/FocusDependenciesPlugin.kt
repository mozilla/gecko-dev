/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import org.gradle.api.Plugin
import org.gradle.api.initialization.Settings

// If you ever need to force a toolchain rebuild (taskcluster) then edit the following comment.
// FORCE REBUILD 2023-05-05

class FocusDependenciesPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) = Unit
}

object FocusVersions {
    object Adjust {
        const val adjust = "4.38.2"
        const val install_referrer = "2.2"
    }

    object Testing {
        const val falcon = "2.2.0"
        const val fastlane = "2.1.1"
    }
}

object FocusDependencies {
    const val adjust = "com.adjust.sdk:adjust-android:${FocusVersions.Adjust.adjust}"
    const val install_referrer = "com.android.installreferrer:installreferrer:${FocusVersions.Adjust.install_referrer}"

    const val falcon = "com.jraska:falcon:${FocusVersions.Testing.falcon}"
    const val fastlane = "tools.fastlane:screengrab:${FocusVersions.Testing.fastlane}"
}

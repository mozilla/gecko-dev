/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import org.gradle.api.Plugin
import org.gradle.api.initialization.Settings

// If you ever need to force a toolchain rebuild (taskcluster) then edit the following comment.
// FORCE REBUILD 2023-05-12

class FenixDependenciesPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) = Unit
}

object FenixVersions {
    const val falcon = "2.2.0"
    const val fastlane = "2.1.1"

    const val adjust = "4.38.2"
    const val installreferrer = "2.2"

    const val mockk = "1.13.12"
}

@Suppress("unused")
object FenixDependencies {
    const val adjust = "com.adjust.sdk:adjust-android:${FenixVersions.adjust}"
    const val installreferrer = "com.android.installreferrer:installreferrer:${FenixVersions.installreferrer}"

    const val mockk = "io.mockk:mockk:${FenixVersions.mockk}"
    const val mockk_android = "io.mockk:mockk-android:${FenixVersions.mockk}"
    const val falcon = "com.jraska:falcon:${FenixVersions.falcon}"
    const val fastlane = "tools.fastlane:screengrab:${FenixVersions.fastlane}"
}

/**
 * Functionality to limit specific dependencies to specific repositories. These are typically expected to be used by
 * dependency group name (i.e. with `include/excludeGroup`). For additional info, see:
 * https://docs.gradle.org/current/userguide/declaring_repositories.html#sec::matching_repositories_to_dependencies
 *
 * Note: I wanted to nest this in Deps but for some reason gradle can't find it so it's top-level now. :|
 */
object RepoMatching {
    const val mozilla = "org\\.mozilla\\..*"
    const val androidx = "androidx\\..*"
    const val comAndroid = "com\\.android.*"
    const val comGoogle = "com\\.google\\..*"
}

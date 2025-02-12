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
}

object FocusDependencies {
}

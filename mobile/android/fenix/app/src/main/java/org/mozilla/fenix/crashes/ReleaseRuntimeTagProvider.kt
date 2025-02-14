/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.RuntimeTagProvider
import org.mozilla.fenix.BuildConfig

/**
 * Includes the current release version with the crash so that it can be persisted.
 */
class ReleaseRuntimeTagProvider : RuntimeTagProvider {
    override fun invoke(): Map<String, String> {
        return mapOf(CrashReporter.RELEASE_RUNTIME_TAG to BuildConfig.VERSION_NAME)
    }
}

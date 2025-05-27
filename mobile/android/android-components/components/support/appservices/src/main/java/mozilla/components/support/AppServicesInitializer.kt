/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support

import mozilla.appservices.RustComponentsInitializer
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.rusterrors.initializeRustErrors
import mozilla.components.support.rustlog.RustLog

/**
 * A namespaced object for initialization.
 */
object AppServicesInitializer {

    /**
     * Initialize the critical app services components.
     *
     * N.B: The internals need to be executed in this particular order. Changes can lead to
     * unexpected runtime failures.
     */
    fun init(crashReporting: CrashReporting) {
        // Rust components must be initialized at the very beginning, before any other Rust call, ...
        RustComponentsInitializer.init()

        initializeRustErrors(crashReporting)

        // ... but RustHttpConfig.setClient() and RustLog.enable() can be called later.
        RustLog.enable()
    }
}

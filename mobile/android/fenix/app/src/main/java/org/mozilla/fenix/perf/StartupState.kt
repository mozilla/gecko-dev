/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

/**
 * The restoration state of the application upon this most recent start up. See the
 * [Fenix perf glossary](https://wiki.mozilla.org/index.php?title=Performance/Fenix/Glossary)
 * for specific definitions.
 */
enum class StartupState {
    COLD, WARM, HOT,

    /**
     * A start up state where we weren't able to bucket it into the other categories.
     * This includes, but is not limited to:
     * - if the activity this is called from is not currently started
     * - if the currently started activity is not the first started activity
     */
    UNKNOWN,
}

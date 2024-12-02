/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.desktopmode

import org.mozilla.fenix.Config

/**
 * @see [DefaultDesktopModeFeatureFlag]
 */
class DefaultDesktopModeFeatureFlagImpl : DefaultDesktopModeFeatureFlag {

    override fun isDesktopModeEnabled(): Boolean = Config.channel.isDebug
}

/**
 * Interface for checking if the app wide default desktop mode functionality is enabled.
 */
interface DefaultDesktopModeFeatureFlag {

    /**
     * Returns true if the default desktop mode functionality is enabled.
     */
    fun isDesktopModeEnabled(): Boolean
}

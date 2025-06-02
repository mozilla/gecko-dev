/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.preferences

import mozilla.components.concept.engine.preferences.Branch
import mozilla.components.concept.engine.preferences.BrowserPreference
import org.mozilla.geckoview.GeckoPreferenceController.GeckoPreference
import org.mozilla.geckoview.GeckoPreferenceController.PREF_BRANCH_DEFAULT
import org.mozilla.geckoview.GeckoPreferenceController.PREF_BRANCH_USER

/**
 * Utility file for preferences functions related to the Gecko implementation.
 */
object GeckoPreferencesUtils {

    /**
     * Convenience method for mapping an Android Components [Branch]
     * into the corresponding GeckoView branch
     */
    fun Branch.intoGeckoBranch(): Int {
        return when (this) {
            Branch.DEFAULT -> PREF_BRANCH_DEFAULT
            Branch.USER -> PREF_BRANCH_USER
        }
    }

    /**
     * Convenience method for mapping a GeckoView [GeckoPreference]
     * into an Android Components [BrowserPreference].
     */
    fun GeckoPreference<*>.intoBrowserPreference(): BrowserPreference<*> {
        return BrowserPreference(
            pref = this.pref,
            value = this.value,
            defaultValue = this.defaultValue,
            userValue = this.userValue,
            hasUserChangedValue = this.hasUserChangedValue,
        )
    }
}

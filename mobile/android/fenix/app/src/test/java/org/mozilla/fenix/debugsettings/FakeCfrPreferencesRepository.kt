/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flowOf
import org.mozilla.fenix.debugsettings.cfrs.CfrPreferencesRepository

class FakeCfrPreferencesRepository : CfrPreferencesRepository {
    override val cfrPreferenceUpdates: Flow<CfrPreferencesRepository.CfrPreferenceUpdate> = flowOf()

    override fun init() { }

    override fun updateCfrPreference(preferenceUpdate: CfrPreferencesRepository.CfrPreferenceUpdate) { }

    override fun resetLastCfrTimestamp() { }
}

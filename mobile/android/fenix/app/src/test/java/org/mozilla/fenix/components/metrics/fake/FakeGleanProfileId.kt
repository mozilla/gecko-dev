/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.fenix.components.metrics.fake

import org.mozilla.fenix.components.metrics.GleanProfileId
import java.util.UUID

class FakeGleanProfileId : GleanProfileId {
    var gleanStoredProfileId: UUID? = null
    var generatedProfileId: UUID = UUID.randomUUID()

    override fun generateAndSet(): UUID {
        gleanStoredProfileId = generatedProfileId
        return generatedProfileId
    }
    override fun set(profileId: UUID) { gleanStoredProfileId = profileId }
    override fun unset() { gleanStoredProfileId = null }
}

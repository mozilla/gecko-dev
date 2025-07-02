/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.fake

import mozilla.components.concept.engine.EngineSession
import org.mozilla.fenix.webcompat.WebCompatReporterMoreInfoSender
import org.mozilla.fenix.webcompat.store.WebCompatReporterState

class FakeWebCompatReporterMoreInfoSender : WebCompatReporterMoreInfoSender {
    override suspend fun sendMoreWebCompatInfo(
        reason: WebCompatReporterState.BrokenSiteReason?,
        problemDescription: String?,
        enteredUrl: String?,
        tabUrl: String?,
        engineSession: EngineSession?,
    ) {}
}

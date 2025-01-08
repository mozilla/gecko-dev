/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.fake

import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.metrics.MetricServiceType

/**
 * A class to facilitate inspection of MetricController interactions for unit tests.
 */
class FakeMetricController : MetricController {
    val startedServiceTypes: MutableList<MetricServiceType> = emptyList<MetricServiceType>().toMutableList()

    override fun start(type: MetricServiceType) {
        startedServiceTypes.add(type)
    }

    override fun stop(type: MetricServiceType) {
        // no-op
    }

    override fun track(event: Event) {
        // no-op
    }
}

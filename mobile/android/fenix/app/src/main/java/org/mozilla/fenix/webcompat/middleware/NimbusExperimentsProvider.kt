/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.middleware

import mozilla.components.service.nimbus.NimbusApi
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment

/**
 * Provides active experiments and branch information for experiments.
 */
interface NimbusExperimentsProvider {

    /**
     * Get the list of currently enrolled experiments
     */
    val activeExperiments: List<EnrolledExperiment>

    /**
     * Get the currently enrolled branch for the given experiment
     *
     * @param experimentId The string experiment-id or "slug" for which to retrieve the branch
     * @return A String representing the branch-id or "slug"
     */
    fun getExperimentBranch(experimentId: String): String?
}

/**
 * Default implementation of [NimbusExperimentsProvider].
 *
 * @param nimbusApi A [NimbusApi] with which to get active experiments.
 */
class DefaultNimbusExperimentsProvider(
    private val nimbusApi: NimbusApi,
) : NimbusExperimentsProvider {

    /**
     * Get the list of currently enrolled experiments
     */
    override val activeExperiments: List<EnrolledExperiment>
        get() = nimbusApi.getActiveExperiments()

    /**
     * Get the currently enrolled branch for the given experiment
     *
     * @param experimentId The string experiment-id or "slug" for which to retrieve the branch
     * @return A String representing the branch-id or "slug"
     */
    override fun getExperimentBranch(experimentId: String): String? =
        nimbusApi.getExperimentBranch(experimentId)
}

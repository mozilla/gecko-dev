/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus.controller

import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.service.nimbus.ui.NimbusBranchesAdapterDelegate
import org.mozilla.experiments.nimbus.Branch
import org.mozilla.fenix.nimbus.NimbusBranchesAction
import org.mozilla.fenix.nimbus.NimbusBranchesStore

/**
 * Controller for managing Nimbus experiment branches.
 *
 * This implements [NimbusBranchesAdapterDelegate] to handle interactions with a Nimbus branch.
 *
 * @param isTelemetryEnabled A function that returns true if telemetry is enabled, false otherwise.
 * @param isExperimentationEnabled A function that returns true if experimentation is enabled, false otherwise.
 * @param nimbusBranchesStore The store for managing Nimbus branch state.
 * @param experiments The Nimbus API for interacting with experiments.
 * @param experimentId The ID of the experiment being managed.
 * @param notifyUserToEnableExperiments A callback function to notify the user to enable
 *                                      telemetry and experimentation if they are disabled.
 */
class NimbusBranchesController(
    private val isTelemetryEnabled: () -> Boolean,
    private val isExperimentationEnabled: () -> Boolean,
    private val nimbusBranchesStore: NimbusBranchesStore,
    private val experiments: NimbusApi,
    private val experimentId: String,
    private val notifyUserToEnableExperiments: () -> Unit,
) : NimbusBranchesAdapterDelegate {

    override fun onBranchItemClicked(branch: Branch) {
        updateOptInState(branch)

        if (!isTelemetryEnabled() && !isExperimentationEnabled()) {
            notifyUserToEnableExperiments()
        }
    }

    private fun updateOptInState(branch: Branch) {
        nimbusBranchesStore.dispatch(
            if (experiments.getExperimentBranch(experimentId) != branch.slug) {
                experiments.optInWithBranch(experimentId, branch.slug)
                NimbusBranchesAction.UpdateSelectedBranch(branch.slug)
            } else {
                experiments.optOut(experimentId)
                NimbusBranchesAction.UpdateUnselectBranch
            },
        )
    }
}

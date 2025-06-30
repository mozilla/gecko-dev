/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyAll
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.experiments.nimbus.Branch
import org.mozilla.fenix.nimbus.controller.NimbusBranchesController
import org.mozilla.fenix.utils.Settings

class NimbusBranchesControllerTest {

    private val experiments: NimbusApi = mockk(relaxed = true)
    private val experimentId = "id"

    private lateinit var controller: NimbusBranchesController
    private lateinit var nimbusBranchesStore: NimbusBranchesStore
    private lateinit var settings: Settings
    private lateinit var notifyUserToEnableExperiments: () -> Unit

    @Before
    fun setup() {
        settings = mockk(relaxed = true)
        notifyUserToEnableExperiments = mockk(relaxed = true)

        nimbusBranchesStore = NimbusBranchesStore(NimbusBranchesState(emptyList()))
        controller = NimbusBranchesController(
            isTelemetryEnabled = { true },
            isExperimentationEnabled = { true },
            nimbusBranchesStore = nimbusBranchesStore,
            experiments = experiments,
            experimentId = experimentId,
            notifyUserToEnableExperiments = notifyUserToEnableExperiments,
        )
    }

    @Test
    fun `WHEN branch item is clicked THEN branch is opted into and selectedBranch state is updated`() {
        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN branch item is clicked THEN branch is opted out and selectedBranch state is updated`() {
        every { experiments.getExperimentBranch(experimentId) } returns "slug"

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optOut(experimentId)
        }
    }

    @Test
    fun `WHEN studies and telemetry are ON and item is clicked THEN branch is opted in`() {
        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN studies and telemetry are Off THEN branch is opted in AND data is not sent`() {
        controller = NimbusBranchesController(
            isTelemetryEnabled = { false },
            isExperimentationEnabled = { false },
            nimbusBranchesStore = nimbusBranchesStore,
            experiments = experiments,
            experimentId = experimentId,
            notifyUserToEnableExperiments = notifyUserToEnableExperiments,
        )

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verifyAll {
            experiments.getExperimentBranch(experimentId)
            experiments.optInWithBranch(experimentId, branch.slug)
            notifyUserToEnableExperiments()
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN studies are ON and telemetry Off THEN branch is opted in`() {
        controller = NimbusBranchesController(
            isTelemetryEnabled = { false },
            isExperimentationEnabled = { true },
            nimbusBranchesStore = nimbusBranchesStore,
            experiments = experiments,
            experimentId = experimentId,
            notifyUserToEnableExperiments = notifyUserToEnableExperiments,
        )

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN studies are OFF and telemetry ON THEN branch is opted in`() {
        controller = NimbusBranchesController(
            isTelemetryEnabled = { true },
            isExperimentationEnabled = { false },
            nimbusBranchesStore = nimbusBranchesStore,
            experiments = experiments,
            experimentId = experimentId,
            notifyUserToEnableExperiments = notifyUserToEnableExperiments,
        )

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }
}

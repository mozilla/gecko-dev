package org.mozilla.fenix.webcompat.middleware

import org.mozilla.experiments.nimbus.internal.EnrolledExperiment

class FakeNimbusExperimentsProvider(
    override val activeExperiments: List<EnrolledExperiment> = emptyList(),
    private val experimentBranchLambda: (String) -> String? = { null },
) : NimbusExperimentsProvider {
    override fun getExperimentBranch(experimentId: String): String? =
        experimentBranchLambda(experimentId)
}

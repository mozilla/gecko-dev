/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import org.junit.Assert
import org.junit.Ignore
import org.junit.Test
import org.mozilla.fenix.GleanMetrics.NimbusSystem
import org.mozilla.fenix.helpers.TestSetup

class NimbusSystemMetricsTest : TestSetup() {

    @Test
    @Ignore("It will be addressed in https://bugzilla.mozilla.org/show_bug.cgi?id=1933543")
    fun testRecordedContextIsPassedInAndRecordedToGlean() {
        val value = NimbusSystem.recordedNimbusContext.testGetValue()

        Assert.assertTrue(value?.jsonObject?.get("isFirstRun")?.jsonPrimitive?.booleanOrNull ?: false)
        Assert.assertTrue(value?.jsonObject?.get("installReferrerResponseUtmSource")?.jsonPrimitive?.isString ?: false)
        Assert.assertTrue(value?.jsonObject?.get("installReferrerResponseUtmMedium")?.jsonPrimitive?.isString ?: false)
        Assert.assertTrue(value?.jsonObject?.get("installReferrerResponseUtmCampaign")?.jsonPrimitive?.isString ?: false)
        Assert.assertTrue(value?.jsonObject?.get("installReferrerResponseUtmTerm")?.jsonPrimitive?.isString ?: false)
        Assert.assertTrue(value?.jsonObject?.get("installReferrerResponseUtmContent")?.jsonPrimitive?.isString ?: false)
    }
}

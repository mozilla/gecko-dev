/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.addJsonObject
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.fenix.webcompat.di.WebCompatReporterMiddlewareProvider
import org.mozilla.fenix.webcompat.fake.FakeEngineSession
import org.mozilla.fenix.webcompat.testdata.WebCompatTestData

@RunWith(AndroidJUnit4::class)
class DefaultWebCompatReporterRetrievalServiceTest {
    private val webCompatInfoDeserializer = WebCompatReporterMiddlewareProvider.provideWebCompatInfoDeserializer()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN Nimbus is disabled THEN WebCompatInfo has no experiments`() = runTest {
        val nimbusExperimentsProvider = FakeNimbusExperimentsProvider()
        val engineSession = FakeEngineSession(WebCompatTestData.basicDataJson)
        val service = createService(engineSession = engineSession, nimbusExperimentsProvider)

        assertEquals(emptyList<WebCompatInfoDto.WebCompatBrowserDto>(), service.retrieveInfo()?.browser?.experiments)
    }

    @Test
    fun `WHEN Nimbus is enabled but no experiments THEN WebCompatInfo experiments is null`() = runTest {
        val nimbusExperimentsProvider = FakeNimbusExperimentsProvider()
        val engineSession = FakeEngineSession(WebCompatTestData.basicDataJson)
        val service = createService(engineSession = engineSession, nimbusExperimentsProvider)

        assertEquals(emptyList<WebCompatInfoDto.WebCompatBrowserDto>(), service.retrieveInfo()?.browser?.experiments)
    }

    @Test
    fun `WHEN Nimbus is enabled THEN WebCompatInfo has all active experiments`() = runTest {
        val experiments = listOf(
            EnrolledExperiment(
                slug = "expSlug",
                branchSlug = "expBranch",
                featureIds = listOf("expFeatureId"),
                userFacingName = "expDame",
                userFacingDescription = "expDescription",
            ),
        )

        val nimbusExperimentsProvider = FakeNimbusExperimentsProvider(experiments) { slug ->
            experiments.firstOrNull { it.slug == slug }?.branchSlug ?: "none"
        }
        val engineSession = FakeEngineSession(WebCompatTestData.basicDataJson)
        val service = createService(engineSession = engineSession, nimbusExperimentsProvider)
        val actual = service.retrieveInfo()?.browser?.experiments
        val expected = listOf(
            WebCompatInfoDto.WebCompatBrowserDto.ExperimentDto(branch = "expBranch", slug = "expSlug", kind = "nimbusExperiment"),
        )
        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN WebCompatInfo is retrieved successfully THEN all corresponding fields in the DTO are submitted`() = runTest {
        val engineSession = FakeEngineSession(WebCompatTestData.basicDataJson)
        val service = createService(engineSession = engineSession)

        val actual = service.retrieveInfo()
        val expected = WebCompatInfoDto(
            antitracking = WebCompatInfoDto.WebCompatAntiTrackingDto(
                blockList = "basic",
                btpHasPurgedSite = false,
                etpCategory = "standard",
                hasMixedActiveContentBlocked = false,
                hasMixedDisplayContentBlocked = false,
                hasTrackingContentBlocked = false,
                isPrivateBrowsing = false,
            ),
            browser = WebCompatInfoDto.WebCompatBrowserDto(
                addons = listOf(
                    WebCompatInfoDto.WebCompatBrowserDto.AddonDto(id = "id.temp", name = "name1", temporary = true, version = "version1"),
                    WebCompatInfoDto.WebCompatBrowserDto.AddonDto(id = "id.perm", name = "name2", temporary = false, version = "version2"),
                ),
                app = WebCompatInfoDto.WebCompatBrowserDto.AppDto(
                    defaultUserAgent = "testDefaultUserAgent",
                ),
                experiments = emptyList(),
                graphics = WebCompatInfoDto.WebCompatBrowserDto.GraphicsDto(
                    devices = buildJsonArray {
                        addJsonObject {
                            put("id", JsonPrimitive("device1"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("device2"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("device3"))
                        }
                    },
                    drivers = buildJsonArray {
                        addJsonObject {
                            put("id", JsonPrimitive("driver1"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("driver2"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("driver3"))
                        }
                    },
                    features = buildJsonObject { put("id", JsonPrimitive("feature1")) },
                    hasTouchScreen = true,
                    monitors = buildJsonArray {
                        addJsonObject {
                            put("id", JsonPrimitive("monitor1"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("monitor2"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("monitor3"))
                        }
                    },
                ),
                locales = listOf("en-CA", "en-US"),
                platform = WebCompatInfoDto.WebCompatBrowserDto.PlatformDto(
                    fissionEnabled = false,
                    memoryMB = 1,
                ),
                prefs = WebCompatInfoDto.WebCompatBrowserDto.PrefsDto(
                    browserOpaqueResponseBlocking = false,
                    extensionsInstallTriggerEnabled = false,
                    gfxWebRenderSoftware = false,
                    networkCookieBehavior = 1,
                    privacyGlobalPrivacyControlEnabled = false,
                    privacyResistFingerprinting = false,
                ),
            ),
            url = "https://www.mozilla.org",
            devicePixelRatio = 1.5,
            frameworks = WebCompatInfoDto.WebCompatFrameworksDto(
                fastclick = true,
                marfeel = true,
                mobify = true,
            ),
            languages = listOf("en-CA", "en-US"),
            userAgent = "testUserAgent",
        )
        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN a required WebCompatInfo field is missing THEN null is returned`() = runTest {
        val engineSession = FakeEngineSession(WebCompatTestData.missingDataJson)
        val service = createService(engineSession = engineSession)

        assertNull(service.retrieveInfo())
    }

    @Test
    fun `GIVEN the json has irrelevant fields WHEN calling decode THEN the relevant fields are parsed and the irrelevant fields are ignored`() = runTest {
        val engineSession = FakeEngineSession(WebCompatTestData.extraDataJson)
        val service = createService(engineSession = engineSession)

        val actual = service.retrieveInfo()
        val expected = WebCompatInfoDto(
            antitracking = WebCompatInfoDto.WebCompatAntiTrackingDto(
                blockList = "basic",
                btpHasPurgedSite = false,
                etpCategory = "standard",
                hasMixedActiveContentBlocked = false,
                hasMixedDisplayContentBlocked = false,
                hasTrackingContentBlocked = false,
                isPrivateBrowsing = false,
            ),
            browser = WebCompatInfoDto.WebCompatBrowserDto(
                addons = listOf(
                    WebCompatInfoDto.WebCompatBrowserDto.AddonDto(id = "id.temp", name = "name1", temporary = true, version = "version1"),
                    WebCompatInfoDto.WebCompatBrowserDto.AddonDto(id = "id.perm", name = "name2", temporary = false, version = "version2"),
                ),
                app = WebCompatInfoDto.WebCompatBrowserDto.AppDto(
                    defaultUserAgent = "testDefaultUserAgent",
                ),
                experiments = emptyList(),
                graphics = WebCompatInfoDto.WebCompatBrowserDto.GraphicsDto(
                    devices = buildJsonArray {
                        addJsonObject {
                            put("id", JsonPrimitive("device1"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("device2"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("device3"))
                        }
                    },
                    drivers = buildJsonArray {
                        addJsonObject {
                            put("id", JsonPrimitive("driver1"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("driver2"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("driver3"))
                        }
                    },
                    features = buildJsonObject { put("id", JsonPrimitive("feature1")) },
                    hasTouchScreen = true,
                    monitors = buildJsonArray {
                        addJsonObject {
                            put("id", JsonPrimitive("monitor1"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("monitor2"))
                        }
                        addJsonObject {
                            put("id", JsonPrimitive("monitor3"))
                        }
                    },
                ),
                locales = listOf("en-CA", "en-US"),
                platform = WebCompatInfoDto.WebCompatBrowserDto.PlatformDto(
                    fissionEnabled = false,
                    memoryMB = 1,
                ),
                prefs = WebCompatInfoDto.WebCompatBrowserDto.PrefsDto(
                    browserOpaqueResponseBlocking = false,
                    extensionsInstallTriggerEnabled = false,
                    gfxWebRenderSoftware = false,
                    networkCookieBehavior = 1,
                    privacyGlobalPrivacyControlEnabled = false,
                    privacyResistFingerprinting = false,
                ),
            ),
            url = "https://www.mozilla.org",
            devicePixelRatio = 1.5,
            frameworks = WebCompatInfoDto.WebCompatFrameworksDto(
                fastclick = true,
                marfeel = true,
                mobify = true,
            ),
            languages = listOf("en-CA", "en-US"),
            userAgent = "testUserAgent",
        )
        assertEquals(expected, actual)
    }

    private fun createService(engineSession: EngineSession, nimbusExperimentsProvider: NimbusExperimentsProvider = FakeNimbusExperimentsProvider()): WebCompatReporterRetrievalService {
        val tab = createTab(
            url = "https://www.mozilla.org",
            id = "test-tab",
            engineSession = engineSession,
        )
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            ),
        )

        return DefaultWebCompatReporterRetrievalService(
            browserStore = browserStore,
            webCompatInfoDeserializer = webCompatInfoDeserializer,
            nimbusExperimentsProvider = nimbusExperimentsProvider,
        )
    }
}

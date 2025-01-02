/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.store

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.GleanMetrics.BrokenSiteReport
import org.mozilla.fenix.GleanMetrics.BrokenSiteReportBrowserInfoApp
import org.mozilla.fenix.GleanMetrics.BrokenSiteReportBrowserInfoGraphics
import org.mozilla.fenix.GleanMetrics.BrokenSiteReportBrowserInfoPrefs
import org.mozilla.fenix.GleanMetrics.BrokenSiteReportBrowserInfoSystem
import org.mozilla.fenix.GleanMetrics.BrokenSiteReportTabInfo
import org.mozilla.fenix.GleanMetrics.BrokenSiteReportTabInfoAntitracking
import org.mozilla.fenix.GleanMetrics.BrokenSiteReportTabInfoFrameworks
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.webcompat.retrievalservice.WebCompatInfoDto
import org.mozilla.fenix.webcompat.retrievalservice.WebCompatReporterRetrievalService

/**
 * [Middleware] that reacts to submission related [WebCompatReporterAction]s.
 *
 * @param webCompatReporterRetrievalService The service that handles submission requests.
 * @param scope The [CoroutineScope] for launching coroutines.
 */
class WebCompatReporterSubmissionMiddleware(
    private val webCompatReporterRetrievalService: WebCompatReporterRetrievalService,
    private val scope: CoroutineScope,
) : Middleware<WebCompatReporterState, WebCompatReporterAction> {

    override fun invoke(
        context: MiddlewareContext<WebCompatReporterState, WebCompatReporterAction>,
        next: (WebCompatReporterAction) -> Unit,
        action: WebCompatReporterAction,
    ) {
        next(action)

        when (action) {
            is WebCompatReporterAction.SendReportClicked -> {
                scope.launch {
                    val webCompatInfo = webCompatReporterRetrievalService.retrieveInfo()
                    webCompatInfo?.let {
                        val enteredUrlMatchesTabUrl = context.state.enteredUrl == webCompatInfo.url
                        if (enteredUrlMatchesTabUrl) {
                            setTabAntiTrackingMetrics(antiTracking = webCompatInfo.antitracking)
                            setTabFrameworksMetrics(frameworks = webCompatInfo.frameworks)
                            setTabLanguageMetrics(languages = webCompatInfo.languages)
                            setTabUserAgentMetrics(userAgent = webCompatInfo.userAgent)
                        }

                        setBrowserInfoMetrics(browserInfo = webCompatInfo.browser)
                        setDevicePixelRatioMetrics(devicePixelRatio = webCompatInfo.devicePixelRatio)
                    }
                    setUrlMetrics(url = context.state.enteredUrl)
                    setReasonMetrics(reason = context.state.reason)
                    setDescriptionMetrics(description = context.state.problemDescription)

                    Pings.brokenSiteReport.submit()
                    context.store.dispatch(WebCompatReporterAction.ReportSubmitted)
                }
            }
            else -> {}
        }
    }

    private fun setTabAntiTrackingMetrics(antiTracking: WebCompatInfoDto.WebCompatAntiTrackingDto) {
        BrokenSiteReportTabInfoAntitracking.blockList.set(antiTracking.blockList)
        BrokenSiteReportTabInfoAntitracking.btpHasPurgedSite.set(antiTracking.btpHasPurgedSite)
        BrokenSiteReportTabInfoAntitracking.hasMixedActiveContentBlocked.set(
            antiTracking.hasMixedActiveContentBlocked,
        )
        BrokenSiteReportTabInfoAntitracking.hasMixedDisplayContentBlocked.set(
            antiTracking.hasMixedDisplayContentBlocked,
        )
        BrokenSiteReportTabInfoAntitracking.hasTrackingContentBlocked.set(
            antiTracking.hasTrackingContentBlocked,
        )
        BrokenSiteReportTabInfoAntitracking.isPrivateBrowsing.set(antiTracking.isPrivateBrowsing)
    }

    private fun setBrowserInfoMetrics(browserInfo: WebCompatInfoDto.WebCompatBrowserDto) {
        browserInfo.app?.let {
            BrokenSiteReportBrowserInfoApp.defaultUseragentString.set(it.defaultUserAgent)
        }

        BrokenSiteReportBrowserInfoApp.defaultLocales.set(browserInfo.locales)

        BrokenSiteReportBrowserInfoApp.fissionEnabled.set(browserInfo.platform.fissionEnabled)
        BrokenSiteReportBrowserInfoSystem.memory.set(browserInfo.platform.memoryMB)

        setBrowserInfoGraphicsMetrics(browserInfo.graphics)
        setBrowserInfoPrefsMetrics(browserInfo.prefs)
    }

    private fun setBrowserInfoGraphicsMetrics(graphicsInfo: WebCompatInfoDto.WebCompatBrowserDto.GraphicsDto?) {
        graphicsInfo?.let {
            it.devices?.let { devices ->
                BrokenSiteReportBrowserInfoGraphics.devicesJson.set(devices.toString())
            }

            BrokenSiteReportBrowserInfoGraphics.driversJson.set(it.drivers.toString())

            it.features?.let { features ->
                BrokenSiteReportBrowserInfoGraphics.featuresJson.set(features.toString())
            }

            it.hasTouchScreen?.let { hasTouchScreen ->
                BrokenSiteReportBrowserInfoGraphics.hasTouchScreen.set(hasTouchScreen)
            }

            it.monitors?.let { monitors ->
                BrokenSiteReportBrowserInfoGraphics.monitorsJson.set(monitors.toString())
            }
        }
    }

    private fun setBrowserInfoPrefsMetrics(prefsInfo: WebCompatInfoDto.WebCompatBrowserDto.PrefsDto) {
        BrokenSiteReportBrowserInfoPrefs.opaqueResponseBlocking.set(prefsInfo.browserOpaqueResponseBlocking)
        BrokenSiteReportBrowserInfoPrefs.installtriggerEnabled.set(prefsInfo.extensionsInstallTriggerEnabled)
        BrokenSiteReportBrowserInfoPrefs.softwareWebrender.set(prefsInfo.gfxWebRenderSoftware)
        BrokenSiteReportBrowserInfoPrefs.cookieBehavior.set(prefsInfo.networkCookieBehavior)
        BrokenSiteReportBrowserInfoPrefs.globalPrivacyControlEnabled.set(prefsInfo.privacyGlobalPrivacyControlEnabled)
        BrokenSiteReportBrowserInfoPrefs.resistFingerprintingEnabled.set(prefsInfo.privacyResistFingerprinting)
    }

    private fun setDevicePixelRatioMetrics(devicePixelRatio: Double) {
        BrokenSiteReportBrowserInfoGraphics.devicePixelRatio.set(devicePixelRatio.toString())
    }

    private fun setTabFrameworksMetrics(frameworks: WebCompatInfoDto.WebCompatFrameworksDto) {
        BrokenSiteReportTabInfoFrameworks.fastclick.set(frameworks.fastclick)
        BrokenSiteReportTabInfoFrameworks.marfeel.set(frameworks.marfeel)
        BrokenSiteReportTabInfoFrameworks.mobify.set(frameworks.mobify)
    }

    private fun setTabLanguageMetrics(languages: List<String>) {
        BrokenSiteReportTabInfo.languages.set(languages)
    }

    private fun setUrlMetrics(url: String) {
        BrokenSiteReport.url.set(url)
    }

    private fun setReasonMetrics(reason: WebCompatReporterState.BrokenSiteReason?) {
        reason?.let {
            BrokenSiteReport.breakageCategory.set(reason.name)
        }
    }

    private fun setDescriptionMetrics(description: String) {
        BrokenSiteReport.description.set(description)
    }

    private fun setTabUserAgentMetrics(userAgent: String) {
        BrokenSiteReportTabInfo.useragentString.set(userAgent)
    }
}

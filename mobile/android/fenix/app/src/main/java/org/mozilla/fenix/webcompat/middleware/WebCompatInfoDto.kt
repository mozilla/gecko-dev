/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.middleware

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject

/**
 * WebCompat data wrapper class.
 *
 * @property antitracking The anti-tracking data associated with the current tab.
 * @property browser The browser data.
 * @property devicePixelRatio The device pixel ratio.
 * @property frameworks The frameworks data associated with the current tab.
 * @property languages The languages associated with the current tab.
 * @property url The url associated with the current tab.
 * @property userAgent The user agent associated with the current tab.
 */
@Serializable
data class WebCompatInfoDto(
    val antitracking: WebCompatAntiTrackingDto,
    val browser: WebCompatBrowserDto,
    val devicePixelRatio: Double,
    val frameworks: WebCompatFrameworksDto,
    val languages: List<String>,
    val url: String,
    val userAgent: String,
) {

    /**
     * WebCompat anti-tracking data associated with the current tab.
     *
     * @property blockList The blocklist string.
     * @property btpHasPurgedSite Whether the current tab has recently been purged by Bounce Tracking Protection.
     * @property hasMixedActiveContentBlocked Whether the current tab has mixed active content blocked.
     * @property hasMixedDisplayContentBlocked Whether the current tab has mixed display content blocked.
     * @property hasTrackingContentBlocked Whether the current tab has tracking content blocked.
     * @property isPrivateBrowsing Whether the current tab is in private browsing mode.
     */
    @Serializable
    data class WebCompatAntiTrackingDto(
        val blockList: String,
        val btpHasPurgedSite: Boolean,
        val hasMixedActiveContentBlocked: Boolean,
        val hasMixedDisplayContentBlocked: Boolean,
        val hasTrackingContentBlocked: Boolean,
        val isPrivateBrowsing: Boolean,
    )

    /**
     * WebCompat browser data.
     *
     * @property app The app data.
     * @property graphics The graphics data.
     * @property locales The locales data.
     * @property platform The platform data.
     * @property prefs The prefs data.
     */
    @Serializable
    data class WebCompatBrowserDto(
        val app: AppDto? = null,
        val graphics: GraphicsDto? = null,
        val locales: List<String>,
        val platform: PlatformDto,
        val prefs: PrefsDto,
    ) {

        /**
         * WebCompat app data.
         *
         * @property defaultUserAgent The default user agent.
         */
        @Serializable
        data class AppDto(
            val defaultUserAgent: String,
        )

        /**
         * WebCompat graphics data.
         *
         * @property devices The devices data, with vendorID and deviceID.
         * @property drivers The drivers data, with renderer and version.
         * @property features The features data.
         * @property hasTouchScreen Whether the device has a touch screen.
         * @property monitors The monitors data, with screenWidth, screenHeight, and scale.
         */
        @Serializable
        data class GraphicsDto(
            val devices: JsonArray? = null,
            val drivers: JsonArray,
            val features: JsonObject? = null,
            val hasTouchScreen: Boolean? = null,
            val monitors: JsonArray? = null,
        )

        /**
         * WebCompat platform data.
         *
         * @property fissionEnabled Whether or not Fission is enabled.
         * @property memoryMB The amount of RAM the device has in MB.
         */
        @Serializable
        data class PlatformDto(
            val fissionEnabled: Boolean,
            val memoryMB: Long,
        )

        /**
         * WebCompat prefs data.
         *
         * @property browserOpaqueResponseBlocking The value of browser.opaqueResponseBlocking.
         * @property extensionsInstallTriggerEnabled The value of extensions.InstallTrigger.enabled.
         * @property gfxWebRenderSoftware The value of gfx.webrender.software.
         * @property networkCookieBehavior The value of network.cookie.cookieBehavior.
         * @property privacyGlobalPrivacyControlEnabled The value of privacy.globalprivacycontrol.enabled.
         * @property privacyResistFingerprinting The value of privacy.resistFingerprinting.
         */
        @Serializable
        data class PrefsDto(
            @SerialName(BROWSER_OPAQUE_RESPONSE_BLOCKING_KEY) val browserOpaqueResponseBlocking: Boolean,
            @SerialName(EXTENSIONS_INSTALL_TRIGGER_ENABLED_KEY) val extensionsInstallTriggerEnabled: Boolean,
            @SerialName(GFX_WEB_RENDER_SOFTWARE_KEY) val gfxWebRenderSoftware: Boolean,
            @SerialName(NETWORK_COOKIE_BEHAVIOR_KEY) val networkCookieBehavior: Long,
            @SerialName(PRIVACY_GLOBAL_PRIVACY_CONTROL_ENABLED_KEY)
            val privacyGlobalPrivacyControlEnabled: Boolean,
            @SerialName(PRIVACY_RESIST_FINGERPRINTING_KEY) val privacyResistFingerprinting: Boolean,
        ) {
            /**
             * @see [PrefsDto].
             */
            companion object {
                private const val BROWSER_OPAQUE_RESPONSE_BLOCKING_KEY = "browser.opaqueResponseBlocking"

                private const val EXTENSIONS_INSTALL_TRIGGER_ENABLED_KEY = "extensions.InstallTrigger.enabled"

                private const val GFX_WEB_RENDER_SOFTWARE_KEY = "gfx.webrender.software"

                private const val NETWORK_COOKIE_BEHAVIOR_KEY = "network.cookie.cookieBehavior"

                private const val PRIVACY_GLOBAL_PRIVACY_CONTROL_ENABLED_KEY = "privacy.globalprivacycontrol.enabled"

                private const val PRIVACY_RESIST_FINGERPRINTING_KEY = "privacy.resistFingerprinting"
            }
        }
    }

    /**
     * WebCompat frameworks data.
     *
     * @property fastclick Whether the FastClick web library was detected on the current tab.
     * @property marfeel Whether the Marfeel web framework was detected on the current tab.
     * @property mobify Whether the Mobify web framework was detected on the original tab.
     */
    @Serializable
    data class WebCompatFrameworksDto(
        val fastclick: Boolean,
        val marfeel: Boolean,
        val mobify: Boolean,
    )
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.Context
import android.view.View
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.contextmenu.ContextMenuCandidate
import mozilla.components.feature.customtabs.CustomTabWindowFeature
import mozilla.components.feature.pwa.feature.ManifestUpdateFeature
import mozilla.components.feature.pwa.feature.WebAppActivityFeature
import mozilla.components.feature.pwa.feature.WebAppContentFeature
import mozilla.components.feature.pwa.feature.WebAppHideToolbarFeature
import mozilla.components.feature.pwa.feature.WebAppSiteControlsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import mozilla.components.support.ktx.kotlin.isContentUrl
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BaseBrowserFragment
import org.mozilla.fenix.browser.ContextMenuSnackbarDelegate
import org.mozilla.fenix.browser.CustomTabContextMenuCandidate
import org.mozilla.fenix.components.toolbar.BrowserToolbarView
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.getCookieBannerUIMode

/**
 * Fragment used for browsing the web within external apps.
 */
class ExternalAppBrowserFragment : BaseBrowserFragment() {

    private val args by navArgs<ExternalAppBrowserFragmentArgs>()

    private val customTabsIntegration = ViewBoundFeatureWrapper<CustomTabsIntegration>()
    private val windowFeature = ViewBoundFeatureWrapper<CustomTabWindowFeature>()
    private val hideToolbarFeature = ViewBoundFeatureWrapper<WebAppHideToolbarFeature>()

    @Suppress("LongMethod", "ComplexMethod")
    override fun initializeUI(view: View, tab: SessionState) {
        super.initializeUI(view, tab)

        val customTabSessionId = customTabSessionId ?: return
        val activity = requireActivity()
        val components = activity.components

        val manifest = args.webAppManifestUrl?.ifEmpty { null }?.let { url ->
            requireComponents.core.webAppManifestStorage.getManifestCache(url)
        }

        ((browserToolbarView as? BrowserToolbarView)?.toolbar as? BrowserToolbar)?.let {
            customTabsIntegration.set(
                feature = CustomTabsIntegration(
                    context = requireContext(),
                    store = requireComponents.core.store,
                    useCases = requireComponents.useCases.customTabsUseCases,
                    browserToolbar = it,
                    sessionId = customTabSessionId,
                    activity = activity,
                    interactor = browserToolbarInteractor,
                    isPrivate = tab.content.private,
                    shouldReverseItems = !activity.settings().shouldUseBottomToolbar,
                    isSandboxCustomTab = args.isSandboxCustomTab,
                    isMenuRedesignEnabled = requireContext().settings().enableMenuRedesign,
                ),
                owner = this,
                view = view,
            )
        }

        windowFeature.set(
            feature = CustomTabWindowFeature(activity, components.core.store, customTabSessionId),
            owner = this,
            view = view,
        )

        val customTabSession = (tab as? CustomTabSessionState)
        val isPwaTabOrTwaTab = customTabSession?.config?.externalAppType == ExternalAppType.PROGRESSIVE_WEB_APP ||
            customTabSession?.config?.externalAppType == ExternalAppType.TRUSTED_WEB_ACTIVITY

        // Only set hideToolbarFeature if isPwaTabOrTwaTab
        if (isPwaTabOrTwaTab) {
            hideToolbarFeature.set(
                feature = WebAppHideToolbarFeature(
                    store = requireComponents.core.store,
                    customTabsStore = requireComponents.core.customTabsStore,
                    tabId = customTabSessionId,
                    manifest = manifest,
                ) { toolbarVisible ->
                    webAppToolbarShouldBeVisible = toolbarVisible
                    when (toolbarVisible) {
                        true -> collapseBrowserView()
                        false -> expandBrowserView()
                    }
                },
                owner = this,
                view = view,
            )
        }

        if (manifest != null) {
            activity.lifecycle.addObservers(
                WebAppActivityFeature(
                    activity,
                    components.core.icons,
                    manifest,
                ),
                WebAppContentFeature(
                    store = requireComponents.core.store,
                    tabId = customTabSessionId,
                    manifest,
                ),
                ManifestUpdateFeature(
                    activity.applicationContext,
                    requireComponents.core.store,
                    requireComponents.core.webAppShortcutManager,
                    requireComponents.core.webAppManifestStorage,
                    customTabSessionId,
                    manifest,
                ),
            )
            viewLifecycleOwner.lifecycle.addObserver(
                WebAppSiteControlsFeature(
                    activity.applicationContext,
                    requireComponents.core.store,
                    requireComponents.useCases.sessionUseCases.reload,
                    customTabSessionId,
                    manifest,
                    WebAppSiteControlsBuilder(
                        requireComponents.core.store,
                        requireComponents.useCases.sessionUseCases.reload,
                        customTabSessionId,
                        manifest,
                    ),
                    notificationsDelegate = requireComponents.notificationsDelegate,
                ),
            )
        } else {
            viewLifecycleOwner.lifecycle.addObserver(
                PoweredByNotification(
                    activity.applicationContext,
                    requireComponents.core.store,
                    customTabSessionId,
                    requireComponents.notificationsDelegate,
                ),
            )
        }
    }

    override fun navToQuickSettingsSheet(tab: SessionState, sitePermissions: SitePermissions?) {
        requireComponents.useCases.trackingProtectionUseCases.containsException(tab.id) { contains ->
            lifecycleScope.launch {
                val cookieBannersStorage = requireComponents.core.cookieBannersStorage
                val cookieBannerUIMode = cookieBannersStorage.getCookieBannerUIMode(
                    requireContext(),
                    tab,
                )
                withContext(Dispatchers.Main) {
                    runIfFragmentIsAttached {
                        val directions = ExternalAppBrowserFragmentDirections
                            .actionGlobalQuickSettingsSheetDialogFragment(
                                sessionId = tab.id,
                                url = tab.content.url,
                                title = tab.content.title,
                                isLocalPdf = tab.content.url.isContentUrl(),
                                isSecured = tab.content.securityInfo.secure,
                                sitePermissions = sitePermissions,
                                gravity = getAppropriateLayoutGravity(),
                                certificateName = tab.content.securityInfo.issuer,
                                permissionHighlights = tab.content.permissionHighlights,
                                isTrackingProtectionEnabled = tab.trackingProtection.enabled && !contains,
                                cookieBannerUIMode = cookieBannerUIMode,
                            )
                        nav(R.id.externalAppBrowserFragment, directions)
                    }
                }
            }
        }
    }

    override fun getContextMenuCandidates(
        context: Context,
        view: View,
    ): List<ContextMenuCandidate> = CustomTabContextMenuCandidate.defaultCandidates(
        context,
        context.components.useCases.contextMenuUseCases,
        view,
        ContextMenuSnackbarDelegate(),
    )
}

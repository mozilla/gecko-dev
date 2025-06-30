/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.browser

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.CallSuper
import androidx.fragment.app.Fragment
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.downloads.manager.FetchDownloadManager
import mozilla.components.feature.privatemode.feature.SecureWindowFeature
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.feature.session.CoordinateScrollingFeature
import mozilla.components.feature.session.SessionFeature
import mozilla.components.feature.session.SwipeRefreshFeature
import mozilla.components.feature.sitepermissions.SitePermissionsFeature
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.AutoplayAction
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.lib.state.ext.consumeFlow
import mozilla.components.support.base.feature.ActivityResultHandler
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.arch.lifecycle.addObservers
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import mozilla.components.support.locale.ActivityContextWrapper
import mozilla.components.support.utils.ext.requestInPlacePermissions
import org.mozilla.samples.browser.databinding.FragmentBrowserBinding
import org.mozilla.samples.browser.downloads.DownloadService
import org.mozilla.samples.browser.ext.components
import org.mozilla.samples.browser.integration.ContextMenuIntegration
import org.mozilla.samples.browser.integration.FindInPageIntegration

/**
 * Base fragment extended by [BrowserFragment] and [ExternalAppBrowserFragment].
 * This class only contains shared code focused on the main browsing content.
 * UI code specific to the app or to custom tabs can be found in the subclasses.
 */
@SuppressWarnings("LargeClass")
abstract class BaseBrowserFragment : Fragment(), UserInteractionHandler, ActivityResultHandler {
    private val sessionFeature = ViewBoundFeatureWrapper<SessionFeature>()
    private val toolbarFeature = ViewBoundFeatureWrapper<ToolbarFeature>()
    private val contextMenuIntegration = ViewBoundFeatureWrapper<ContextMenuIntegration>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()
    private val promptFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val findInPageIntegration = ViewBoundFeatureWrapper<FindInPageIntegration>()
    private val sitePermissionsFeature = ViewBoundFeatureWrapper<SitePermissionsFeature>()
    private val swipeRefreshFeature = ViewBoundFeatureWrapper<SwipeRefreshFeature>()

    protected val sessionId: String?
        get() = arguments?.getString(SESSION_ID_KEY)

    private val activityResultHandler: List<ViewBoundFeatureWrapper<*>> = listOf(
        promptFeature,
    )

    private var _binding: FragmentBrowserBinding? = null
    val binding get() = _binding!!

    @CallSuper
    @Suppress("LongMethod")
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        _binding = FragmentBrowserBinding.inflate(inflater, container, false)

        binding.toolbar.display.menuBuilder = components.menuBuilder
        val originalContext = ActivityContextWrapper.getOriginalContext(requireActivity())
        binding.engineView.setActivityContext(originalContext)

        sessionFeature.set(
            feature = SessionFeature(
                components.store,
                components.sessionUseCases.goBack,
                components.sessionUseCases.goForward,
                binding.engineView,
                sessionId,
            ),
            owner = this,
            view = binding.root,
        )

        toolbarFeature.set(
            feature = ToolbarFeature(
                binding.toolbar,
                components.store,
                components.sessionUseCases.loadUrl,
                components.defaultSearchUseCase,
                sessionId,
            ),
            owner = this,
            view = binding.root,
        )

        binding.toolbar.display.indicators += listOf(
            DisplayToolbar.Indicators.TRACKING_PROTECTION,
            DisplayToolbar.Indicators.HIGHLIGHT,
        )

        swipeRefreshFeature.set(
            feature = SwipeRefreshFeature(
                components.store,
                components.sessionUseCases.reload,
                binding.swipeToRefresh,
            ),
            owner = this,
            view = binding.root,
        )

        downloadsFeature.set(
            feature = DownloadsFeature(
                requireContext().applicationContext,
                store = components.store,
                useCases = components.downloadsUseCases,
                fragmentManager = childFragmentManager,
                onDownloadStopped = { download, id, status ->
                    Logger.debug("Download done. ID#$id $download with status $status")
                },
                downloadManager = FetchDownloadManager(
                    requireContext().applicationContext,
                    components.store,
                    DownloadService::class,
                    notificationsDelegate = components.notificationsDelegate,
                ),
                tabId = sessionId,
                onNeedToRequestPermissions = { permissions ->
                    requestInPlacePermissions(REQUEST_KEY_DOWNLOAD_PERMISSIONS, permissions) { result ->
                        downloadsFeature.get()?.onPermissionsResult(
                            result.keys.toTypedArray(),
                            result.values.map {
                                when (it) {
                                    true -> PackageManager.PERMISSION_GRANTED
                                    false -> PackageManager.PERMISSION_DENIED
                                }
                            }.toIntArray(),
                        )
                    }
                },
            ),
            owner = this,
            view = binding.root,
        )

        val scrollFeature = CoordinateScrollingFeature(components.store, binding.engineView, binding.toolbar)

        contextMenuIntegration.set(
            feature = ContextMenuIntegration(
                context = requireContext(),
                fragmentManager = parentFragmentManager,
                browserStore = components.store,
                tabsUseCases = components.tabsUseCases,
                contextMenuUseCases = components.contextMenuUseCases,
                parentView = binding.root,
                sessionId = sessionId,
            ),
            owner = this,
            view = binding.root,
        )

        promptFeature.set(
            feature = PromptFeature(
                fragment = this,
                store = components.store,
                customTabId = sessionId,
                tabsUseCases = components.tabsUseCases,
                fragmentManager = parentFragmentManager,
                fileUploadsDirCleaner = components.fileUploadsDirCleaner,
                onNeedToRequestPermissions = { permissions ->
                    requestInPlacePermissions(REQUEST_KEY_PROMPT_PERMISSIONS, permissions) { result ->
                        promptFeature.get()?.onPermissionsResult(
                            result.keys.toTypedArray(),
                            result.values.map {
                                when (it) {
                                    true -> PackageManager.PERMISSION_GRANTED
                                    false -> PackageManager.PERMISSION_DENIED
                                }
                            }.toIntArray(),
                        )
                    }
                },
            ),
            owner = this,
            view = binding.root,
        )

        sitePermissionsFeature.set(
            feature = SitePermissionsFeature(
                context = requireContext(),
                sessionId = sessionId,
                storage = components.permissionStorage,
                fragmentManager = parentFragmentManager,
                sitePermissionsRules = SitePermissionsRules(
                    autoplayAudible = AutoplayAction.BLOCKED,
                    autoplayInaudible = AutoplayAction.BLOCKED,
                    camera = SitePermissionsRules.Action.ASK_TO_ALLOW,
                    location = SitePermissionsRules.Action.ASK_TO_ALLOW,
                    notification = SitePermissionsRules.Action.ASK_TO_ALLOW,
                    microphone = SitePermissionsRules.Action.ASK_TO_ALLOW,
                    persistentStorage = SitePermissionsRules.Action.ASK_TO_ALLOW,
                    mediaKeySystemAccess = SitePermissionsRules.Action.ASK_TO_ALLOW,
                    crossOriginStorageAccess = SitePermissionsRules.Action.ASK_TO_ALLOW,
                ),
                onNeedToRequestPermissions = { permissions ->
                    requestInPlacePermissions(REQUEST_KEY_SITE_PERMISSIONS, permissions) { result ->
                        sitePermissionsFeature.get()?.onPermissionsResult(
                            result.keys.toTypedArray(),
                            result.values.map {
                                when (it) {
                                    true -> PackageManager.PERMISSION_GRANTED
                                    false -> PackageManager.PERMISSION_DENIED
                                }
                            }.toIntArray(),
                        )
                    }
                },
                onShouldShowRequestPermissionRationale = { shouldShowRequestPermissionRationale(it) },
                store = components.store,
            ),
            owner = this,
            view = binding.root,
        )

        findInPageIntegration.set(
            feature = FindInPageIntegration(components.store, binding.findInPage, binding.engineView),
            owner = this,
            view = binding.root,
        )

        val secureWindowFeature = SecureWindowFeature(
            window = requireActivity().window,
            store = components.store,
            customTabId = sessionId,
        )

        components.appLinksInterceptor.updateFragmentManger(
            fragmentManager = parentFragmentManager,
        )

        // Observe the lifecycle for supported features
        lifecycle.addObservers(
            scrollFeature,
            secureWindowFeature,
        )

        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        consumeFlow(components.store) { flow ->
            flow.mapNotNull { state -> state.findCustomTabOrSelectedTab(sessionId) }
                .ifAnyChanged { tab ->
                    arrayOf(
                        tab.content.loading,
                        tab.content.canGoBack,
                        tab.content.canGoForward,
                    )
                }
                .collect {
                    binding.toolbar.invalidateActions()
                }
        }
    }

    @CallSuper
    override fun onBackPressed(): Boolean =
        listOf(findInPageIntegration, toolbarFeature, sessionFeature).any { it.onBackPressed() }

    @CallSuper
    override fun onActivityResult(requestCode: Int, data: Intent?, resultCode: Int): Boolean {
        return activityResultHandler.any { it.onActivityResult(requestCode, data, resultCode) }
    }

    companion object {
        private const val SESSION_ID_KEY = "session_id"

        private const val REQUEST_KEY_DOWNLOAD_PERMISSIONS = "downloadFeature"
        private const val REQUEST_KEY_PROMPT_PERMISSIONS = "promptFeature"
        private const val REQUEST_KEY_SITE_PERMISSIONS = "sitePermissionsFeature"

        @JvmStatic
        protected fun Bundle.putSessionId(sessionId: String?) {
            putString(SESSION_ID_KEY, sessionId)
        }
    }
    override fun onDestroyView() {
        super.onDestroyView()
        components.appLinksInterceptor.updateFragmentManger(
            fragmentManager = null,
        )

        binding.engineView.setActivityContext(null)
        _binding = null
    }
}

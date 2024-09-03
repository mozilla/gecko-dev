/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.os.Bundle
import android.os.Environment
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import androidx.preference.PreferenceManager
import com.google.android.material.snackbar.Snackbar
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.concept.fetch.Response
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.feature.downloads.DownloadsFeature
import mozilla.components.feature.downloads.manager.FetchDownloadManager
import mozilla.components.feature.prompts.PromptFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import org.jetbrains.annotations.VisibleForTesting
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.databinding.DownloadDialogLayoutBinding
import org.mozilla.fenix.downloads.DownloadService
import org.mozilla.fenix.downloads.dialog.DynamicDownloadDialog
import org.mozilla.fenix.downloads.dialog.FirstPartyDownloadDialog
import org.mozilla.fenix.downloads.dialog.StartDownloadDialog
import org.mozilla.fenix.downloads.dialog.ThirdPartyDownloadDialog
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.theme.ThemeManager

/**
 * Provides shared functionality to our fragments for add-on settings and
 * browser/page action popups.
 */
abstract class AddonPopupBaseFragment : Fragment(), EngineSession.Observer, UserInteractionHandler {
    private val promptsFeature = ViewBoundFeatureWrapper<PromptFeature>()
    private val downloadsFeature = ViewBoundFeatureWrapper<DownloadsFeature>()

    protected var session: SessionState? = null
    protected var engineSession: EngineSession? = null
    private var canGoBack: Boolean = false
    private var currentStartDownloadDialog: StartDownloadDialog? = null

    @Suppress("DEPRECATION", "LongMethod")
    // https://github.com/mozilla-mobile/fenix/issues/19920
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        session?.let {
            promptsFeature.set(
                feature = PromptFeature(
                    fragment = this,
                    store = requireComponents.core.store,
                    customTabId = it.id,
                    fragmentManager = parentFragmentManager,
                    fileUploadsDirCleaner = requireComponents.core.fileUploadsDirCleaner,
                    onNeedToRequestPermissions = { permissions ->
                        requestPermissions(permissions, REQUEST_CODE_PROMPT_PERMISSIONS)
                    },
                    tabsUseCases = requireComponents.useCases.tabsUseCases,
                ),
                owner = this,
                view = view,
            )
            val downloadFeature = DownloadsFeature(
                requireContext().applicationContext,
                store = provideBrowserStore(),
                useCases = requireContext().components.useCases.downloadUseCases,
                fragmentManager = childFragmentManager,
                tabId = it.id,
                downloadManager = FetchDownloadManager(
                    requireContext().applicationContext,
                    provideBrowserStore(),
                    DownloadService::class,
                    notificationsDelegate = requireContext().components.notificationsDelegate,
                ),
                shouldForwardToThirdParties = {
                    PreferenceManager.getDefaultSharedPreferences(requireContext()).getBoolean(
                        requireContext().getPreferenceKey(R.string.pref_key_external_download_manager),
                        false,
                    )
                },
                promptsStyling = DownloadsFeature.PromptsStyling(
                    gravity = Gravity.BOTTOM,
                    shouldWidthMatchParent = true,
                    positiveButtonBackgroundColor = ThemeManager.resolveAttribute(
                        R.attr.accent,
                        requireContext(),
                    ),
                    positiveButtonTextColor = ThemeManager.resolveAttribute(
                        R.attr.textOnColorPrimary,
                        requireContext(),
                    ),
                    positiveButtonRadius = (resources.getDimensionPixelSize(R.dimen.tab_corner_radius)).toFloat(),
                ),
                onNeedToRequestPermissions = { permissions ->
                    requestPermissions(permissions, REQUEST_CODE_DOWNLOAD_PERMISSIONS)
                },
                customFirstPartyDownloadDialog = { filename, contentSize, positiveAction, negativeAction ->
                    run {
                        if (currentStartDownloadDialog == null) {
                            requireContext().components.analytics.crashReporter.recordCrashBreadcrumb(
                                Breadcrumb("FirstPartyDownloadDialog created"),
                            )
                            FirstPartyDownloadDialog(
                                activity = requireActivity(),
                                filename = filename.value,
                                contentSize = contentSize.value,
                                positiveButtonAction = positiveAction.value,
                                negativeButtonAction = negativeAction.value,
                            ).onDismiss {
                                requireContext().components.analytics.crashReporter.recordCrashBreadcrumb(
                                    Breadcrumb("FirstPartyDownloadDialog onDismiss"),
                                )
                                currentStartDownloadDialog = null
                            }.show(provideDownloadContainer())
                                .also {
                                    currentStartDownloadDialog = it
                                }
                        }
                    }
                },
                customThirdPartyDownloadDialog = { downloaderApps, onAppSelected, negativeActionCallback ->
                    run {
                        if (currentStartDownloadDialog == null) {
                            requireContext().components.analytics.crashReporter.recordCrashBreadcrumb(
                                Breadcrumb("ThirdPartyDownloadDialog created"),
                            )
                            ThirdPartyDownloadDialog(
                                activity = requireActivity(),
                                downloaderApps = downloaderApps.value,
                                onAppSelected = onAppSelected.value,
                                negativeButtonAction = negativeActionCallback.value,
                            ).onDismiss {
                                requireContext().components.analytics.crashReporter.recordCrashBreadcrumb(
                                    Breadcrumb("ThirdPartyDownloadDialog onDismiss"),
                                )
                                currentStartDownloadDialog = null
                            }.show(provideDownloadContainer()).also {
                                currentStartDownloadDialog = it
                            }
                        }
                    }
                },
            )
            downloadsFeature.set(
                downloadFeature,
                owner = this,
                view = view,
            )
            downloadFeature.onDownloadStopped = { downloadState, _, downloadJobStatus ->
                handleOnDownloadFinished(downloadState, downloadJobStatus, downloadFeature::tryAgain)
            }
        }
    }

    override fun onExternalResource(
        url: String,
        fileName: String?,
        contentLength: Long?,
        contentType: String?,
        cookie: String?,
        userAgent: String?,
        isPrivate: Boolean,
        skipConfirmation: Boolean,
        openInApp: Boolean,
        response: Response?,
    ) {
        session?.let { session ->
            val fileSize = if (contentLength != null && contentLength < 0) null else contentLength
            val download = DownloadState(
                url,
                fileName,
                contentType,
                fileSize,
                0,
                DownloadState.Status.INITIATED,
                userAgent,
                Environment.DIRECTORY_DOWNLOADS,
                private = isPrivate,
                skipConfirmation = skipConfirmation,
                openInApp = openInApp,
                response = response,
            )

            provideBrowserStore().dispatch(
                ContentAction.UpdateDownloadAction(
                    session.id,
                    download,
                ),
            )
        }
    }

    @VisibleForTesting
    internal fun provideBrowserStore() = requireComponents.core.store

    /**
     * Provides a container for download-related views.
     *
     * @return A ViewGroup that will contain the download views.
     */
    abstract fun provideDownloadContainer(): ViewGroup

    /**
     * Provides the layout binding for the download dialog.
     *
     * @return A DownloadDialogLayoutBinding instance that binds the download dialog layout.
     */
    abstract fun provideDownloadDialogLayoutBinding(): DownloadDialogLayoutBinding

    override fun onDestroyView() {
        engineSession?.close()
        session?.let {
            requireComponents.core.store.dispatch(CustomTabListAction.RemoveCustomTabAction(it.id))
        }
        super.onDestroyView()
    }

    override fun onStart() {
        super.onStart()
        engineSession?.register(this)
    }

    override fun onStop() {
        super.onStop()
        engineSession?.unregister(this)
        currentStartDownloadDialog?.dismiss()
    }

    override fun onPromptRequest(promptRequest: PromptRequest) {
        session?.let { session ->
            requireComponents.core.store.dispatch(
                ContentAction.UpdatePromptRequestAction(
                    session.id,
                    promptRequest,
                ),
            )
        }
    }

    override fun onWindowRequest(windowRequest: WindowRequest) {
        if (windowRequest.type == WindowRequest.Type.CLOSE) {
            findNavController().popBackStack()
        } else {
            engineSession?.loadUrl(windowRequest.url)
        }
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let { this.canGoBack = canGoBack }
    }

    override fun onBackPressed(): Boolean {
        return if (this.canGoBack) {
            engineSession?.goBack()
            true
        } else {
            false
        }
    }

    protected fun initializeSession(fromEngineSession: EngineSession? = null) {
        engineSession = fromEngineSession ?: requireComponents.core.engine.createSession()
        session = createCustomTab(
            url = "",
            source = SessionState.Source.Internal.CustomTab,
        ).copy(engineState = EngineState(engineSession))
        requireComponents.core.store.dispatch(CustomTabListAction.AddCustomTabAction(session as CustomTabSessionState))
    }

    final override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray,
    ) {
        when (requestCode) {
            REQUEST_CODE_PROMPT_PERMISSIONS -> promptsFeature.get()?.onPermissionsResult(permissions, grantResults)
            REQUEST_CODE_DOWNLOAD_PERMISSIONS -> promptsFeature.get()?.onPermissionsResult(permissions, grantResults)
        }
    }

    internal fun shouldShowCompletedDownloadDialog(
        downloadState: DownloadState,
        status: DownloadState.Status,
    ): Boolean {
        val isValidStatus = status in listOf(DownloadState.Status.COMPLETED, DownloadState.Status.FAILED)
        val isSameTab = downloadState.sessionId == (session?.id ?: false)

        return isValidStatus && isSameTab
    }

    internal fun handleOnDownloadFinished(
        downloadState: DownloadState,
        downloadJobStatus: DownloadState.Status,
        tryAgain: (String) -> Unit,
    ) {
        // If the download is just paused, don't show any in-app notification
        if (shouldShowCompletedDownloadDialog(downloadState, downloadJobStatus)) {
            val safeContext = context ?: return
            val onCannotOpenFile: (DownloadState) -> Unit = {
                FenixSnackbar.make(
                    view = requireView(),
                    duration = Snackbar.LENGTH_SHORT,
                ).setText(DynamicDownloadDialog.getCannotOpenFileErrorMessage(requireContext(), downloadState))
                    .show()
            }
            if (downloadState.openInApp && downloadJobStatus == DownloadState.Status.COMPLETED) {
                val fileWasOpened = AbstractFetchDownloadService.openFile(
                    applicationContext = safeContext.applicationContext,
                    download = downloadState,
                )
                if (!fileWasOpened) {
                    onCannotOpenFile(downloadState)
                }
            } else {
                val dynamicDownloadDialog = DynamicDownloadDialog(
                    context = safeContext,
                    downloadState = downloadState,
                    didFail = downloadJobStatus == DownloadState.Status.FAILED,
                    tryAgain = tryAgain,
                    onCannotOpenFile = onCannotOpenFile,
                    binding = provideDownloadDialogLayoutBinding(),
                ) {}
                dynamicDownloadDialog.show()
            }
        }
    }

    companion object {
        private const val REQUEST_CODE_PROMPT_PERMISSIONS = 1
        private const val REQUEST_CODE_DOWNLOAD_PERMISSIONS = 2
    }
}

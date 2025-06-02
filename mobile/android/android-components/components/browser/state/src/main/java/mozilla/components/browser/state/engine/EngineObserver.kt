/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine

import android.content.Intent
import android.os.Environment
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CookieBannerAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.MediaSessionAction
import mozilla.components.browser.state.action.ReaderAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.selector.findTabOrCustomTab
import mozilla.components.browser.state.state.AppIntentState
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.LoadRequestState
import mozilla.components.browser.state.state.SecurityInfoState
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.state.content.DownloadState.Status.INITIATED
import mozilla.components.browser.state.state.content.FindResultState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.HitResult
import mozilla.components.concept.engine.content.blocking.Tracker
import mozilla.components.concept.engine.history.HistoryItem
import mozilla.components.concept.engine.manifest.WebAppManifest
import mozilla.components.concept.engine.media.RecordingDevice
import mozilla.components.concept.engine.mediasession.MediaSession
import mozilla.components.concept.engine.permission.PermissionRequest
import mozilla.components.concept.engine.prompt.PromptRequest
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.concept.fetch.Response
import mozilla.components.lib.state.Store

private const val PAGE_LOAD_COMPLETION_PROGRESS = 100

/**
 * [EngineSession.Observer] implementation responsible to update the state of a [Session] from the events coming out of
 * an [EngineSession].
 */
@Suppress("TooManyFunctions", "LargeClass")
internal class EngineObserver(
    private val tabId: String,
    private val store: Store<BrowserState, BrowserAction>,
) : EngineSession.Observer {

    override fun onScrollChange(scrollX: Int, scrollY: Int) {
        store.dispatch(ReaderAction.UpdateReaderScrollYAction(tabId, scrollY))
    }

    override fun onNavigateBack() {
        store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, ""))
    }

    override fun onNavigateForward() {
        store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, ""))
    }

    override fun onGotoHistoryIndex() {
        store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, ""))
    }

    override fun onLoadData() {
        store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, ""))
    }

    override fun onLoadUrl() {
        if (store.state.findTabOrCustomTab(tabId)?.content?.isSearch == true) {
            store.dispatch(ContentAction.UpdateIsSearchAction(tabId, false))
        } else {
            store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, ""))
        }
    }

    override fun onFirstContentfulPaint() {
        store.dispatch(ContentAction.UpdateFirstContentfulPaintStateAction(tabId, true))
    }

    override fun onPaintStatusReset() {
        store.dispatch(ContentAction.UpdateFirstContentfulPaintStateAction(tabId, false))
    }

    override fun onLocationChange(url: String, hasUserGesture: Boolean) {
        store.dispatch(ContentAction.UpdateUrlAction(tabId, url, hasUserGesture))
    }

    @Suppress("DEPRECATION") // Session observable is deprecated
    override fun onLoadRequest(
        url: String,
        triggeredByRedirect: Boolean,
        triggeredByWebContent: Boolean,
    ) {
        if (triggeredByWebContent) {
            store.dispatch(ContentAction.UpdateSearchTermsAction(tabId, ""))
        }

        val loadRequest = LoadRequestState(url, triggeredByRedirect, triggeredByWebContent)
        store.dispatch(ContentAction.UpdateLoadRequestAction(tabId, loadRequest))
    }

    override fun onLaunchIntentRequest(url: String, appIntent: Intent?) {
        store.dispatch(ContentAction.UpdateAppIntentAction(tabId, AppIntentState(url, appIntent)))
    }

    override fun onTitleChange(title: String) {
        store.dispatch(ContentAction.UpdateTitleAction(tabId, title))
    }

    override fun onPreviewImageChange(previewImageUrl: String) {
        store.dispatch(ContentAction.UpdatePreviewImageAction(tabId, previewImageUrl))
    }

    override fun onProgress(progress: Int) {
        // page load is completed, start the translation initialization if not initialized yet
        // referencing to a field in the state is not recommended, this flow should be reconsidered
        // while the visual completeness logic is revisited in Bug 1966977.
        if (progress == PAGE_LOAD_COMPLETION_PROGRESS && !store.state.translationsInitialized) {
            store.dispatch(TranslationsAction.InitTranslationsBrowserState)
        }
        store.dispatch(ContentAction.UpdateProgressAction(tabId, progress))
    }

    override fun onLoadingStateChange(loading: Boolean) {
        store.dispatch(ContentAction.UpdateLoadingStateAction(tabId, loading))

        if (loading) {
            store.dispatch(ContentAction.ClearFindResultsAction(tabId))
            store.dispatch(ContentAction.UpdateRefreshCanceledStateAction(tabId, false))
            store.dispatch(TrackingProtectionAction.ClearTrackersAction(tabId))
        }
    }

    override fun onNavigationStateChange(canGoBack: Boolean?, canGoForward: Boolean?) {
        canGoBack?.let {
            store.dispatch(ContentAction.UpdateBackNavigationStateAction(tabId, canGoBack))
        }
        canGoForward?.let {
            store.dispatch(ContentAction.UpdateForwardNavigationStateAction(tabId, canGoForward))
        }
    }

    override fun onSecurityChange(secure: Boolean, host: String?, issuer: String?) {
        store.dispatch(
            ContentAction.UpdateSecurityInfoAction(
                tabId,
                SecurityInfoState(secure, host ?: "", issuer ?: ""),
            ),
        )
    }

    override fun onTrackerBlocked(tracker: Tracker) {
        store.dispatch(TrackingProtectionAction.TrackerBlockedAction(tabId, tracker))
    }

    override fun onTrackerLoaded(tracker: Tracker) {
        store.dispatch(TrackingProtectionAction.TrackerLoadedAction(tabId, tracker))
    }

    override fun onExcludedOnTrackingProtectionChange(excluded: Boolean) {
        store.dispatch(TrackingProtectionAction.ToggleExclusionListAction(tabId, excluded))
    }

    override fun onTrackerBlockingEnabledChange(enabled: Boolean) {
        store.dispatch(TrackingProtectionAction.ToggleAction(tabId, enabled))
    }

    override fun onCookieBannerChange(status: EngineSession.CookieBannerHandlingStatus) {
        store.dispatch(CookieBannerAction.UpdateStatusAction(tabId, status))
    }

    override fun onProductUrlChange(isProductUrl: Boolean) {
        store.dispatch(ContentAction.UpdateProductUrlStateAction(tabId, isProductUrl))
    }

    override fun onTranslatePageChange() {
        store.dispatch(TranslationsAction.SetTranslateProcessingAction(tabId, isProcessing = false))
    }

    override fun onLongPress(hitResult: HitResult) {
        store.dispatch(
            ContentAction.UpdateHitResultAction(tabId, hitResult),
        )
    }

    override fun onFind(text: String) {
        store.dispatch(ContentAction.ClearFindResultsAction(tabId))
    }

    override fun onFindResult(activeMatchOrdinal: Int, numberOfMatches: Int, isDoneCounting: Boolean) {
        store.dispatch(
            ContentAction.AddFindResultAction(
                tabId,
                FindResultState(
                    activeMatchOrdinal,
                    numberOfMatches,
                    isDoneCounting,
                ),
            ),
        )
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
        // We want to avoid negative contentLength values
        // For more info see https://bugzilla.mozilla.org/show_bug.cgi?id=1632594
        val fileSize = if (contentLength != null && contentLength < 0) null else contentLength
        val download = DownloadState(
            url,
            fileName,
            contentType,
            fileSize,
            0,
            INITIATED,
            userAgent,
            Environment.DIRECTORY_DOWNLOADS,
            private = isPrivate,
            skipConfirmation = skipConfirmation,
            openInApp = openInApp,
            response = response,
        )

        store.dispatch(
            ContentAction.UpdateDownloadAction(
                tabId,
                download,
            ),
        )
    }

    override fun onDesktopModeChange(enabled: Boolean) {
        store.dispatch(
            ContentAction.UpdateTabDesktopMode(
                tabId,
                enabled,
            ),
        )
    }

    override fun onFullScreenChange(enabled: Boolean) {
        store.dispatch(
            ContentAction.FullScreenChangedAction(
                tabId,
                enabled,
            ),
        )
    }

    override fun onMetaViewportFitChanged(layoutInDisplayCutoutMode: Int) {
        store.dispatch(
            ContentAction.ViewportFitChangedAction(
                tabId,
                layoutInDisplayCutoutMode,
            ),
        )
    }

    override fun onContentPermissionRequest(permissionRequest: PermissionRequest) {
        store.dispatch(
            ContentAction.UpdatePermissionsRequest(
                tabId,
                permissionRequest,
            ),
        )
    }

    override fun onCancelContentPermissionRequest(permissionRequest: PermissionRequest) {
        store.dispatch(
            ContentAction.ConsumePermissionsRequest(
                tabId,
                permissionRequest,
            ),
        )
    }

    override fun onAppPermissionRequest(permissionRequest: PermissionRequest) {
        store.dispatch(
            ContentAction.UpdateAppPermissionsRequest(
                tabId,
                permissionRequest,
            ),
        )
    }

    override fun onPromptRequest(promptRequest: PromptRequest) {
        store.dispatch(
            ContentAction.UpdatePromptRequestAction(
                tabId,
                promptRequest,
            ),
        )
    }

    override fun onPromptDismissed(promptRequest: PromptRequest) {
        store.dispatch(
            ContentAction.ConsumePromptRequestAction(tabId, promptRequest),
        )
    }

    override fun onPromptUpdate(previousPromptRequestUid: String, promptRequest: PromptRequest) {
        store.dispatch(
            ContentAction.ReplacePromptRequestAction(tabId, previousPromptRequestUid, promptRequest),
        )
    }

    override fun onRepostPromptCancelled() {
        store.dispatch(ContentAction.UpdateRefreshCanceledStateAction(tabId, true))
    }

    override fun onBeforeUnloadPromptDenied() {
        store.dispatch(ContentAction.UpdateRefreshCanceledStateAction(tabId, true))
    }

    override fun onWindowRequest(windowRequest: WindowRequest) {
        store.dispatch(
            ContentAction.UpdateWindowRequestAction(
                tabId,
                windowRequest,
            ),
        )
    }

    override fun onShowDynamicToolbar() {
        store.dispatch(
            ContentAction.UpdateExpandedToolbarStateAction(tabId, true),
        )
    }

    override fun onMediaActivated(mediaSessionController: MediaSession.Controller) {
        store.dispatch(
            MediaSessionAction.ActivatedMediaSessionAction(
                tabId,
                mediaSessionController,
            ),
        )
    }

    override fun onMediaDeactivated() {
        store.dispatch(MediaSessionAction.DeactivatedMediaSessionAction(tabId))
    }

    override fun onMediaMetadataChanged(metadata: MediaSession.Metadata) {
        store.dispatch(MediaSessionAction.UpdateMediaMetadataAction(tabId, metadata))
    }

    override fun onMediaPlaybackStateChanged(playbackState: MediaSession.PlaybackState) {
        store.dispatch(
            MediaSessionAction.UpdateMediaPlaybackStateAction(
                tabId,
                playbackState,
            ),
        )
    }

    override fun onMediaFeatureChanged(features: MediaSession.Feature) {
        store.dispatch(
            MediaSessionAction.UpdateMediaFeatureAction(
                tabId,
                features,
            ),
        )
    }

    override fun onMediaPositionStateChanged(positionState: MediaSession.PositionState) {
        store.dispatch(
            MediaSessionAction.UpdateMediaPositionStateAction(
                tabId,
                positionState,
            ),
        )
    }

    override fun onMediaMuteChanged(muted: Boolean) {
        store.dispatch(
            MediaSessionAction.UpdateMediaMutedAction(
                tabId,
                muted,
            ),
        )
    }

    override fun onMediaFullscreenChanged(
        fullscreen: Boolean,
        elementMetadata: MediaSession.ElementMetadata?,
    ) {
        store.dispatch(
            MediaSessionAction.UpdateMediaFullscreenAction(
                tabId,
                fullscreen,
                elementMetadata,
            ),
        )
    }

    override fun onWebAppManifestLoaded(manifest: WebAppManifest) {
        store.dispatch(ContentAction.UpdateWebAppManifestAction(tabId, manifest))
    }

    override fun onCrash() {
        store.dispatch(
            CrashAction.SessionCrashedAction(
                tabId,
            ),
        )
    }

    override fun onProcessKilled() {
        store.dispatch(
            EngineAction.KillEngineSessionAction(
                tabId,
            ),
        )
    }

    override fun onStateUpdated(state: EngineSessionState) {
        store.dispatch(
            EngineAction.UpdateEngineSessionStateAction(
                tabId,
                state,
            ),
        )
    }

    override fun onRecordingStateChanged(devices: List<RecordingDevice>) {
        store.dispatch(
            ContentAction.SetRecordingDevices(
                tabId,
                devices,
            ),
        )
    }

    override fun onHistoryStateChanged(historyList: List<HistoryItem>, currentIndex: Int) {
        store.dispatch(
            ContentAction.UpdateHistoryStateAction(
                tabId,
                historyList,
                currentIndex,
            ),
        )
    }

    override fun onSaveToPdfException(throwable: Throwable) {
        store.dispatch(EngineAction.SaveToPdfExceptionAction(tabId, throwable))
    }

    override fun onPrintFinish() {
        store.dispatch(EngineAction.PrintContentCompletedAction(tabId))
    }

    override fun onPrintException(isPrint: Boolean, throwable: Throwable) {
        store.dispatch(EngineAction.PrintContentExceptionAction(tabId, isPrint, throwable))
    }

    override fun onSaveToPdfComplete() {
        store.dispatch(EngineAction.SaveToPdfCompleteAction(tabId))
    }

    override fun onCheckForFormData(containsFormData: Boolean, adjustPriority: Boolean) {
        store.dispatch(ContentAction.UpdateHasFormDataAction(tabId, containsFormData, adjustPriority))
    }

    override fun onCheckForFormDataException(throwable: Throwable) {
        store.dispatch(ContentAction.CheckForFormDataExceptionAction(tabId, throwable))
    }

    override fun onTranslateExpected() {
        store.dispatch(TranslationsAction.TranslateExpectedAction(tabId))
    }

    override fun onTranslateOffer() {
        store.dispatch(TranslationsAction.TranslateOfferAction(tabId = tabId, isOfferTranslate = true))
    }

    override fun onTranslateStateChange(state: TranslationEngineState) {
        store.dispatch(TranslationsAction.TranslateStateChangeAction(tabId, state))
    }

    override fun onTranslateComplete(operation: TranslationOperation) {
        store.dispatch(TranslationsAction.TranslateSuccessAction(tabId, operation))
    }

    override fun onTranslateException(operation: TranslationOperation, translationError: TranslationError) {
        store.dispatch(TranslationsAction.TranslateExceptionAction(tabId, operation, translationError))
    }
}

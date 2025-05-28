/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.annotation.VisibleForTesting
import androidx.navigation.NavController
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TranslationsState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.initialFromLanguage
import mozilla.components.concept.engine.translate.initialToLanguage
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.GleanMetrics.Translations
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.store.BrowserScreenAction.PageTranslationStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.SnackbarAction
import org.mozilla.fenix.ext.navigateSafe
import org.mozilla.fenix.translations.TranslationDialogBottomSheet
import org.mozilla.fenix.translations.TranslationsFlowState

/**
 * A binding for observing [TranslationsState] changes
 * from the [BrowserStore] and updating the translations action button.
 *
 * @param browserStore [BrowserStore] observed for any changes related to [TranslationsState].
 * @param browserScreenStore [BrowserScreenStore] integrating all browsing features.
 * @param appStore [AppStore] integrating all application wide features.
 * @param navController [NavController] used for navigation.
 * @param onTranslationStatusUpdate Invoked when the translation status of the current page is updated.
 * @param onShowTranslationsDialog Invoked when [TranslationDialogBottomSheet]
 * should be automatically shown to the user.
 */
class TranslationsBinding(
    private val browserStore: BrowserStore,
    private val browserScreenStore: BrowserScreenStore? = null,
    private val appStore: AppStore? = null,
    private val navController: NavController? = null,
    private val onTranslationStatusUpdate: (PageTranslationStatus) -> Unit = { _ -> },
    private val onShowTranslationsDialog: () -> Unit = { },
) : AbstractBinding<BrowserState>(browserStore) {

    @Suppress("LongMethod")
    override suspend fun onState(flow: Flow<BrowserState>) {
        // Browser level flows
        val browserFlow = flow.mapNotNull { state -> state }
            .distinctUntilChangedBy {
                it.translationEngine
            }

        // Session level flows
        val sessionFlow = flow.mapNotNull { state -> state.selectedTab }
            .distinctUntilChangedBy {
                Pair(it.translationsState, it.readerState)
            }

        // Applying the flows together
        sessionFlow
            .combine(browserFlow) { sessionState, browserState ->
                TranslationsFlowState(
                    sessionState,
                    browserState,
                )
            }
            .collect { state ->
                // Browser Translations State Behavior (Global)
                val browserTranslationsState = state.browserState.translationEngine
                val translateFromLanguages =
                    browserTranslationsState.supportedLanguages?.fromLanguages
                val translateToLanguages =
                    browserTranslationsState.supportedLanguages?.toLanguages
                val isEngineSupported = browserTranslationsState.isEngineSupported

                // Session Translations State Behavior (Tab)
                val sessionTranslationsState = state.sessionState.translationsState

                if (state.sessionState.readerState.active) {
                    onTranslationStateUpdated(
                        PageTranslationStatus(
                            isTranslationPossible = false,
                            isTranslated = false,
                            isTranslateProcessing = false,
                        ),
                    )
                } else if (isEngineSupported == true && sessionTranslationsState.isTranslated) {
                    val fromSelected =
                        sessionTranslationsState.translationEngineState?.initialFromLanguage(
                            translateFromLanguages,
                        )
                    val toSelected =
                        sessionTranslationsState.translationEngineState?.initialToLanguage(
                            translateToLanguages,
                        )

                    if (fromSelected != null && toSelected != null) {
                        onTranslationStateUpdated(
                            PageTranslationStatus(
                                isTranslationPossible = true,
                                isTranslated = true,
                                isTranslateProcessing = sessionTranslationsState.isTranslateProcessing,
                                fromSelectedLanguage = fromSelected,
                                toSelectedLanguage = toSelected,
                            ),
                        )
                    }
                } else if (isEngineSupported == true && sessionTranslationsState.isExpectedTranslate) {
                    onTranslationStateUpdated(
                        PageTranslationStatus(
                            isTranslationPossible = true,
                            isTranslated = false,
                            isTranslateProcessing = sessionTranslationsState.isTranslateProcessing,
                        ),
                    )
                } else {
                    onTranslationStateUpdated(
                        PageTranslationStatus(
                            isTranslationPossible = false,
                            isTranslated = false,
                            isTranslateProcessing = false,
                        ),
                    )
                }

                if (isEngineSupported == true && sessionTranslationsState.isOfferTranslate) {
                    browserStore.dispatch(
                        TranslationsAction.TranslateOfferAction(
                            tabId = state.sessionState.id,
                            isOfferTranslate = false,
                        ),
                    )
                    offerToTranslateCurrentPage()
                }

                if (
                    isEngineSupported == true &&
                    sessionTranslationsState.isExpectedTranslate &&
                    sessionTranslationsState.translationError != null
                ) {
                    offerToTranslateCurrentPage()
                }
            }
    }

    private fun onTranslationStateUpdated(state: PageTranslationStatus) {
        onTranslationStatusUpdate(state)
        browserScreenStore?.dispatch(PageTranslationStatusUpdated(state))
        if (!state.isTranslateProcessing) {
            appStore?.dispatch(SnackbarAction.SnackbarDismissed)
        }
    }

    private fun offerToTranslateCurrentPage() = when (appStore != null && navController != null) {
        true -> {
            recordTranslationStartTelemetry()

            appStore.dispatch(SnackbarAction.SnackbarDismissed)

            navController.navigateSafe(
                R.id.browserFragment,
                BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment(),
            )
        }
        false -> {
            onShowTranslationsDialog()
        }
    }

    @VisibleForTesting
    internal fun recordTranslationStartTelemetry() {
        Translations.action.record(Translations.ActionExtra("main_flow_toolbar"))
    }
}

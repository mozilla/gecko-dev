/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.mockkStatic
import mozilla.components.browser.state.action.TranslationsAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.TranslationsBrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.translate.DetectedLanguages
import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.TranslationEngineState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationOperation
import mozilla.components.concept.engine.translate.TranslationPair
import mozilla.components.concept.engine.translate.TranslationSupport
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.atLeast
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.store.BrowserScreenAction.PageTranslationStatusUpdated
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.SnackbarAction
import org.mozilla.fenix.ext.nav

@RunWith(AndroidJUnit4::class)
class TranslationsBindingTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()

    lateinit var browserStore: BrowserStore
    val browserScreenStore: BrowserScreenStore = mock()
    val appStore: AppStore = mock()

    private val tabId = "1"
    private val tab = createTab(url = tabId, id = tabId)
    private val onTranslationsActionUpdated: (PageTranslationStatus) -> Unit = spy()

    private val onShowTranslationsDialog: () -> Unit = spy()

    @Test
    fun `GIVEN translationState WHEN translation status isTranslated THEN inform about translation changes`() {
        val englishLanguage = Language("en", "English")
        val spanishLanguage = Language("es", "Spanish")
        val expectedTranslationStatus = PageTranslationStatus(
            isTranslationPossible = true,
            isTranslated = true,
            isTranslateProcessing = true,
            fromSelectedLanguage = englishLanguage,
            toSelectedLanguage = spanishLanguage,
        )

        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tabId,
                translationEngine = TranslationsBrowserState(isEngineSupported = true),
            ),
        )

        val binding = TranslationsBinding(
            browserStore = browserStore,
            browserScreenStore = browserScreenStore,
            appStore = appStore,
            onTranslationStatusUpdate = onTranslationsActionUpdated,
            onShowTranslationsDialog = {},
        )
        binding.start()

        val detectedLanguages = DetectedLanguages(
            documentLangTag = englishLanguage.code,
            supportedDocumentLang = true,
            userPreferredLangTag = spanishLanguage.code,
        )

        val translationEngineState = TranslationEngineState(
            detectedLanguages = detectedLanguages,
            error = null,
            isEngineReady = true,
            hasVisibleChange = true,
            requestedTranslationPair = TranslationPair(
                fromLanguage = englishLanguage.code,
                toLanguage = spanishLanguage.code,
            ),
        )

        val supportLanguages = TranslationSupport(
            fromLanguages = listOf(englishLanguage),
            toLanguages = listOf(spanishLanguage),
        )

        browserStore.dispatch(
            TranslationsAction.SetSupportedLanguagesAction(
                supportedLanguages = supportLanguages,
            ),
        ).joinBlocking()

        browserStore.dispatch(
            TranslationsAction.TranslateStateChangeAction(
                tabId = tabId,
                translationEngineState = translationEngineState,
            ),
        ).joinBlocking()

        browserStore.dispatch(
            TranslationsAction.TranslateAction(
                tabId = tab.id,
                fromLanguage = englishLanguage.code,
                toLanguage = spanishLanguage.code,
                options = null,
            ),
        ).joinBlocking()

        verify(onTranslationsActionUpdated).invoke(expectedTranslationStatus)
        verify(browserScreenStore).dispatch(
            PageTranslationStatusUpdated(expectedTranslationStatus),
        )
    }

    @Test
    fun `GIVEN translationState WHEN translation status isExpectedTranslate THEN inform about translation changes`() {
        val expectedTranslationStatus = PageTranslationStatus(
            isTranslationPossible = true,
            isTranslated = false,
            isTranslateProcessing = false,
        )
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tabId,
                translationEngine = TranslationsBrowserState(isEngineSupported = true),
            ),
        )

        val binding = TranslationsBinding(
            browserStore = browserStore,
            browserScreenStore = browserScreenStore,
            appStore = appStore,
            onTranslationStatusUpdate = onTranslationsActionUpdated,
            onShowTranslationsDialog = {},
        )
        binding.start()

        browserStore.dispatch(
            TranslationsAction.TranslateExpectedAction(
                tabId = tabId,
            ),
        ).joinBlocking()

        verify(onTranslationsActionUpdated).invoke(expectedTranslationStatus)
        verify(browserScreenStore).dispatch(
            PageTranslationStatusUpdated(expectedTranslationStatus),
        )
        verify(appStore, atLeast(1)).dispatch(SnackbarAction.SnackbarDismissed)
    }

    @Test
    fun `GIVEN translationState WHEN translation status is not isExpectedTranslate or isTranslated THEN inform about translation changes`() {
        val expectedTranslationStatus = PageTranslationStatus(
            isTranslationPossible = false,
            isTranslated = false,
            isTranslateProcessing = false,
        )
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tabId,
            ),
        )

        val binding = TranslationsBinding(
            browserStore = browserStore,
            browserScreenStore = browserScreenStore,
            appStore = appStore,
            onTranslationStatusUpdate = onTranslationsActionUpdated,
            onShowTranslationsDialog = {},
        )
        binding.start()

        verify(onTranslationsActionUpdated).invoke(expectedTranslationStatus)
        verify(browserScreenStore).dispatch(
            PageTranslationStatusUpdated(expectedTranslationStatus),
        )
        verify(appStore).dispatch(SnackbarAction.SnackbarDismissed)
    }

    @Test
    fun `GIVEN translationState WHEN translation state isOfferTranslate is true THEN offer to translate the current page`() {
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tabId,
                translationEngine = TranslationsBrowserState(isEngineSupported = true),
            ),
        )

        val binding = TranslationsBinding(
            browserStore = browserStore,
            onTranslationStatusUpdate = onTranslationsActionUpdated,
            onShowTranslationsDialog = onShowTranslationsDialog,
        )
        binding.start()

        browserStore.dispatch(
            TranslationsAction.TranslateOfferAction(
                tabId = tab.id,
                isOfferTranslate = true,
            ),
        ).joinBlocking()

        verify(onShowTranslationsDialog).invoke()
    }

    @Test
    fun `GIVEN store dependencies set WHEN translation state isOfferTranslate is true THEN offer to translate the current page`() {
        val currentDestination: NavDestination = mock {
            doReturn(R.id.browserFragment).`when`(this).id
        }
        val navController: NavController = mock {
            doReturn(currentDestination).`when`(this).currentDestination
        }
        val expectedNavigation = BrowserFragmentDirections.actionBrowserFragmentToTranslationsDialogFragment()
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tabId,
                translationEngine = TranslationsBrowserState(isEngineSupported = true),
            ),
        )

        val binding = spy(
            TranslationsBinding(
                browserStore = browserStore,
                browserScreenStore = browserScreenStore,
                appStore = appStore,
                navController = navController,
                onTranslationStatusUpdate = onTranslationsActionUpdated,
                onShowTranslationsDialog = onShowTranslationsDialog,
            ),
        )
        binding.start()

        mockkStatic(NavController::nav) {
            browserStore.dispatch(
                TranslationsAction.TranslateOfferAction(
                    tabId = tab.id,
                    isOfferTranslate = true,
                ),
            ).joinBlocking()

            verify(onShowTranslationsDialog, never()).invoke()
            verify(binding).recordTranslationStartTelemetry()
            verify(appStore, atLeast(1)).dispatch(SnackbarAction.SnackbarDismissed)
            verify(navController).navigate(expectedNavigation)
        }
    }

    @Test
    fun `GIVEN translationState WHEN readerState is active THEN inform about translation changes`() {
        val expectedTranslationStatus = PageTranslationStatus(
            isTranslationPossible = false,
            isTranslated = false,
            isTranslateProcessing = false,
        )
        val tabReaderStateActive = createTab(
            "https://www.firefox.com",
            id = "test-tab",
            readerState = ReaderState(active = true),
        )
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tabReaderStateActive),
                selectedTabId = tabReaderStateActive.id,
            ),
        )

        val binding = TranslationsBinding(
            browserStore = browserStore,
            browserScreenStore = browserScreenStore,
            appStore = appStore,
            onTranslationStatusUpdate = onTranslationsActionUpdated,
            onShowTranslationsDialog = onShowTranslationsDialog,
        )
        binding.start()

        verify(onTranslationsActionUpdated).invoke(expectedTranslationStatus)
    }

    @Test
    fun `GIVEN translationState WHEN translation state isOfferTranslate is false THEN don't offer to translate the current page`() {
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tabId,
                translationEngine = TranslationsBrowserState(isEngineSupported = true),
            ),
        )

        val binding = spy(
            TranslationsBinding(
                browserStore = browserStore,
                onTranslationStatusUpdate = onTranslationsActionUpdated,
                onShowTranslationsDialog = onShowTranslationsDialog,
            ),
        )
        binding.start()

        browserStore.dispatch(
            TranslationsAction.TranslateOfferAction(
                tabId = tab.id,
                isOfferTranslate = false,
            ),
        ).joinBlocking()

        verify(onShowTranslationsDialog, never()).invoke()
        verify(binding, never()).recordTranslationStartTelemetry()
        verify(appStore, never()).dispatch(SnackbarAction.SnackbarDismissed)
    }

    @Test
    fun `GIVEN translationState WHEN translation state has an error THEN don't offer to translate the current page`() {
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tabId,
                translationEngine = TranslationsBrowserState(
                    isEngineSupported = true,
                ),
            ),
        )

        val binding = spy(
            TranslationsBinding(
                browserStore = browserStore,
                onTranslationStatusUpdate = onTranslationsActionUpdated,
                onShowTranslationsDialog = onShowTranslationsDialog,
            ),
        )
        binding.start()

        browserStore.dispatch(
            TranslationsAction.TranslateExpectedAction(
                tabId = tabId,
            ),
        ).joinBlocking()

        browserStore.dispatch(
            TranslationsAction.TranslateOfferAction(
                tabId = tab.id,
                isOfferTranslate = false,
            ),
        ).joinBlocking()

        browserStore.dispatch(
            TranslationsAction.TranslateExceptionAction(
                tabId,
                TranslationOperation.TRANSLATE,
                TranslationError.CouldNotTranslateError(null),
            ),
        ).joinBlocking()

        verify(onShowTranslationsDialog).invoke()
        verify(binding, never()).recordTranslationStartTelemetry()
        verify(onShowTranslationsDialog).invoke()
        verify(appStore, never()).dispatch(SnackbarAction.SnackbarDismissed)
    }
}

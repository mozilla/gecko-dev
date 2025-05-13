/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.content.Context
import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import androidx.fragment.app.FragmentManager
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.EXTERNAL
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE
import mozilla.components.feature.app.links.AppLinksUseCases.Companion.ENGINE_SUPPORTED_SCHEMES
import mozilla.components.feature.app.links.RedirectDialogFragment.Companion.FRAGMENT_TAG
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.appName

// Minimum time for dialog to settle before accepting user interactions.
internal const val MAX_SUCCESSIVE_DIALOG_MILLIS_LIMIT: Int = 500 // 0.5 seconds

/**
 * This feature implements observer for handling redirects to external apps. The users are asked to
 * confirm their intention before leaving the app if in private session.  These include the Android
 * Intents, custom schemes and support for [Intent.CATEGORY_BROWSABLE] `http(s)` URLs.
 *
 * It requires: a [Context], and a [FragmentManager].
 *
 * @param context Context the feature is associated with.
 * @param store Reference to the application's [BrowserStore].
 * @param sessionId The session ID to observe.
 * @param fragmentManager FragmentManager for interacting with fragments.
 * @param dialog The dialog for redirect.
 * @param launchInApp If {true} then launch app links in third party app(s). Default to false because
 * of security concerns.
 * @param useCases These use cases allow for the detection of, and opening of links that other apps
 * have registered to open.
 * @param failedToLaunchAction Action to perform when failing to launch in third party app.
 * @param loadUrlUseCase Used to load URL if user decides not to launch in third party app.
 * @param engineSupportedSchemes Set of URI schemes the engine supports.
 * @param shouldPrompt If {true} then user should be prompted before launching app links.
 * @param alwaysOpenCheckboxAction Action to perform when user checked the always open checkbox in the prompt.
 **/
class AppLinksFeature(
    private val context: Context,
    private val store: BrowserStore,
    private val sessionId: String? = null,
    private val fragmentManager: FragmentManager? = null,
    private val dialog: RedirectDialogFragment? = null,
    private val launchInApp: () -> Boolean = { false },
    private val useCases: AppLinksUseCases = AppLinksUseCases(context, launchInApp),
    private val failedToLaunchAction: (fallbackUrl: String?) -> Unit = {},
    private val loadUrlUseCase: SessionUseCases.DefaultLoadUrlUseCase? = null,
    private val engineSupportedSchemes: Set<String> = ENGINE_SUPPORTED_SCHEMES,
    private val shouldPrompt: () -> Boolean = { true },
    private val alwaysOpenCheckboxAction: (() -> Unit)? = null,
) : LifecycleAwareFeature {

    private var scope: CoroutineScope? = null

    /**
     * Starts observing app links on the selected session.
     */
    override fun start() {
        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findTabOrCustomTabOrSelectedTab(sessionId) }
                .distinctUntilChangedBy {
                    it.content.appIntent
                }
                .collect { sessionState ->
                    sessionState.content.appIntent?.let {
                        handleAppIntent(
                            sessionState = sessionState,
                            url = it.url,
                            appIntent = it.appIntent,
                            fallbackUrl = it.fallbackUrl,
                            appName = it.appName,
                        )
                        store.dispatch(ContentAction.ConsumeAppIntentAction(sessionState.id))
                    }
                }
        }

        findPreviousDialogFragment()?.let {
            fragmentManager?.beginTransaction()?.remove(it)?.commit()
        }
    }

    override fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun handleAppIntent(
        sessionState: SessionState,
        url: String,
        appIntent: Intent?,
        fallbackUrl: String?,
        appName: String?,
    ) {
        if (appIntent == null) return

        val isPrivate = sessionState.content.private
        val isAuthenticationFlow =
            AppLinksInterceptor.isAuthentication(sessionState, appIntent.component?.packageName)

        if (shouldBypassPrompt(isPrivate, isAuthenticationFlow, fragmentManager)) {
            openApp(appIntent)
            return
        }

        if (isADialogAlreadyCreated() || fragmentManager?.isStateSaved == true) {
            return
        }

        showRedirectDialog(
            sessionState = sessionState,
            url = url,
            fallbackUrl = fallbackUrl,
            appIntent = appIntent,
            appName = appName,
            isPrivate = isPrivate,
            fragmentManager = fragmentManager,
        )
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun shouldBypassPrompt(
        isPrivate: Boolean,
        isAuthenticationFlow: Boolean,
        fragmentManager: FragmentManager?,
    ): Boolean {
        val shouldShowPrompt = isPrivate || shouldPrompt()
        return fragmentManager == null || !shouldShowPrompt || isAuthenticationFlow
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun openApp(appIntent: Intent) {
        useCases.openAppLink(
            appIntent,
            failedToLaunchAction = failedToLaunchAction,
        )
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun cancelRedirect(
        sessionState: SessionState,
        url: String,
        fallbackUrl: String?,
        appIntent: Intent,
    ) {
        AppLinksInterceptor.addUserDoNotIntercept(url, appIntent, sessionState.id)

        val urlToLoad = when {
            isSchemeSupported(url) -> url
            fallbackUrl != null && isSchemeSupported(fallbackUrl) -> fallbackUrl
            else -> return // No supported URL to load.
        }

        loadUrlUseCase?.invoke(
            url = urlToLoad,
            sessionId = sessionState.id,
            flags = EngineSession.LoadUrlFlags.select(EXTERNAL, LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE),
        )
    }

    private fun showRedirectDialog(
        sessionState: SessionState,
        url: String,
        fallbackUrl: String?,
        appIntent: Intent,
        appName: String?,
        isPrivate: Boolean,
        fragmentManager: FragmentManager?,
    ) {
        if (fragmentManager == null) {
            return
        }

        getOrCreateDialog(isPrivate, url, appName).apply {
            onConfirmRedirect = { isCheckboxTicked ->
                if (isCheckboxTicked) {
                    alwaysOpenCheckboxAction?.invoke()
                }
                openApp(appIntent)
            }
            onCancelRedirect = {
                cancelRedirect(sessionState, url, fallbackUrl, appIntent)
            }
        }.showNow(fragmentManager, FRAGMENT_TAG)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getOrCreateDialog(
        isPrivate: Boolean,
        url: String,
        targetAppName: String?,
    ): RedirectDialogFragment {
        if (dialog != null) {
            return dialog
        }

        val dialogTitle = when {
            isPrivate && !targetAppName.isNullOrBlank() -> {
                context.getString(R.string.mozac_feature_applinks_confirm_dialog_title_with_app_name, targetAppName)
            }
            isPrivate -> {
                context.getString(R.string.mozac_feature_applinks_confirm_dialog_title)
            }
            !targetAppName.isNullOrBlank() -> {
                context.getString(
                    R.string.mozac_feature_applinks_normal_confirm_dialog_title_with_app_name,
                    targetAppName,
                )
            }
            else -> {
                context.getString(R.string.mozac_feature_applinks_normal_confirm_dialog_title)
            }
        }

        val dialogMessage = if (isPrivate) {
            url
        } else {
            context.getString(
                R.string.mozac_feature_applinks_normal_confirm_dialog_message,
                context.appName,
            )
        }

        return SimpleRedirectDialogFragment.newInstance(
            dialogTitleString = dialogTitle,
            dialogMessageString = dialogMessage,
            showCheckbox = if (isPrivate) false else alwaysOpenCheckboxAction != null,
            maxSuccessiveDialogMillisLimit = MAX_SUCCESSIVE_DIALOG_MILLIS_LIMIT,
        )
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun isSchemeSupported(url: String): Boolean {
        return engineSupportedSchemes.contains(url.toUri().scheme)
    }

    private fun isADialogAlreadyCreated(): Boolean {
        return findPreviousDialogFragment() != null
    }

    private fun findPreviousDialogFragment(): RedirectDialogFragment? {
        return fragmentManager?.findFragmentByTag(FRAGMENT_TAG) as? RedirectDialogFragment
    }
}

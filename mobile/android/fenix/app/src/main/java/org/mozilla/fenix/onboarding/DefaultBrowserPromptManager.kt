/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.support.utils.BrowsersCache
import org.mozilla.fenix.ext.isDefaultBrowserPromptSupported
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.view.OnboardingPageUiData

/**
 * An interface to calculate and persist the state of the default browser prompt.
 */
interface DefaultBrowserPromptStorage {
    /**
     * Indicates if the browser is already set as the default.
     */
    val isDefaultBrowser: Boolean

    /**
     * Indicates if the device supports default browser prompt functionality.
     */
    val isDefaultBrowserPromptSupported: Boolean

    /**
     * Indicates whether the prompt to set the default browser has been shown during onboarding.
     */
    var promptToSetAsDefaultBrowserDisplayedInOnboarding: Boolean
}

/**
 * Default implementation of [DefaultBrowserPromptStorage].
 *
 * @property context is used for calculating and persisting the state.
 */
class DefaultDefaultBrowserPromptStorage(
    val context: Context,
) : DefaultBrowserPromptStorage {
    override val isDefaultBrowser = BrowsersCache.all(context.applicationContext).isDefaultBrowser

    override val isDefaultBrowserPromptSupported = context.isDefaultBrowserPromptSupported()

    override var promptToSetAsDefaultBrowserDisplayedInOnboarding: Boolean
        get() = context.settings().promptToSetAsDefaultBrowserDisplayedInOnboarding
        set(value) { context.settings().promptToSetAsDefaultBrowserDisplayedInOnboarding = value }
}

/**
 * Handles the logic of prompting users to set Firefox as the default browser during onboarding.
 *
 * @param storage A [DefaultBrowserPromptStorage] implementation to persist and retrieve the prompt state.
 * @param promptToSetAsDefaultBrowser A callback to trigger the default browser prompt.
 */
class DefaultBrowserPromptManager(
    private val storage: DefaultBrowserPromptStorage,
    private val promptToSetAsDefaultBrowser: () -> Unit,
) {

    @VisibleForTesting
    internal fun canShowPrompt() = !storage.isDefaultBrowser &&
        storage.isDefaultBrowserPromptSupported &&
        !storage.promptToSetAsDefaultBrowserDisplayedInOnboarding

    /**
     * Determines whether to show the default browser prompt during onboarding.
     *
     * @param pagesToDisplay The list of onboarding pages that are being displayed to the user.
     * @param currentCard The currently displayed onboarding page.
     */
    fun maybePromptToSetAsDefaultBrowser(
        pagesToDisplay: List<OnboardingPageUiData>,
        currentCard: OnboardingPageUiData,
    ) {
        val shouldWaitForTosToBeAccepted = pagesToDisplay.find {
            it.type == OnboardingPageUiData.Type.TERMS_OF_SERVICE
        }?.let { tosCard ->
            val tosPosition = pagesToDisplay.indexOfFirst { it.type == tosCard.type }
            val currentPosition = pagesToDisplay.indexOfFirst { it.type == currentCard.type }
            // waiting until we move past a ToS card
            tosPosition >= currentPosition
        } ?: false

        if (canShowPrompt() && !shouldWaitForTosToBeAccepted) {
            promptToSetAsDefaultBrowser()
            storage.promptToSetAsDefaultBrowserDisplayedInOnboarding = true
        }
    }
}

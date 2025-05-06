/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.RestoreCompleteAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

/**
 * [Middleware] implementation for adding a homepage tab during application startup to ensure that
 * a tab is always available.
 *
 * @param applicationContext The application [Context].
 * @param repository [HomepageAsANewTabPreferencesRepository] used to access the homepage as a
 * new tab preferences.
 */
class StartupMiddleware(
    private val applicationContext: Context,
    private val repository: HomepageAsANewTabPreferencesRepository,
) : Middleware<BrowserState, BrowserAction> {
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        if (action is RestoreCompleteAction &&
            context.state.tabs.isEmpty() &&
            repository.getHomepageAsANewTabEnabled()
        ) {
            // After previous sessions are restored, add a new homepage tab if
            // there are no tabs on startup.
            val useCases = applicationContext.components.useCases.fenixBrowserUseCases
            useCases.addNewHomepageTab(private = false)
        }

        next(action)
    }
}

/**
 * The repository for managing the homepage as a new tab preference.
 */
interface HomepageAsANewTabPreferencesRepository {

    /**
     * Ret the state of a specific preference.
     */
    fun getHomepageAsANewTabEnabled(): Boolean
}

/**
 * The default implementation of [HomepageAsANewTabPreferencesRepository].
 *
 * @param settings [Settings] used to check the application shared preferences.
 */
class DefaultHomepageAsANewTabPreferenceRepository(
    private val settings: Settings,
) : HomepageAsANewTabPreferencesRepository {

    override fun getHomepageAsANewTabEnabled(): Boolean = settings.enableHomepageAsNewTab
}

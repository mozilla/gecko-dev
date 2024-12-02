/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.middleware

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.ext.getUrl
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.pwa.WebAppUseCases
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.service.fxa.manager.AccountState.Authenticated
import mozilla.components.service.fxa.manager.AccountState.Authenticating
import mozilla.components.service.fxa.manager.AccountState.AuthenticationProblem
import mozilla.components.service.fxa.manager.AccountState.NotAuthenticated
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.collections.SaveCollectionStep
import org.mozilla.fenix.components.menu.BrowserNavigationParams
import org.mozilla.fenix.components.menu.MenuDialogFragmentDirections
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.components.menu.toFenixFxAEntryPoint
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.AMO_HOMEPAGE_FOR_ANDROID
import org.mozilla.fenix.settings.SupportUtils.SumoTopic
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.webcompat.WEB_COMPAT_REPORTER_URL

/**
 * [Middleware] implementation for handling navigating events based on [MenuAction]s that are
 * dispatched to the [MenuStore].
 *
 * @param navController [NavController] used for navigation.
 * @param browsingModeManager [BrowsingModeManager] used for setting the browsing mode.
 * @param openToBrowser Callback to open the provided [BrowserNavigationParams]
 * in a new browser tab.
 * @param webAppUseCases [WebAppUseCases] used for adding items to the home screen.
 * @param settings Used to check [Settings] when adding items to the home screen.
 * @param onDismiss Callback invoked to dismiss the menu dialog.
 * @param scope [CoroutineScope] used to launch coroutines.
 * @param customTab [CustomTabSessionState] used for sharing custom tab.
 */
@Suppress("LongParameterList")
class MenuNavigationMiddleware(
    private val navController: NavController,
    private val browsingModeManager: BrowsingModeManager,
    private val openToBrowser: (params: BrowserNavigationParams) -> Unit,
    private val webAppUseCases: WebAppUseCases,
    private val settings: Settings,
    private val onDismiss: suspend () -> Unit,
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Main),
    private val customTab: CustomTabSessionState?,
) : Middleware<MenuState, MenuAction> {

    @Suppress("CyclomaticComplexMethod", "LongMethod")
    override fun invoke(
        context: MiddlewareContext<MenuState, MenuAction>,
        next: (MenuAction) -> Unit,
        action: MenuAction,
    ) {
        // Get the current state before further processing of the chain of actions.
        // This is to ensure that any navigation action will be using correct
        // state properties before they are modified due to other actions being
        // dispatched and processes.
        val currentState = context.state

        next(action)

        scope.launch(Dispatchers.Main) {
            when (action) {
                is MenuAction.Navigate.MozillaAccount -> {
                    when (action.accountState) {
                        Authenticated -> navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionGlobalAccountSettingsFragment(),
                        )

                        AuthenticationProblem -> navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionGlobalAccountProblemFragment(
                                entrypoint = action.accesspoint.toFenixFxAEntryPoint(),
                            ),
                        )

                        is Authenticating, NotAuthenticated -> navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionGlobalTurnOnSync(
                                entrypoint = action.accesspoint.toFenixFxAEntryPoint(),
                            ),
                        )
                    }
                }

                is MenuAction.Navigate.Help -> openToBrowser(
                    BrowserNavigationParams(sumoTopic = SumoTopic.HELP),
                )

                is MenuAction.Navigate.Settings -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalSettingsFragment(),
                )

                is MenuAction.Navigate.Bookmarks -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
                )

                is MenuAction.Navigate.History -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalHistoryFragment(),
                )

                is MenuAction.Navigate.Downloads -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalDownloadsFragment(),
                )

                is MenuAction.Navigate.Passwords -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionMenuDialogFragmentToLoginsListFragment(),
                )

                is MenuAction.Navigate.CustomizeHomepage -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalHomeSettingsFragment(),
                )

                is MenuAction.Navigate.ReleaseNotes -> openToBrowser(
                    BrowserNavigationParams(url = SupportUtils.WHATS_NEW_URL),
                )

                is MenuAction.Navigate.EditBookmark -> {
                    currentState.browserMenuState?.bookmarkState?.guid?.let { guidToEdit ->
                        navController.nav(
                            R.id.menuDialogFragment,
                            BrowserFragmentDirections.actionGlobalBookmarkEditFragment(
                                guidToEdit = guidToEdit,
                                requiresSnackbarPaddingForToolbar = true,
                            ),
                        )
                    }
                }

                is MenuAction.Navigate.AddToHomeScreen -> {
                    settings.installPwaOpened = true
                    if (webAppUseCases.isInstallable()) {
                        webAppUseCases.addToHomescreen()
                        onDismiss()
                    } else {
                        navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionMenuDialogFragmentToCreateShortcutFragment(),
                            navOptions = NavOptions.Builder()
                                .setPopUpTo(R.id.browserFragment, false)
                                .build(),
                        )
                    }
                }

                is MenuAction.Navigate.SaveToCollection -> {
                    currentState.browserMenuState?.selectedTab?.let { currentSession ->
                        navController.nav(
                            R.id.menuDialogFragment,
                            MenuDialogFragmentDirections.actionGlobalCollectionCreationFragment(
                                tabIds = arrayOf(currentSession.id),
                                selectedTabIds = arrayOf(currentSession.id),
                                saveCollectionStep = if (action.hasCollection) {
                                    SaveCollectionStep.SelectCollection
                                } else {
                                    SaveCollectionStep.NameCollection
                                },
                            ),
                            navOptions = NavOptions.Builder()
                                .setPopUpTo(R.id.browserFragment, false)
                                .build(),
                        )
                    }
                }

                is MenuAction.Navigate.Translate -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionMenuDialogFragmentToTranslationsDialogFragment(),
                    navOptions = NavOptions.Builder()
                        .setPopUpTo(R.id.browserFragment, false)
                        .build(),
                )

                is MenuAction.Navigate.Share -> {
                    val session = customTab ?: currentState.browserMenuState?.selectedTab
                    val url = customTab?.content?.url ?: currentState.browserMenuState?.selectedTab?.getUrl()

                    session?.let {
                        val shareData = ShareData(title = it.content.title, url = url)
                        val direction = MenuDialogFragmentDirections.actionGlobalShareFragment(
                            sessionId = it.id,
                            data = arrayOf(shareData),
                            showPage = true,
                        )

                        val popUpToId = if (customTab != null) {
                            R.id.externalAppBrowserFragment
                        } else {
                            R.id.browserFragment
                        }

                        navController.nav(
                            R.id.menuDialogFragment,
                            direction,
                            navOptions = NavOptions.Builder()
                                .setPopUpTo(popUpToId, false)
                                .build(),
                        )
                    }
                }

                is MenuAction.Navigate.ManageExtensions -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionGlobalAddonsManagementFragment(),
                )

                is MenuAction.Navigate.DiscoverMoreExtensions -> openToBrowser(
                    BrowserNavigationParams(url = AMO_HOMEPAGE_FOR_ANDROID),
                )

                is MenuAction.Navigate.ExtensionsLearnMore -> openToBrowser(
                    BrowserNavigationParams(sumoTopic = SumoTopic.FIND_INSTALL_ADDONS),
                )

                is MenuAction.Navigate.NewTab -> openNewTab(isPrivate = false)

                is MenuAction.Navigate.NewPrivateTab -> openNewTab(isPrivate = true)

                is MenuAction.Navigate.AddonDetails -> navController.nav(
                    R.id.menuDialogFragment,
                    MenuDialogFragmentDirections.actionMenuDialogFragmenToAddonDetailsFragment(
                        addon = action.addon,
                    ),
                )

                is MenuAction.Navigate.WebCompatReporter -> {
                    val session = customTab ?: currentState.browserMenuState?.selectedTab
                    session?.content?.url?.let { tabUrl ->
                        if (FeatureFlags.webCompatReporter) {
                            navController.nav(
                                id = R.id.menuDialogFragment,
                                directions = MenuDialogFragmentDirections
                                    .actionMenuDialogFragmentToWebCompatReporterFragment(
                                        tabUrl = tabUrl,
                                    ),
                            )
                        } else {
                            openToBrowser(
                                BrowserNavigationParams(url = "$WEB_COMPAT_REPORTER_URL$tabUrl"),
                            )
                        }
                    }
                }

                else -> Unit
            }
        }
    }

    private fun openNewTab(isPrivate: Boolean) {
        browsingModeManager.mode = BrowsingMode.fromBoolean(isPrivate)

        navController.nav(
            R.id.menuDialogFragment,
            MenuDialogFragmentDirections.actionGlobalHome(focusOnAddressBar = true),
        )
    }
}

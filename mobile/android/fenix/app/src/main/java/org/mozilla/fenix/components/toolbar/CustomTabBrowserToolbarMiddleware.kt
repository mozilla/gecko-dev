/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.NavController
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsEndUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.UpdateProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.ProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissionsStorage
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.ktx.kotlin.getOrigin
import mozilla.components.support.ktx.kotlin.isContentUrl
import mozilla.components.support.ktx.kotlin.trimmed
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.DisplayActions.MenuClicked
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.DisplayActions.ShareClicked
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.StartBrowserActions.CloseClicked
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.StartPageActions.SiteInfoClicked
import org.mozilla.fenix.customtabs.ExternalAppBrowserFragmentDirections
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.getCookieBannerUIMode
import org.mozilla.fenix.utils.Settings
import mozilla.components.lib.state.Action as MVIAction

/**
 * [Middleware] responsible for configuring and handling interactions with the composable toolbar
 * when shown in a custom tab.
 *
 * This is also a [ViewModel] allowing to be easily persisted between activity restarts.
 *
 * @param customTabId [String] of the custom tab in which the toolbar is shown.
 * @param browserStore [BrowserStore] to sync from.
 * @param permissionsStorage [SitePermissionsStorage] to sync from.
 * @param cookieBannersStorage [CookieBannersStorage] to sync from.
 * @param useCases [CustomTabsUseCases] used for cleanup when closing the custom tab.
 * @param trackingProtectionUseCases [TrackingProtectionUseCases] allowing to query
 * tracking protection data of the current tab.
 * @param publicSuffixList [PublicSuffixList] used to obtain the base domain of the current site.
 * @param settings [Settings] for accessing user preferences.
 */
@Suppress("LongParameterList")
class CustomTabBrowserToolbarMiddleware(
    private val customTabId: String,
    private val browserStore: BrowserStore,
    private val permissionsStorage: SitePermissionsStorage,
    private val cookieBannersStorage: CookieBannersStorage,
    private val useCases: CustomTabsUseCases,
    private val trackingProtectionUseCases: TrackingProtectionUseCases,
    private val publicSuffixList: PublicSuffixList,
    private val settings: Settings,
) : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies
    private var store: BrowserToolbarStore? = null
    private val customTab
        get() = browserStore.state.findCustomTab(customTabId)
    private var wasTitleShown = false

    /**
     * Updates the [LifecycleDependencies] of this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
        observePageLoadUpdates()
        observePageOriginUpdates()
        observePageSecurityUpdates()
    }

    @Suppress("LongMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        when (action) {
            is BrowserToolbarAction.Init -> {
                store = context.store as BrowserToolbarStore
                val customTab = customTab

                updateStartBrowserActions(customTab)
                updateStartPageActions(customTab)
                updateCurrentPageOrigin(customTab)
                updateEndBrowserActions(customTab)
            }

            is CloseClicked -> {
                useCases.remove(customTabId)
                dependencies.closeTabDelegate()
            }

            is SiteInfoClicked -> {
                val customTab = requireNotNull(customTab)
                dependencies.lifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
                    val sitePermissions: SitePermissions? = customTab.content.url.getOrigin()?.let { origin ->
                        permissionsStorage.findSitePermissionsBy(origin, private = customTab.content.private)
                    }

                    dependencies.lifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
                        trackingProtectionUseCases.containsException(customTabId) { isExcepted ->
                            dependencies.lifecycleOwner.lifecycleScope.launch {
                                val cookieBannerUIMode = cookieBannersStorage.getCookieBannerUIMode(
                                    tab = customTab,
                                    isFeatureEnabledInPrivateMode = settings.shouldUseCookieBannerPrivateMode,
                                    publicSuffixList = publicSuffixList,
                                )

                                val directions = ExternalAppBrowserFragmentDirections
                                    .actionGlobalQuickSettingsSheetDialogFragment(
                                        sessionId = customTabId,
                                        url = customTab.content.url,
                                        title = customTab.content.title,
                                        isLocalPdf = customTab.content.url.isContentUrl(),
                                        isSecured = customTab.content.securityInfo.secure,
                                        sitePermissions = sitePermissions,
                                        gravity = settings.toolbarPosition.androidGravity,
                                        certificateName = customTab.content.securityInfo.issuer,
                                        permissionHighlights = customTab.content.permissionHighlights,
                                        isTrackingProtectionEnabled =
                                            customTab.trackingProtection.enabled && !isExcepted,
                                        cookieBannerUIMode = cookieBannerUIMode,
                                    )
                                dependencies.navController.nav(
                                    R.id.externalAppBrowserFragment,
                                    directions,
                                )
                            }
                        }
                    }
                }
            }

            is ShareClicked -> {
                val customTab = customTab
                dependencies.navController.navigate(
                    NavGraphDirections.actionGlobalShareFragment(
                        sessionId = customTabId,
                        data = arrayOf(
                            ShareData(
                                url = customTab?.content?.url,
                                title = customTab?.content?.title,
                            ),
                        ),
                        showPage = true,
                    ),
                )
            }

            is MenuClicked -> {
                dependencies.navController.nav(
                    R.id.externalAppBrowserFragment,
                    BrowserFragmentDirections.actionGlobalMenuDialogFragment(
                        accesspoint = MenuAccessPoint.External,
                        customTabSessionId = customTabId,
                    ),
                )
            }

            else -> next(action)
        }
    }

    private fun observePageOriginUpdates() {
        observeWhileActive(browserStore) {
            mapNotNull { state -> state.findCustomTab(customTabId) }
                .ifAnyChanged { tab -> arrayOf(tab.content.title, tab.content.url) }
                .collect {
                    updateCurrentPageOrigin(it)
                }
        }
    }

    private fun observePageLoadUpdates() {
        observeWhileActive(browserStore) {
            mapNotNull { state -> state.findCustomTab(customTabId) }
                .distinctUntilChangedBy { it.content.progress }
                .collect {
                    store?.dispatch(
                        UpdateProgressBarConfig(
                            buildProgressBar(it.content.progress),
                        ),
                    )
                }
        }
    }

    private fun observePageSecurityUpdates() {
        observeWhileActive(browserStore) {
            mapNotNull { state -> state.findCustomTab(customTabId) }
                .distinctUntilChangedBy { tab -> tab.content.securityInfo }
                .collect {
                    updateStartPageActions(it)
                }
        }
    }

    private fun updateStartBrowserActions(customTab: CustomTabSessionState?) = store?.dispatch(
        BrowserActionsStartUpdated(
            buildStartBrowserActions(customTab),
        ),
    )

    private fun updateStartPageActions(customTab: CustomTabSessionState?) = store?.dispatch(
        PageActionsStartUpdated(
            buildStartPageActions(customTab),
        ),
    )

    private fun updateCurrentPageOrigin(customTab: CustomTabSessionState?) {
        dependencies.lifecycleOwner.lifecycleScope.launch {
            store?.dispatch(
                BrowserDisplayToolbarAction.PageOriginUpdated(
                    PageOrigin(
                        hint = R.string.search_hint,
                        title = getTitleToShown(customTab),
                        url = getUrlDomain()?.trimmed(),
                        onClick = null,
                    ),
                ),
            )
        }
    }

    private fun updateEndBrowserActions(customTab: CustomTabSessionState?) = store?.dispatch(
        BrowserActionsEndUpdated(
            buildEndBrowserActions(customTab),
        ),
    )

    private fun buildStartBrowserActions(customTab: CustomTabSessionState?): List<Action> =
        when (customTab?.config?.showCloseButton) {
            true -> listOf(
                ActionButton(
                    icon = R.drawable.mozac_ic_cross_24,
                    contentDescription = R.string.mozac_feature_customtabs_exit_button,
                    onClick = CloseClicked,
                ),
            )

            else -> emptyList()
        }

    private fun buildStartPageActions(customTab: CustomTabSessionState?) = buildList {
        if (customTab?.content?.url?.isContentUrl() == true) {
            add(
                ActionButton(
                    icon = R.drawable.mozac_ic_page_portrait_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = SiteInfoClicked,
                ),
            )
        } else if (customTab?.content?.securityInfo?.secure == true) {
            add(
                ActionButton(
                    icon = R.drawable.mozac_ic_lock_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = SiteInfoClicked,
                ),
            )
        } else {
            add(
                ActionButton(
                    icon = R.drawable.mozac_ic_broken_lock,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = SiteInfoClicked,
                ),
            )
        }
    }

    private fun buildEndBrowserActions(customTab: CustomTabSessionState?) = buildList {
        if (customTab?.config?.showShareMenuItem == true) {
            add(
                ActionButton(
                    icon = R.drawable.mozac_ic_share_android_24,
                    contentDescription = R.string.mozac_feature_customtabs_share_link,
                    onClick = ShareClicked,
                ),
            )
        }

        add(
            ActionButton(
                icon = R.drawable.mozac_ic_ellipsis_vertical_24,
                contentDescription = R.string.content_description_menu,
                onClick = MenuClicked,
            ),
        )
    }

    private fun buildProgressBar(progress: Int = 0) = ProgressBarConfig(
        progress = progress,
        gravity = when (settings.shouldUseBottomToolbar) {
            true -> ProgressBarGravity.Top
            false -> ProgressBarGravity.Bottom
        },
    )

    private suspend fun getUrlDomain(): String? {
        val url = customTab?.content?.url
        return url?.toUri()?.host?.ifEmpty { null }
            ?.let { publicSuffixList.getPublicSuffixPlusOne(it) }
            ?.await()
            ?: url
    }

    private fun getTitleToShown(customTab: CustomTabSessionState?): String? {
        val title = customTab?.content?.title
        // If we showed a title once in a custom tab then we are going to continue displaying
        // a title (to avoid the layout bouncing around).
        // However if no title is available then we just use the URL.
        return when {
            wasTitleShown && title.isNullOrBlank() -> customTab?.content?.url
            !title.isNullOrBlank() -> {
                wasTitleShown = true
                title
            }
            else -> null // title was not shown previously and is currently blank
        }
    }

    private inline fun <S : State, A : MVIAction> observeWhileActive(
        store: Store<S, A>,
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ) {
        with(dependencies.lifecycleOwner) {
            lifecycleScope.launch {
                repeatOnLifecycle(RESUMED) {
                    store.flow().observe()
                }
            }
        }
    }

    /**
     * Lifecycle dependencies for the [BrowserToolbarMiddleware].
     *
     * @property lifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
     * @property navController [NavController] to use for navigating to other in-app destinations.
     * @property closeTabDelegate Callback for when the current custom tab needs to be closed.
     */
    data class LifecycleDependencies(
        val lifecycleOwner: LifecycleOwner,
        val navController: NavController,
        val closeTabDelegate: () -> Unit,
    )

    /**
     * Static functionalities of the [BrowserToolbarMiddleware].
     */
    companion object {
        /**
         * [ViewModelProvider.Factory] for creating a [BrowserToolbarMiddleware].
         *
         * @param customTabId [String] of the custom tab in which the toolbar is shown.
         * @param browserStore [BrowserStore] to sync from.
         * @param permissionsStorage [SitePermissionsStorage] to sync from.
         * @param cookieBannersStorage [CookieBannersStorage] to sync from.
         * @param useCases [CustomTabsUseCases] used for cleanup when closing the custom tab.
         * @param trackingProtectionUseCases [TrackingProtectionUseCases] allowing to query
         * tracking protection data of the current tab.
         * @param publicSuffixList [PublicSuffixList] used to obtain the base domain of the current site.
         * @param settings [Settings] for accessing user preferences.
         */
        fun viewModelFactory(
            customTabId: String,
            browserStore: BrowserStore,
            permissionsStorage: SitePermissionsStorage,
            cookieBannersStorage: CookieBannersStorage,
            useCases: CustomTabsUseCases,
            trackingProtectionUseCases: TrackingProtectionUseCases,
            publicSuffixList: PublicSuffixList,
            settings: Settings,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T {
                if (modelClass.isAssignableFrom(CustomTabBrowserToolbarMiddleware::class.java)) {
                    return CustomTabBrowserToolbarMiddleware(
                        customTabId = customTabId,
                        browserStore = browserStore,
                        permissionsStorage = permissionsStorage,
                        useCases = useCases,
                        trackingProtectionUseCases = trackingProtectionUseCases,
                        cookieBannersStorage = cookieBannersStorage,
                        publicSuffixList = publicSuffixList,
                        settings = settings,
                    ) as T
                }
                throw IllegalArgumentException("Unknown ViewModel class")
            }
        }

        @VisibleForTesting
        internal sealed class StartBrowserActions : BrowserToolbarEvent {
            data object CloseClicked : StartBrowserActions()
        }

        @VisibleForTesting
        internal sealed class StartPageActions : BrowserToolbarEvent {
            data object SiteInfoClicked : StartPageActions()
        }

        @VisibleForTesting
        internal sealed class DisplayActions : BrowserToolbarEvent {
            data object ShareClicked : DisplayActions()
            data object MenuClicked : DisplayActions()
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Intent
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.graphics.drawable.toDrawable
import androidx.core.net.toUri
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.ViewModel
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButtonRes
import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsEndUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.BrowserActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageActionsStartUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.UpdateProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction.Init
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.EnvironmentCleared
import mozilla.components.compose.browser.toolbar.store.EnvironmentRehydrated
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
import mozilla.components.support.ktx.kotlin.isIpv4OrIpv6
import mozilla.components.support.ktx.kotlin.trimmed
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.DisplayActions.MenuClicked
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.DisplayActions.ShareClicked
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.EndPageActions.CustomButtonClicked
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.StartBrowserActions.CloseClicked
import org.mozilla.fenix.components.toolbar.CustomTabBrowserToolbarMiddleware.Companion.StartPageActions.SiteInfoClicked
import org.mozilla.fenix.customtabs.ExternalAppBrowserFragmentDirections
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.settings.quicksettings.protections.cookiebanners.getCookieBannerUIMode
import org.mozilla.fenix.utils.Settings
import mozilla.components.lib.state.Action as MVIAction

private const val CUSTOM_BUTTON_CLICK_RETURN_CODE = 0

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
    @VisibleForTesting
    internal var environment: CustomTabToolbarEnvironment? = null
    private val customTab
        get() = browserStore.state.findCustomTab(customTabId)
    private var wasTitleShown = false

    @Suppress("LongMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        when (action) {
            is Init -> {
                next(action)

                val customTab = customTab
                updateStartPageActions(context.store, customTab)
                updateEndBrowserActions(context.store, customTab)
            }

            is EnvironmentRehydrated -> {
                next(action)

                environment = action.environment as? CustomTabToolbarEnvironment

                updateStartBrowserActions(context.store, customTab)
                updateCurrentPageOrigin(context.store, customTab)
                updateEndPageActions(context.store, customTab)

                observePageLoadUpdates(context.store)
                observePageOriginUpdates(context.store)
                observePageSecurityUpdates(context.store)
            }

            is EnvironmentCleared -> {
                next(action)

                environment = null
            }

            is CloseClicked -> {
                useCases.remove(customTabId)
                environment?.closeTabDelegate()
            }

            is SiteInfoClicked -> {
                val environment = environment ?: return
                val customTab = requireNotNull(customTab)
                environment.viewLifecycleOwner.lifecycleScope.launch(Dispatchers.IO) {
                    val sitePermissions: SitePermissions? = customTab.content.url.getOrigin()?.let { origin ->
                        permissionsStorage.findSitePermissionsBy(origin, private = customTab.content.private)
                    }

                    environment.viewLifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
                        trackingProtectionUseCases.containsException(customTabId) { isExcepted ->
                            environment.viewLifecycleOwner.lifecycleScope.launch {
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
                                environment.navController.nav(
                                    R.id.externalAppBrowserFragment,
                                    directions,
                                )
                            }
                        }
                    }
                }
            }

            is CustomButtonClicked -> {
                val environment = environment ?: return
                val customTab = customTab
                customTab?.config?.actionButtonConfig?.pendingIntent?.send(
                    environment.context,
                    CUSTOM_BUTTON_CLICK_RETURN_CODE,
                    Intent(null, customTab.content.url.toUri()),
                )
            }

            is ShareClicked -> {
                val customTab = customTab
                environment?.navController?.navigate(
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
                runWithinEnvironment {
                    navController.nav(
                        R.id.externalAppBrowserFragment,
                        BrowserFragmentDirections.actionGlobalMenuDialogFragment(
                            accesspoint = MenuAccessPoint.External,
                            customTabSessionId = customTabId,
                        ),
                    )
                }
            }

            else -> next(action)
        }
    }

    private fun observePageOriginUpdates(store: Store<BrowserToolbarState, BrowserToolbarAction>) {
        browserStore.observeWhileActive {
            mapNotNull { state -> state.findCustomTab(customTabId) }
                .ifAnyChanged { tab -> arrayOf(tab.content.title, tab.content.url) }
                .collect {
                    updateCurrentPageOrigin(store, it)
                }
        }
    }

    private fun observePageLoadUpdates(store: Store<BrowserToolbarState, BrowserToolbarAction>) {
        browserStore.observeWhileActive {
            mapNotNull { state -> state.findCustomTab(customTabId) }
                .distinctUntilChangedBy { it.content.progress }
                .collect {
                    store.dispatch(
                        UpdateProgressBarConfig(
                            buildProgressBar(it.content.progress),
                        ),
                    )
                }
        }
    }

    private fun observePageSecurityUpdates(store: Store<BrowserToolbarState, BrowserToolbarAction>) {
        browserStore.observeWhileActive {
            mapNotNull { state -> state.findCustomTab(customTabId) }
                .distinctUntilChangedBy { tab -> tab.content.securityInfo }
                .collect {
                    updateStartPageActions(store, it)
                }
        }
    }

    private fun updateStartBrowserActions(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        customTab: CustomTabSessionState?,
    ) = store.dispatch(
        BrowserActionsStartUpdated(
            buildStartBrowserActions(customTab),
        ),
    )

    private fun updateStartPageActions(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        customTab: CustomTabSessionState?,
    ) = store.dispatch(
        PageActionsStartUpdated(
            buildStartPageActions(customTab),
        ),
    )

    private fun updateCurrentPageOrigin(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        customTab: CustomTabSessionState?,
    ) {
        environment?.viewLifecycleOwner?.lifecycleScope?.launch {
            store.dispatch(
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

    private fun updateEndPageActions(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        customTab: CustomTabSessionState?,
    ) = store.dispatch(
        BrowserDisplayToolbarAction.PageActionsEndUpdated(
            buildEndPageActions(customTab),
        ),
    )

    private fun updateEndBrowserActions(
        store: Store<BrowserToolbarState, BrowserToolbarAction>,
        customTab: CustomTabSessionState?,
    ) = store.dispatch(
        BrowserActionsEndUpdated(
            buildEndBrowserActions(customTab),
        ),
    )

    private fun buildStartBrowserActions(customTab: CustomTabSessionState?): List<Action> {
        val environment = environment ?: return emptyList()
        val customTabConfig = customTab?.config
        val customIconBitmap = customTabConfig?.closeButtonIcon

        return when (customTabConfig?.showCloseButton) {
            true -> listOf(
                ActionButton(
                    drawable = when (customIconBitmap) {
                        null -> AppCompatResources.getDrawable(
                            environment.context, R.drawable.mozac_ic_cross_24,
                        )

                        else -> customIconBitmap.toDrawable(environment.context.resources)
                    },
                    contentDescription = environment.context.getString(R.string.mozac_feature_customtabs_exit_button),
                    onClick = CloseClicked,
                ),
            )

            else -> emptyList()
        }
    }

    private fun buildStartPageActions(customTab: CustomTabSessionState?) = buildList {
        if (customTab?.content?.url?.isContentUrl() == true) {
            add(
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_page_portrait_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = SiteInfoClicked,
                ),
            )
        } else if (customTab?.content?.securityInfo?.secure == true) {
            add(
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_shield_checkmark_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = SiteInfoClicked,
                ),
            )
        } else {
            add(
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_shield_slash_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = SiteInfoClicked,
                ),
            )
        }
    }

    private fun buildEndPageActions(customTab: CustomTabSessionState?): List<ActionButton> {
        val environment = environment ?: return emptyList()
        val customButtonConfig = customTab?.config?.actionButtonConfig
        val customButtonIcon = customButtonConfig?.icon

        return when (customButtonIcon) {
            null -> emptyList()
            else -> listOf(
                ActionButton(
                    drawable = customButtonIcon.toDrawable(environment.context.resources),
                    shouldTint = customTab.content.private || customButtonConfig.tint,
                    contentDescription = customButtonConfig.description,
                    onClick = CustomButtonClicked,
                ),
            )
        }
    }

    private fun buildEndBrowserActions(customTab: CustomTabSessionState?) = buildList {
        if (customTab?.config?.showShareMenuItem == true) {
            add(
                ActionButtonRes(
                    drawableResId = R.drawable.mozac_ic_share_android_24,
                    contentDescription = R.string.mozac_feature_customtabs_share_link,
                    onClick = ShareClicked,
                ),
            )
        }

        add(
            ActionButtonRes(
                drawableResId = R.drawable.mozac_ic_ellipsis_vertical_24,
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
        val host = url?.toUri()?.host
        return when {
            host.isNullOrEmpty() -> url
            host.isIpv4OrIpv6() -> host
            else -> publicSuffixList.getPublicSuffixPlusOne(host).await() ?: url
        }
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

    private inline fun <S : State, A : MVIAction> Store<S, A>.observeWhileActive(
        crossinline observe: suspend (Flow<S>.() -> Unit),
    ): Job? = environment?.viewLifecycleOwner?.run {
        lifecycleScope.launch {
            repeatOnLifecycle(RESUMED) {
                flow().observe()
            }
        }
    }

    private inline fun runWithinEnvironment(
        block: CustomTabToolbarEnvironment.() -> Unit,
    ) = environment?.let { block(it) }

    /**
     * Static functionalities of the [BrowserToolbarMiddleware].
     */
    companion object {
        @VisibleForTesting
        internal sealed class StartBrowserActions : BrowserToolbarEvent {
            data object CloseClicked : StartBrowserActions()
        }

        @VisibleForTesting
        internal sealed class StartPageActions : BrowserToolbarEvent {
            data object SiteInfoClicked : StartPageActions()
        }

        @VisibleForTesting
        internal sealed class EndPageActions : BrowserToolbarEvent {
            data object CustomButtonClicked : EndPageActions()
        }

        @VisibleForTesting
        internal sealed class DisplayActions : BrowserToolbarEvent {
            data object ShareClicked : DisplayActions()
            data object MenuClicked : DisplayActions()
        }
    }
}

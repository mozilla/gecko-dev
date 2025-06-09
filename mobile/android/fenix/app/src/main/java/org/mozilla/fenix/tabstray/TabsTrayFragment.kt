/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.app.Dialog
import android.content.Intent
import android.content.res.Configuration
import android.os.Build.VERSION.SDK_INT
import android.os.Build.VERSION_CODES
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.ActivityResultLauncher
import androidx.annotation.UiThread
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDialogFragment
import androidx.biometric.BiometricManager
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updateLayoutParams
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.setFragmentResultListener
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import com.google.android.material.bottomsheet.BottomSheetBehavior
import kotlinx.coroutines.Dispatchers
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.feature.accounts.push.CloseTabsUseCases
import mozilla.components.feature.downloads.ui.DownloadCancelDialogFragment
import mozilla.components.feature.tabs.tabstray.TabsFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.PrivateBrowsingLocked
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.biometricauthentication.AuthenticationStatus
import org.mozilla.fenix.biometricauthentication.BiometricAuthenticationManager
import org.mozilla.fenix.biometricauthentication.BiometricAuthenticationNeededInfo
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.compose.core.Action
import org.mozilla.fenix.compose.snackbar.Snackbar
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.databinding.ComponentTabstray3Binding
import org.mozilla.fenix.databinding.ComponentTabstray3FabBinding
import org.mozilla.fenix.databinding.FragmentTabTrayDialogBinding
import org.mozilla.fenix.ext.actualInactiveTabs
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.registerForActivityResult
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.lifecycle.registerForVerification
import org.mozilla.fenix.lifecycle.verifyUser
import org.mozilla.fenix.navigation.DefaultNavControllerProvider
import org.mozilla.fenix.navigation.NavControllerProvider
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.settings.biometric.BiometricUtils
import org.mozilla.fenix.settings.biometric.DefaultBiometricUtils
import org.mozilla.fenix.settings.biometric.ext.isAuthenticatorAvailable
import org.mozilla.fenix.settings.biometric.ext.isHardwareAvailable
import org.mozilla.fenix.share.ShareFragment
import org.mozilla.fenix.tabstray.browser.TabSorter
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsIntegration
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.allowUndo
import kotlin.math.abs
import kotlin.math.max

/**
 * The action or screen that was used to navigate to the Tabs Tray.
 */
enum class TabsTrayAccessPoint {
    None,
    HomeRecentSyncedTab,
}

@Suppress("TooManyFunctions", "LargeClass")
class TabsTrayFragment : AppCompatDialogFragment() {

    private lateinit var tabsTrayDialog: TabsTrayDialog
    private lateinit var tabsTrayInteractor: TabsTrayInteractor
    private lateinit var tabsTrayController: DefaultTabsTrayController
    private lateinit var navigationInteractor: DefaultNavigationInteractor
    private lateinit var enablePbmPinLauncher: ActivityResultLauncher<Intent>

    @VisibleForTesting
    internal var verificationResultLauncher: ActivityResultLauncher<Intent> =
        registerForVerification(onVerified = ::openPrivateTabsPage)

    @VisibleForTesting internal lateinit var tabsTrayStore: TabsTrayStore

    @VisibleForTesting internal lateinit var trayBehaviorManager: TabSheetBehaviorManager

    private val inactiveTabsBinding = ViewBoundFeatureWrapper<InactiveTabsBinding>()
    private val secureTabsTrayBinding = ViewBoundFeatureWrapper<SecureTabsTrayBinding>()
    private val tabsFeature = ViewBoundFeatureWrapper<TabsFeature>()
    private val syncedTabsIntegration = ViewBoundFeatureWrapper<SyncedTabsIntegration>()

    @VisibleForTesting
    @Suppress("VariableNaming")
    internal var _tabsTrayDialogBinding: FragmentTabTrayDialogBinding? = null
    private val tabsTrayDialogBinding get() = _tabsTrayDialogBinding!!

    @Suppress("VariableNaming")
    internal var _tabsTrayComposeBinding: ComponentTabstray3Binding? = null
    private val tabsTrayComposeBinding get() = _tabsTrayComposeBinding!!

    @Suppress("VariableNaming")
    internal var _fabButtonComposeBinding: ComponentTabstray3FabBinding? = null
    private val fabButtonComposeBinding get() = _fabButtonComposeBinding!!

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        recordBreadcrumb("TabsTrayFragment dismissTabsTray")
        setStyle(STYLE_NO_TITLE, R.style.TabTrayDialogStyle)
    }

    @Suppress("LongMethod")
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val args by navArgs<TabsTrayFragmentArgs>()
        args.accessPoint.takeIf { it != TabsTrayAccessPoint.None }?.let {
            TabsTray.accessPoint[it.name.lowercase()].add()
        }
        val initialMode = if (args.enterMultiselect) {
            TabsTrayState.Mode.Select(emptySet())
        } else {
            TabsTrayState.Mode.Normal
        }
        val initialPage = args.page
        val activity = activity as HomeActivity
        val initialInactiveExpanded = requireComponents.appStore.state.inactiveTabsExpanded
        val inactiveTabs = requireComponents.core.store.state.actualInactiveTabs(requireContext().settings())
        val normalTabs = requireComponents.core.store.state.normalTabs - inactiveTabs.toSet()

        enablePbmPinLauncher = registerForActivityResult(
            onSuccess = {
                PrivateBrowsingLocked.authSuccess.record()
                PrivateBrowsingLocked.featureEnabled.record()
                requireContext().settings().privateBrowsingLockedEnabled = true
            },
            onFailure = {
                PrivateBrowsingLocked.authFailure.record()
            },
        )

        tabsTrayStore = StoreProvider.get(this) {
            TabsTrayStore(
                initialState = TabsTrayState(
                    selectedPage = initialPage,
                    mode = initialMode,
                    inactiveTabs = inactiveTabs,
                    inactiveTabsExpanded = initialInactiveExpanded,
                    normalTabs = normalTabs,
                    privateTabs = requireComponents.core.store.state.privateTabs,
                    selectedTabId = requireComponents.core.store.state.selectedTabId,
                ),
                middlewares = listOf(
                    TabsTrayTelemetryMiddleware(),
                ),
            )
        }

        navigationInteractor =
            DefaultNavigationInteractor(
                browserStore = requireComponents.core.store,
                navController = findNavController(),
                dismissTabTray = ::dismissTabsTray,
                dismissTabTrayAndNavigateHome = ::dismissTabsTrayAndNavigateHome,
                showCancelledDownloadWarning = ::showCancelledDownloadWarning,
                accountManager = requireComponents.backgroundServices.accountManager,
            )

        tabsTrayController = DefaultTabsTrayController(
            activity = activity,
            appStore = requireComponents.appStore,
            tabsTrayStore = tabsTrayStore,
            browserStore = requireComponents.core.store,
            settings = requireContext().settings(),
            browsingModeManager = activity.browsingModeManager,
            navController = findNavController(),
            navigateToHomeAndDeleteSession = ::navigateToHomeAndDeleteSession,
            navigationInteractor = navigationInteractor,
            profiler = requireComponents.core.engine.profiler,
            tabsUseCases = requireComponents.useCases.tabsUseCases,
            fenixBrowserUseCases = requireComponents.useCases.fenixBrowserUseCases,
            closeSyncedTabsUseCases = requireComponents.useCases.closeSyncedTabsUseCases,
            bookmarksStorage = requireComponents.core.bookmarksStorage,
            ioDispatcher = Dispatchers.IO,
            collectionStorage = requireComponents.core.tabCollectionStorage,
            dismissTray = ::dismissTabsTray,
            showUndoSnackbarForTab = ::showUndoSnackbarForTab,
            showUndoSnackbarForInactiveTab = ::showUndoSnackbarForInactiveTab,
            showUndoSnackbarForSyncedTab = ::showUndoSnackbarForSyncedTab,
            showCancelledDownloadWarning = ::showCancelledDownloadWarning,
            showCollectionSnackbar = ::showCollectionSnackbar,
            showBookmarkSnackbar = ::showBookmarkSnackbar,
        )

        tabsTrayInteractor = DefaultTabsTrayInteractor(
            controller = tabsTrayController,
        )

        recordBreadcrumb("TabsTrayFragment onCreateDialog")

        tabsTrayDialog = TabsTrayDialog(requireContext(), theme) { tabsTrayInteractor }
        return tabsTrayDialog
    }

    override fun onPause() {
        super.onPause()
        recordBreadcrumb("TabsTrayFragment onPause")
        dialog?.window?.setWindowAnimations(R.style.DialogFragmentRestoreAnimation)
    }

    @Suppress("LongMethod")
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _tabsTrayDialogBinding = FragmentTabTrayDialogBinding.inflate(
            inflater,
            container,
            false,
        )

        _tabsTrayComposeBinding = ComponentTabstray3Binding.inflate(
            inflater,
            tabsTrayDialogBinding.root,
            true,
        )

        _fabButtonComposeBinding = ComponentTabstray3FabBinding.inflate(
            inflater,
            tabsTrayDialogBinding.root,
            true,
        )

        tabsTrayComposeBinding.root.setContent {
            FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
                TabsTray(
                    tabsTrayStore = tabsTrayStore,
                    displayTabsInGrid = requireContext().settings().gridTabView,
                    isInDebugMode = Config.channel.isDebug ||
                        requireComponents.settings.showSecretDebugMenuThisSession,
                    shouldShowTabAutoCloseBanner = requireContext().settings().shouldShowAutoCloseTabsBanner &&
                        requireContext().settings().canShowCfr,
                    shouldShowLockPbmBanner =
                        if (FxNimbus.features.privateBrowsingLock.value().enabled) {
                            shouldShowLockPbmBanner(
                                isPrivateMode = (activity as HomeActivity).browsingModeManager.mode.isPrivate,
                                hasPrivateTabs = requireComponents.core.store.state.privateTabs.isNotEmpty(),
                                biometricAvailable = BiometricManager.from(requireContext())
                                    .isHardwareAvailable(),
                                privateLockEnabled = requireContext().settings().privateBrowsingLockedEnabled,
                                shouldShowBanner = requireContext().settings().shouldShowLockPbmBanner,
                            )
                        } else {
                            false
                        },
                    shouldShowInactiveTabsAutoCloseDialog =
                        requireContext().settings()::shouldShowInactiveTabsAutoCloseDialog,
                    onTabPageClick = { page ->
                        onTabPageClick(
                            tabsTrayInteractor = tabsTrayInteractor,
                            page = page,
                            isPrivateScreenLocked = requireComponents.appStore.state.isPrivateScreenLocked,
                        )
                    },
                    onTabClose = { tab ->
                        tabsTrayInteractor.onTabClosed(tab, TABS_TRAY_FEATURE_NAME)
                    },
                    onTabMediaClick = tabsTrayInteractor::onMediaClicked,
                    onTabClick = { tab ->
                        run outer@{
                            if (!requireContext().settings().hasShownTabSwipeCFR &&
                                !requireContext().isTabStripEnabled() &&
                                requireContext().settings().isSwipeToolbarToSwitchTabsEnabled
                            ) {
                                val normalTabs = tabsTrayStore.state.normalTabs
                                val currentTabId = tabsTrayStore.state.selectedTabId

                                if (normalTabs.size >= 2) {
                                    val currentTabPosition = currentTabId
                                        ?.let { getTabPositionFromId(normalTabs, it) }
                                        ?: return@outer
                                    val newTabPosition =
                                        getTabPositionFromId(normalTabs, tab.id)

                                    if (abs(currentTabPosition - newTabPosition) == 1) {
                                        requireContext().settings().shouldShowTabSwipeCFR = true
                                    }
                                }
                            }
                        }

                        tabsTrayInteractor.onTabSelected(tab, TABS_TRAY_FEATURE_NAME)
                    },
                    onTabLongClick = tabsTrayInteractor::onTabLongClicked,
                    onInactiveTabsHeaderClick = tabsTrayInteractor::onInactiveTabsHeaderClicked,
                    onDeleteAllInactiveTabsClick = tabsTrayInteractor::onDeleteAllInactiveTabsClicked,
                    onInactiveTabsAutoCloseDialogShown = {
                        tabsTrayStore.dispatch(TabsTrayAction.TabAutoCloseDialogShown)
                    },
                    onInactiveTabAutoCloseDialogCloseButtonClick =
                        tabsTrayInteractor::onAutoCloseDialogCloseButtonClicked,
                    onEnableInactiveTabAutoCloseClick = {
                        tabsTrayInteractor.onEnableAutoCloseClicked()
                        showInactiveTabsAutoCloseConfirmationSnackbar()
                    },
                    onInactiveTabClick = tabsTrayInteractor::onInactiveTabClicked,
                    onInactiveTabClose = tabsTrayInteractor::onInactiveTabClosed,
                    onSyncedTabClick = tabsTrayInteractor::onSyncedTabClicked,
                    onSyncedTabClose = tabsTrayInteractor::onSyncedTabClosed,
                    onSaveToCollectionClick = tabsTrayInteractor::onAddSelectedTabsToCollectionClicked,
                    onShareSelectedTabsClick = tabsTrayInteractor::onShareSelectedTabs,
                    onShareAllTabsClick = {
                        if (tabsTrayStore.state.selectedPage == Page.NormalTabs) {
                            tabsTrayStore.dispatch(TabsTrayAction.ShareAllNormalTabs)
                        } else if (tabsTrayStore.state.selectedPage == Page.PrivateTabs) {
                            tabsTrayStore.dispatch(TabsTrayAction.ShareAllPrivateTabs)
                        }

                        navigationInteractor.onShareTabsOfTypeClicked(
                            private = tabsTrayStore.state.selectedPage == Page.PrivateTabs,
                        )
                    },
                    onTabSettingsClick = navigationInteractor::onTabSettingsClicked,
                    onRecentlyClosedClick = navigationInteractor::onOpenRecentlyClosedClicked,
                    onAccountSettingsClick = navigationInteractor::onAccountSettingsClicked,
                    onDeleteAllTabsClick = {
                        if (tabsTrayStore.state.selectedPage == Page.NormalTabs) {
                            tabsTrayStore.dispatch(TabsTrayAction.CloseAllNormalTabs)
                        } else if (tabsTrayStore.state.selectedPage == Page.PrivateTabs) {
                            tabsTrayStore.dispatch(TabsTrayAction.CloseAllPrivateTabs)
                        }

                        navigationInteractor.onCloseAllTabsClicked(
                            private = tabsTrayStore.state.selectedPage == Page.PrivateTabs,
                        )
                    },
                    onDeleteSelectedTabsClick = tabsTrayInteractor::onDeleteSelectedTabsClicked,
                    onBookmarkSelectedTabsClick = tabsTrayInteractor::onBookmarkSelectedTabsClicked,
                    onForceSelectedTabsAsInactiveClick = tabsTrayInteractor::onForceSelectedTabsAsInactiveClicked,
                    onTabsTrayDismiss = ::onTabsTrayDismissed,
                    onTabsTrayPbmLockedClick = ::onTabsTrayPbmLockedClick,
                    onTabsTrayPbmLockedDismiss = {
                        requireContext().settings().shouldShowLockPbmBanner = false
                        PrivateBrowsingLocked.bannerNegativeClicked.record()
                    },
                    onTabAutoCloseBannerViewOptionsClick = {
                        navigationInteractor.onTabSettingsClicked()
                        requireContext().settings().shouldShowAutoCloseTabsBanner = false
                        requireContext().settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                    },
                    onTabAutoCloseBannerDismiss = {
                        requireContext().settings().shouldShowAutoCloseTabsBanner = false
                        requireContext().settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                    },
                    onTabAutoCloseBannerShown = {},
                    onMove = tabsTrayInteractor::onTabsMove,
                    shouldShowInactiveTabsCFR = {
                        requireContext().settings().shouldShowInactiveTabsOnboardingPopup &&
                            requireContext().settings().canShowCfr
                    },
                    onInactiveTabsCFRShown = {
                        TabsTray.inactiveTabsCfrVisible.record(NoExtras())
                    },
                    onInactiveTabsCFRClick = {
                        requireContext().settings().shouldShowInactiveTabsOnboardingPopup = false
                        requireContext().settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                        navigationInteractor.onTabSettingsClicked()
                        TabsTray.inactiveTabsCfrSettings.record(NoExtras())
                        onTabsTrayDismissed()
                    },
                    onInactiveTabsCFRDismiss = {
                        requireContext().settings().shouldShowInactiveTabsOnboardingPopup = false
                        requireContext().settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
                        TabsTray.inactiveTabsCfrDismissed.record(NoExtras())
                    },
                )
            }
        }

        fabButtonComposeBinding.root.setContent {
            FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
                TabsTrayFab(
                    tabsTrayStore = tabsTrayStore,
                    isSignedIn = requireContext().settings().signedInFxaAccount,
                    onNormalTabsFabClicked = tabsTrayInteractor::onNormalTabsFabClicked,
                    onPrivateTabsFabClicked = tabsTrayInteractor::onPrivateTabsFabClicked,
                    onSyncedTabsFabClicked = tabsTrayInteractor::onSyncedTabsFabClicked,
                )
            }
        }

        return tabsTrayDialogBinding.root
    }

    override fun onStart() {
        super.onStart()
        recordBreadcrumb("TabsTrayFragment onStart")
        findPreviousDialogFragment()?.let { dialog ->
            dialog.onAcceptClicked = ::onCancelDownloadWarningAccepted
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        recordBreadcrumb("TabsTrayFragment onDestroyView")
        _tabsTrayDialogBinding = null
        _tabsTrayComposeBinding = null
        _fabButtonComposeBinding = null
    }

    @Suppress("LongMethod")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        TabsTray.opened.record(NoExtras())

        val rootView = tabsTrayComposeBinding.root
        val newTabFab = fabButtonComposeBinding.root

        setupEdgeToEdgeSupport(view)

        val behavior = BottomSheetBehavior.from(rootView).apply {
            addBottomSheetCallback(
                TraySheetBehaviorCallback(
                    this,
                    navigationInteractor,
                    tabsTrayDialog,
                    newTabFab,
                ),
            )
            skipCollapsed = true
        }

        trayBehaviorManager = TabSheetBehaviorManager(
            behavior = behavior,
            orientation = resources.configuration.orientation,
            maxNumberOfTabs = max(
                requireContext().components.core.store.state.normalTabs.size,
                requireContext().components.core.store.state.privateTabs.size,
            ),
            numberForExpandingTray = if (requireContext().settings().gridTabView) {
                EXPAND_AT_GRID_SIZE
            } else {
                EXPAND_AT_LIST_SIZE
            },
            displayMetrics = requireContext().resources.displayMetrics,
        )

        setupBackgroundDismissalListener {
            onTabsTrayDismissed()
        }

        inactiveTabsBinding.set(
            feature = InactiveTabsBinding(
                tabsTrayStore = tabsTrayStore,
                appStore = requireComponents.appStore,
            ),
            owner = this,
            view = view,
        )

        tabsFeature.set(
            feature = TabsFeature(
                tabsTray = TabSorter(
                    requireContext().settings(),
                    tabsTrayStore,
                ),
                store = requireContext().components.core.store,
            ),
            owner = this,
            view = view,
        )

        secureTabsTrayBinding.set(
            feature = SecureTabsTrayBinding(
                store = tabsTrayStore,
                settings = requireComponents.settings,
                fragment = this,
                dialog = dialog as TabsTrayDialog,
            ),
            owner = this,
            view = view,
        )

        syncedTabsIntegration.set(
            feature = SyncedTabsIntegration(
                store = tabsTrayStore,
                context = requireContext(),
                navController = findNavController(),
                storage = requireComponents.backgroundServices.syncedTabsStorage,
                commands = requireComponents.backgroundServices.syncedTabsCommands,
                accountManager = requireComponents.backgroundServices.accountManager,
                lifecycleOwner = this,
            ),
            owner = this,
            view = view,
        )

        setFragmentResultListener(ShareFragment.RESULT_KEY) { _, _ ->
            dismissTabsTray()
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        trayBehaviorManager.updateDependingOnOrientation(newConfig.orientation)
    }

    private fun setupEdgeToEdgeSupport(view: View) {
        if (SDK_INT >= VERSION_CODES.TIRAMISU) {
            ViewCompat.setOnApplyWindowInsetsListener(view) { _, windowInsets ->
                val insets = windowInsets.getInsets(
                    WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout(),
                )
                view.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    leftMargin = insets.left
                    bottomMargin = insets.bottom
                    rightMargin = insets.right
                    topMargin = insets.top
                }

                WindowInsetsCompat.CONSUMED
            }
        }
    }

    private fun onCancelDownloadWarningAccepted(tabId: String?, source: String?) {
        if (tabId != null) {
            tabsTrayInteractor.onDeletePrivateTabWarningAccepted(tabId, source)
        } else {
            navigationInteractor.onCloseAllPrivateTabsWarningConfirmed(private = true)
        }
    }

    private fun showCancelledDownloadWarning(downloadCount: Int, tabId: String?, source: String?) {
        recordBreadcrumb("DownloadCancelDialogFragment show")

        val dialog = DownloadCancelDialogFragment.newInstance(
            downloadCount = downloadCount,
            tabId = tabId,
            source = source,
            promptStyling = DownloadCancelDialogFragment.PromptStyling(
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

            onPositiveButtonClicked = ::onCancelDownloadWarningAccepted,
        )
        dialog.show(parentFragmentManager, DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG)
    }

    @UiThread
    internal fun showUndoSnackbarForSyncedTab(closeOperation: CloseTabsUseCases.UndoableOperation) {
        lifecycleScope.allowUndo(
            view = requireView(),
            message = getString(R.string.snackbar_tab_closed),
            undoActionTitle = getString(R.string.snackbar_deleted_undo),
            onCancel = closeOperation::undo,
            operation = { },
            elevation = ELEVATION,
            anchorView = getSnackbarAnchor(),
        )
    }

    private fun showUndoSnackbarForTab(isPrivate: Boolean) {
        val snackbarMessage =
            when (isPrivate) {
                true -> getString(R.string.snackbar_private_tab_closed)
                false -> getString(R.string.snackbar_tab_closed)
            }
        val pagePosition = if (isPrivate) Page.PrivateTabs.ordinal else Page.NormalTabs.ordinal

        lifecycleScope.allowUndo(
            view = requireView(),
            message = snackbarMessage,
            undoActionTitle = getString(R.string.snackbar_deleted_undo),
            onCancel = {
                requireComponents.useCases.tabsUseCases.undo.invoke()
                tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(pagePosition)))
            },
            operation = { },
            elevation = ELEVATION,
            anchorView = getSnackbarAnchor(),
        )
    }

    private fun showUndoSnackbarForInactiveTab(numClosed: Int) {
        val snackbarMessage =
            when (numClosed == 1) {
                true -> getString(R.string.snackbar_tab_closed)
                false -> getString(R.string.snackbar_num_tabs_closed, numClosed.toString())
            }

        lifecycleScope.allowUndo(
            view = requireView(),
            message = snackbarMessage,
            undoActionTitle = getString(R.string.snackbar_deleted_undo),
            onCancel = {
                requireComponents.useCases.tabsUseCases.undo.invoke()
                tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(Page.NormalTabs.ordinal)))
            },
            operation = { },
            elevation = ELEVATION,
            anchorView = getSnackbarAnchor(),
        )
    }

    private fun setupBackgroundDismissalListener(block: (View) -> Unit) {
        tabsTrayDialogBinding.tabLayout.setOnClickListener(block)
    }

    @VisibleForTesting
    internal fun dismissTabsTrayAndNavigateHome(sessionId: String) {
        navigateToHomeAndDeleteSession(sessionId)
        dismissTabsTray()
    }

    internal val homeViewModel: HomeScreenViewModel by activityViewModels()

    @VisibleForTesting
    internal fun navigateToHomeAndDeleteSession(
        sessionId: String,
        navControllerProvider: NavControllerProvider = DefaultNavControllerProvider(),
    ) {
        homeViewModel.sessionToDelete = sessionId
        val directions = NavGraphDirections.actionGlobalHome()
        navControllerProvider.getNavController(this).navigate(directions)
    }

    @VisibleForTesting
    internal fun getTabPositionFromId(tabsList: List<TabSessionState>, tabId: String): Int {
        tabsList.forEachIndexed { index, tab -> if (tab.id == tabId) return index }
        return -1
    }

    @VisibleForTesting
    internal fun dismissTabsTray() {
        // This should always be the last thing we do because nothing (e.g. telemetry)
        // is guaranteed after that.
        recordBreadcrumb("TabsTrayFragment dismissTabsTray")
        dismissAllowingStateLoss()
    }

    /**
     * Records a breadcrumb for crash reporting.
     *
     * @param message The message to record.
     */
    @VisibleForTesting
    internal fun recordBreadcrumb(message: String) {
        context?.components?.analytics?.crashReporter?.recordCrashBreadcrumb(
            Breadcrumb(message = message),
        )
    }

    private fun showCollectionSnackbar(
        tabSize: Int,
        isNewCollection: Boolean = false,
    ) {
        runIfFragmentIsAttached {
            showSnackbar(
                snackBarParentView = requireView(),
                snackbarState = SnackbarState(
                    message = getString(
                        when {
                            isNewCollection -> {
                                R.string.create_collection_tabs_saved_new_collection
                            }
                            tabSize > 1 -> {
                                R.string.create_collection_tabs_saved
                            }
                            else -> {
                                R.string.create_collection_tab_saved
                            }
                        },
                    ),
                    duration = SnackbarState.Duration.Preset.Long,
                ),
            )
        }
    }

    private fun showBookmarkSnackbar(
        tabSize: Int,
        parentFolderTitle: String?,
    ) {
        val displayFolderTitle = parentFolderTitle ?: getString(R.string.library_bookmarks)
        val displayResId = when {
            tabSize > 1 -> {
                R.string.snackbar_message_bookmarks_saved_in
            }
            else -> {
                R.string.bookmark_saved_in_folder_snackbar
            }
        }

        showSnackbar(
            snackBarParentView = requireView(),
            snackbarState = SnackbarState(
                message = getString(displayResId, displayFolderTitle),
                duration = SnackbarState.Duration.Preset.Long,
                action = Action(
                    label = getString(R.string.create_collection_view),
                    onClick = {
                        findNavController().navigate(
                            TabsTrayFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id),
                        )
                        dismissTabsTray()
                    },
                ),
            ),
        )
    }

    private fun findPreviousDialogFragment(): DownloadCancelDialogFragment? {
        return parentFragmentManager
            .findFragmentByTag(DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG) as? DownloadCancelDialogFragment
    }

    private fun getSnackbarAnchor(): View = fabButtonComposeBinding.root

    private fun showInactiveTabsAutoCloseConfirmationSnackbar() {
        showSnackbar(
            snackBarParentView = tabsTrayComposeBinding.root,
            snackbarState = SnackbarState(
                message = getString(R.string.inactive_tabs_auto_close_message_snackbar),
                duration = SnackbarState.Duration.Preset.Long,
            ),
        )
    }

    private fun showSnackbar(
        snackBarParentView: View,
        snackbarState: SnackbarState,
    ) {
        Snackbar.make(
            snackBarParentView = snackBarParentView,
            snackbarState = snackbarState,
        ).apply {
            setAnchorView(getSnackbarAnchor())
            view.elevation = ELEVATION
            show()
        }
    }

    /**
     * This can only turn the feature ON and should not handle turning the feature OFF.
     */
    private fun onTabsTrayPbmLockedClick(
        navControllerProvider: NavControllerProvider = DefaultNavControllerProvider(),
    ) {
        val isAuthenticatorAvailable =
            BiometricManager.from(requireContext()).isAuthenticatorAvailable()
        if (!isAuthenticatorAvailable) {
            navControllerProvider.getNavController(this)
                .navigate(TabsTrayFragmentDirections.actionGlobalPrivateBrowsingFragment())
        } else {
            DefaultBiometricUtils.bindBiometricsCredentialsPromptOrShowWarning(
                titleRes = R.string.pbm_authentication_enable_lock,
                view = requireView(),
                onShowPinVerification = { intent -> enablePbmPinLauncher.launch(intent) },
                onAuthSuccess = {
                    PrivateBrowsingLocked.bannerPositiveClicked.record()
                    PrivateBrowsingLocked.authSuccess.record()
                    PrivateBrowsingLocked.featureEnabled.record()
                    requireContext().settings().privateBrowsingLockedEnabled = true
                    requireContext().settings().shouldShowLockPbmBanner = false
                },
                onAuthFailure = {
                    PrivateBrowsingLocked.authFailure.record()
                },
            )
        }
    }

    private fun onTabsTrayDismissed() {
        recordBreadcrumb("TabsTrayFragment onTabsTrayDismissed")
        TabsTray.closed.record(NoExtras())
        dismissAllowingStateLoss()
    }

    @VisibleForTesting
    internal fun onTabPageClick(
        biometricAuthenticationNeededInfo: BiometricAuthenticationNeededInfo =
            BiometricAuthenticationManager.biometricAuthenticationNeededInfo,
        biometricUtils: BiometricUtils = DefaultBiometricUtils,
        tabsTrayInteractor: TabsTrayInteractor,
        page: Page,
        isPrivateScreenLocked: Boolean,
    ) {
        val isPrivateTabPage = page == Page.PrivateTabs

        if (isPrivateTabPage && isPrivateScreenLocked) {
            biometricAuthenticationNeededInfo.authenticationStatus =
                AuthenticationStatus.AUTHENTICATION_IN_PROGRESS

            verifyUser(
                biometricUtils = biometricUtils,
                fallbackVerification = verificationResultLauncher,
                onVerified = ::openPrivateTabsPage,
            )
        } else {
            // Reset authentication state when leaving PBM
            if (!isPrivateTabPage) {
                biometricAuthenticationNeededInfo.apply {
                    authenticationStatus = AuthenticationStatus.NOT_AUTHENTICATED
                }
            }
            tabsTrayInteractor.onTrayPositionSelected(page.ordinal, false)
        }
    }

    private fun openPrivateTabsPage() {
        tabsTrayInteractor.onTrayPositionSelected(Page.PrivateTabs.ordinal, false)
    }

    /**
     * Determines whether the Lock Private Browsing Mode banner should be shown.
     *
     * The banner is shown only when all of the following conditions are met:
     * - The app is currently in private browsing mode
     * - There are existing private tabs open
     * - Biometric hardware is available on the device
     * - The user has not already enabled the private browsing lock
     * - The user has not already dismissed or acknowledged the Pbm banner from tabs tray
     *
     * We only want to show the banner when the feature is available,
     * applicable, and relevant to the current user context.
     */
    @VisibleForTesting
    internal fun shouldShowLockPbmBanner(
        isPrivateMode: Boolean,
        hasPrivateTabs: Boolean,
        biometricAvailable: Boolean,
        privateLockEnabled: Boolean,
        shouldShowBanner: Boolean,
    ): Boolean {
        return isPrivateMode && hasPrivateTabs && biometricAvailable && !privateLockEnabled && shouldShowBanner
    }

    companion object {
        private const val DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG = "DOWNLOAD_CANCEL_DIALOG_FRAGMENT_TAG"

        // Minimum number of list items for which to show the tabs tray as expanded.
        const val EXPAND_AT_LIST_SIZE = 4

        // Minimum number of grid items for which to show the tabs tray as expanded.
        private const val EXPAND_AT_GRID_SIZE = 3

        // Elevation for undo toasts
        private const val ELEVATION = 80f

        private const val TABS_TRAY_FEATURE_NAME = "Tabs tray"
    }
}

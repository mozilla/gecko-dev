/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.biometricauthentication

import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.ActivityResultLauncher
import androidx.compose.ui.platform.ComposeView
import androidx.fragment.app.Fragment
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.withResumed
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.feature.customtabs.isCustomTabIntent
import mozilla.components.support.base.feature.UserInteractionHandler
import org.mozilla.fenix.GleanMetrics.PrivateBrowsingLocked
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.registerForActivityResult
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.settings.biometric.DefaultBiometricUtils
import org.mozilla.fenix.tabstray.DefaultTabManagementFeatureHelper
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Fragment used to display biometric authentication when the app is locked.
 */
class UnlockPrivateTabsFragment : Fragment(), UserInteractionHandler {
    private lateinit var startForResult: ActivityResultLauncher<Intent>
    private val args: UnlockPrivateTabsFragmentArgs by navArgs()
    private val navigationOrigin by lazy { args.navigationOrigin }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        startForResult = registerForActivityResult(
            onSuccess = { onAuthSuccess() },
            onFailure = { onAuthFailure() },
        )

        return ComposeView(requireContext()).apply {
            isTransitionGroup = true
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        PrivateBrowsingLocked.promptShown.record()

        val homeActivity = activity as HomeActivity
        val isCustomPrivateTab = isCustomTabIntent(homeActivity.intent) &&
            homeActivity.browsingModeManager.mode.isPrivate

        (view as ComposeView).setContent {
            FirefoxTheme {
                UnlockPrivateTabsScreen(
                    onUnlockClicked = { requestPrompt() },
                    onLeaveClicked = {
                        PrivateBrowsingLocked.seeOtherTabsClicked.record()
                        closeFragment()
                    },
                    showNegativeButton = !isCustomPrivateTab,
                )
            }
        }

        // Delay the prompt until this fragment is resumed to ensure that `BiometricPromptFeature`
        // is wiring up the system `BiometricPrompt` to the right (currently active) fragment.
        viewLifecycleOwner.lifecycleScope.launch {
            viewLifecycleOwner.withResumed {
                requestPrompt()
            }
        }
    }

    override fun onBackPressed(): Boolean {
        closeFragment()
        return true
    }

    private fun requestPrompt() {
        DefaultBiometricUtils.bindBiometricsCredentialsPromptOrShowWarning(
            titleRes = R.string.pbm_authentication_unlock_private_tabs,
            view = requireView(),
            onShowPinVerification = { intent -> startForResult.launch(intent) },
            onAuthSuccess = ::onAuthSuccess,
            onAuthFailure = ::onAuthFailure,
        )
    }

    /**
     * If the users decides to leave the fragment, we want to navigate them to normal tabs page.
     * If they don't have regular opened tabs, we navigate back to homepage as a fallback.
     */
    private fun closeFragment() {
        when (navigationOrigin) {
            // If we locked private mode while the user had a private tab opened or was on private
            // home page, then, when leaving without authentication, we want to navigate user back
            // to normal mode. If they have opened regular tabs, we open the tabs tray as well.
            NavigationOrigin.TAB, NavigationOrigin.HOME_PAGE -> {
                (activity as HomeActivity).browsingModeManager.mode = BrowsingMode.Normal

                findNavController().navigate(UnlockPrivateTabsFragmentDirections.actionGlobalHome())

                val hasNormalTabs = requireComponents.core.store.state.normalTabs.isNotEmpty()
                if (hasNormalTabs) {
                    if (DefaultTabManagementFeatureHelper.enhancementsEnabled) {
                        findNavController().navigate(
                            HomeFragmentDirections.actionGlobalTabManagementFragment(
                                page = Page.NormalTabs,
                            ),
                        )
                    } else {
                        findNavController().navigate(
                            HomeFragmentDirections.actionGlobalTabsTrayFragment(
                                page = Page.NormalTabs,
                            ),
                        )
                    }
                }
            }
            // If we locked private mode while the user had the tabs tray private page opened, then
            // going to the previous state means just closing the fragment and defaulting to tabs
            // tray regular page
            NavigationOrigin.TABS_TRAY -> {
                findNavController().popBackStack()

                if (DefaultTabManagementFeatureHelper.enhancementsEnabled) {
                    findNavController().navigate(
                        HomeFragmentDirections.actionGlobalTabManagementFragment(
                            page = Page.NormalTabs,
                        ),
                    )
                } else {
                    findNavController().navigate(
                        HomeFragmentDirections.actionGlobalTabsTrayFragment(
                            page = Page.NormalTabs,
                        ),
                    )
                }
            }
        }
    }

    private fun onAuthSuccess() {
        PrivateBrowsingLocked.authSuccess.record()

        requireComponents.privateBrowsingLockFeature.onSuccessfulAuthentication()

        findNavController().popBackStack()

        if (navigationOrigin == NavigationOrigin.TABS_TRAY) {
            if (DefaultTabManagementFeatureHelper.enhancementsEnabled) {
                findNavController().navigate(
                    HomeFragmentDirections.actionGlobalTabManagementFragment(
                        page = Page.PrivateTabs,
                    ),
                )
            } else {
                findNavController().navigate(
                    HomeFragmentDirections.actionGlobalTabsTrayFragment(
                        page = Page.PrivateTabs,
                    ),
                )
            }
        }
    }

    private fun onAuthFailure() {
        PrivateBrowsingLocked.authFailure.record()
    }
}

/**
 * The navigation entry point from which the fragment was triggered.
 *
 * It helps determine the correct navigation behaviour when the user is leaving the fragment.
 */
enum class NavigationOrigin {
    TABS_TRAY,
    HOME_PAGE,
    TAB,
}

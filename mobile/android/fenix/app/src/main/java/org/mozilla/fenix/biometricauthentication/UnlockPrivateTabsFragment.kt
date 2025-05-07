/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.biometricauthentication

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricManager.Authenticators.DEVICE_CREDENTIAL
import androidx.biometric.BiometricPrompt
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.stringResource
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import org.mozilla.fenix.GleanMetrics.PrivateBrowsingLocked
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.AppAction.PrivateBrowsingLockAction
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.tabstray.Page
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Fragment used to display biometric authentication when the app is locked.
 */
class UnlockPrivateTabsFragment : Fragment() {

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        return ComposeView(requireContext()).apply {
            isTransitionGroup = true
        }
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        PrivateBrowsingLocked.promptShown.record()

        (view as ComposeView).setContent {
            FirefoxTheme {
                val title = stringResource(R.string.pbm_authentication_unlock_private_tabs)

                UnlockPrivateTabsScreen(
                    onUnlockClicked = { requestPrompt(title) },
                    onLeaveClicked = { leavePrivateMode() },
                )

                requestPrompt(title)
            }
        }
    }

    private fun requestPrompt(title: String) {
        val biometricPrompt = BiometricPrompt(
            this,
            object : BiometricPrompt.AuthenticationCallback() {
                override fun onAuthenticationSucceeded(
                    result: BiometricPrompt.AuthenticationResult,
                ) {
                    super.onAuthenticationSucceeded(result)

                    onAuthSuccess()
                }

                override fun onAuthenticationFailed() {
                    super.onAuthenticationFailed()

                    onAuthFailure()
                }
            },
        )

        val promptInfo = BiometricPrompt.PromptInfo.Builder()
            .setTitle(title)
            .setAllowedAuthenticators(DEVICE_CREDENTIAL or BiometricManager.Authenticators.BIOMETRIC_WEAK)
            .build()

        biometricPrompt.authenticate(promptInfo)
    }

    private fun leavePrivateMode() {
        PrivateBrowsingLocked.seeOtherTabsClicked.record()

        findNavController().popBackStack(R.id.homeFragment, true)
        findNavController().navigate(HomeFragmentDirections.actionGlobalTabsTrayFragment(page = Page.NormalTabs))
    }

    private fun onAuthSuccess() {
        PrivateBrowsingLocked.authSuccess.record()

        requireComponents.appStore.dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = false,
            ),
        )

        findNavController().popBackStack()
    }

    private fun onAuthFailure() {
        PrivateBrowsingLocked.authFailure.record()

        requireComponents.appStore.dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = true,
            ),
        )
    }
}

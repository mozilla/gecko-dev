/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

import androidx.activity.OnBackPressedCallback
import androidx.activity.OnBackPressedDispatcher
import androidx.fragment.app.FragmentManager

/**
 * Custom callback class that manages the back navigation inside of an activity.
 *
 * This class navigates through the fragment stack and makes use of the mechanism that delegates back press handling to
 * child fragments that implement the [UserInteractionHandler] interface. If no fragment handles the back press, it
 * falls back to the default system behavior (either navigating back or exiting the app).
 *
 * NB: the callback is disabled by default and should be enabled in an appropriate lifecycle call.
 * Example of enabling in `onResume`:
 *
 * override fun onResume() {
 *     super.onResume()
 *     backPressedCallback.isEnabled = true
 * }
 *
 * @param fragmentManager used to manage fragment transactions and navigation.
 * @param dispatcher responsible for managing back press callbacks within the activity lifecycle.
 */
class UserInteractionOnBackPressedCallback(
    private val fragmentManager: FragmentManager,
    private val dispatcher: OnBackPressedDispatcher,
) : OnBackPressedCallback(false) {
    override fun handleOnBackPressed() {
        var onBackPressedHandled = false

        fragmentManager.primaryNavigationFragment?.childFragmentManager?.fragments?.forEach {
            if (it is UserInteractionHandler && it.onBackPressed()) {
                onBackPressedHandled = true
            }
        }

        if (!onBackPressedHandled) {
            val backStackCount =
                fragmentManager.primaryNavigationFragment?.childFragmentManager?.backStackEntryCount ?: 0
            if (backStackCount == 0) {
                // NB: Disabling the callback enables the default system handling of the back navigation.
                // In this case it means closing the current activity
                this.isEnabled = false
                dispatcher.onBackPressed()
            } else {
                fragmentManager.popBackStack()
            }
        }
    }
}

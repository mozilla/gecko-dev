/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.AppCompatImageButton
import androidx.appcompat.widget.Toolbar
import androidx.core.view.children
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R

/**
 * General utilities to to improve a11y support, such as managing screen reader focus.
 */
object AccessibilityUtils {
    /**
     * This function attempts to move focus to the back button on the navigation bar as a generic
     * default location to send focus.
     *
     * Only use this function when focus should be specifically controlled due to TalkBack focus not
     * consistently setting or other inconsistent behavior.
     *
     * Caution, focus should only be manually controlled in certain situations. Be sure this is the
     * ideal solution.
     *
     * @param activity The activity that focus should move to.
     */
    fun moveFocusToBackNavButton(activity: AppCompatActivity) {
        val toolbar = (activity as HomeActivity).findViewById<Toolbar>(R.id.navigationToolbar)
        var backNavigationView: AppCompatImageButton? = null
        for (child in toolbar.children) {
            if (child is AppCompatImageButton) {
                backNavigationView = child
            }
        }

        // Ensures the back button is focusable
        backNavigationView?.isFocusableInTouchMode = true
        // Clear current focus
        backNavigationView?.clearFocus()
        backNavigationView?.requestFocus()
        // Removes depressed UI state from back button
        backNavigationView?.clearFocus()
    }
}

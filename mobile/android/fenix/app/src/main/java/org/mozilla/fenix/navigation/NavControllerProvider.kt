/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.navigation

import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController

/**
 * Interface for providing a [.NavController] instance associated with a specific [Fragment].
 *
 *  A class that implements this interface should provide a concrete implementation of the
 * [getNavController] function to return the appropriate NavController.
 */
interface NavControllerProvider {
    /**
     * Retrieves the [NavController] associated with a given [Fragment].
     *
     * This function simplifies accessing the NavController for navigation actions within a fragment.
     * It assumes the NavController is attached to the fragment's host activity or a parent fragment.
     *
     * @param fragment The [Fragment] whose associated [NavController] is to be retrieved.
     * @return The [NavController] for the provided fragment.
     */
    fun getNavController(fragment: Fragment): NavController
}

/**
 * Default implementation of [NavControllerProvider] that retrieves the [NavController] associated with a Fragment.
 *
 * This class uses the standard [findNavController] method to obtain the NavController.  It serves as a
 * straightforward provider in cases where no custom logic is needed for retrieving the NavController.
 *
 */
class DefaultNavControllerProvider : NavControllerProvider {
    override fun getNavController(fragment: Fragment): NavController {
        return fragment.findNavController()
    }
}

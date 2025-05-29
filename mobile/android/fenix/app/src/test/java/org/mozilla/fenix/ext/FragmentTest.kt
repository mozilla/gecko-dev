/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.Runs
import io.mockk.confirmVerified
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.navigation.NavControllerProvider

class FragmentTest {

    private val navDirections: NavDirections = mockk(relaxed = true)
    private val mockDestination = spyk(NavDestination("hi"))
    private val mockId = 4
    private val navController: NavController = mockk(relaxed = true)
    private val mockFragment: Fragment = mockk(relaxed = true)
    private val mockOptions: NavOptions = mockk(relaxed = true)
    private val navControllerProvider: NavControllerProvider = mockk(relaxed = true)

    @Before
    fun setup() {
        every { navControllerProvider.getNavController(mockFragment) } returns navController
        every { navController.currentDestination } returns mockDestination
        every { mockDestination.id } returns mockId
    }

    @Test
    fun `Test nav fun with ID and directions`() {
        every { navController.navigate(navDirections, null) } just Runs

        mockFragment.nav(mockId, navDirections, navControllerProvider = navControllerProvider)
        verify { navController.currentDestination }
        verify { navController.navigate(navDirections, null) }
        confirmVerified(mockFragment)
    }

    @Test
    fun `Test nav fun with ID, directions, and options`() {
        every { navController.navigate(navDirections, mockOptions) } just Runs

        mockFragment.nav(mockId, navDirections, mockOptions, navControllerProvider = navControllerProvider)
        verify { navController.currentDestination }
        verify { navController.navigate(navDirections, mockOptions) }
        confirmVerified(mockFragment)
    }
}

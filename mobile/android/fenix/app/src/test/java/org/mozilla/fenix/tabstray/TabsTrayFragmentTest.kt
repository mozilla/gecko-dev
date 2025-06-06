/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.content.res.Configuration
import android.view.LayoutInflater
import androidx.navigation.NavController
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.biometricauthentication.AuthenticationStatus
import org.mozilla.fenix.biometricauthentication.BiometricAuthenticationNeededInfo
import org.mozilla.fenix.databinding.FragmentTabTrayDialogBinding
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.mozilla.fenix.helpers.MockkRetryTestRule
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.navigation.NavControllerProvider
import org.mozilla.fenix.settings.biometric.BiometricUtils
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TabsTrayFragmentTest {
    private lateinit var context: Context
    private lateinit var fragment: TabsTrayFragment
    private lateinit var tabsTrayDialogBinding: FragmentTabTrayDialogBinding

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    @Before
    fun setup() {
        context = mockk(relaxed = true)
        val inflater = LayoutInflater.from(testContext)
        tabsTrayDialogBinding = FragmentTabTrayDialogBinding.inflate(inflater)

        fragment = spyk(TabsTrayFragment())
        fragment._tabsTrayDialogBinding = tabsTrayDialogBinding
        every { fragment.context } returns context
        every { fragment.viewLifecycleOwner } returns mockk(relaxed = true)
    }

    @Test
    fun `WHEN dismissTabsTrayAndNavigateHome is called with a sessionId THEN it navigates to home to delete that sessions and dismisses the tray`() {
        every { fragment.navigateToHomeAndDeleteSession(any(), any()) } just Runs
        every { fragment.dismissTabsTray() } just Runs

        fragment.dismissTabsTrayAndNavigateHome("test")

        verify { fragment.navigateToHomeAndDeleteSession("test", any()) }
        verify { fragment.dismissTabsTray() }
    }

    @Test
    fun `WHEN navigateToHomeAndDeleteSession is called with a sessionId THEN it navigates to home and transmits there the sessionId`() {
        val viewModel: HomeScreenViewModel = mockk(relaxed = true)
        every { fragment.homeViewModel } returns viewModel
        val navController: NavController = mockk(relaxed = true)

        val navControllerProvider: NavControllerProvider = mockk()
        every { navControllerProvider.getNavController(fragment) } returns navController

        fragment.navigateToHomeAndDeleteSession(
            "test",
            navControllerProvider = navControllerProvider,
        )

        verify { viewModel.sessionToDelete = "test" }
        verify { navController.navigate(NavGraphDirections.actionGlobalHome()) }
    }

    @Test
    fun `WHEN dismissTabsTray is called THEN it dismisses the tray`() {
        every { fragment.dismissAllowingStateLoss() } just Runs
        every { fragment.recordBreadcrumb(any()) } just Runs

        fragment.dismissTabsTray()

        verify { fragment.dismissAllowingStateLoss() }
    }

    @Test
    fun `WHEN onConfigurationChanged is called THEN it delegates the tray behavior manager to update the tray`() {
        val trayBehaviorManager: TabSheetBehaviorManager = mockk(relaxed = true)
        fragment.trayBehaviorManager = trayBehaviorManager
        val newConfiguration = Configuration()
        every { context.settings().gridTabView } returns false

        fragment.onConfigurationChanged(newConfiguration)

        verify { trayBehaviorManager.updateDependingOnOrientation(newConfiguration.orientation) }
    }

    @Test
    fun `GIVEN a list of tabs WHEN a tab is present with an ID THEN the index is returned`() {
        val tab1 = TabSessionState(
            id = "tab1",
            content = ContentState(
                url = "https://mozilla.org",
                private = false,
                isProductUrl = false,
            ),
        )
        val tab2 = TabSessionState(
            id = "tab2",
            content = ContentState(
                url = "https://mozilla.org",
                private = false,
                isProductUrl = false,
            ),
        )
        val tab3 = TabSessionState(
            id = "tab3",
            content = ContentState(
                url = "https://mozilla.org",
                private = false,
                isProductUrl = false,
            ),
        )
        val tabsList = listOf(
            tab1,
            tab2,
            tab3,
        )
        val position = fragment.getTabPositionFromId(tabsList, "tab2")
        assertEquals(1, position)
    }

    // tests for onTabPageClick
    @Test
    fun `WHEN shouldShowPrompt is true THEN onTabPageClick sets the authentication status to authentication in progress and calls show prompt`() {
        every { fragment.view } returns mockk()
        every { fragment.requireComponents.core.store.state.privateTabs } returns listOf(
            mockk(),
            mockk(),
        )
        every { fragment.requireContext().settings().privateBrowsingLockedEnabled } returns true
        every { fragment.tabsTrayStore.state.selectedPage } returns Page.NormalTabs
        every { fragment.verificationResultLauncher } returns mockk()

        val biometricAuthenticationNeededInfo = BiometricAuthenticationNeededInfo()
        val biometricUtils = mockk<BiometricUtils>(relaxed = true)
        val tabsTrayInteractor = mockk<TabsTrayInteractor>(relaxed = true)
        val page = Page.PrivateTabs
        fragment.onTabPageClick(
            biometricAuthenticationNeededInfo = biometricAuthenticationNeededInfo,
            biometricUtils = biometricUtils,
            tabsTrayInteractor = tabsTrayInteractor,
            page = page,
            isPrivateScreenLocked = true,
        )

        assertEquals(
            AuthenticationStatus.AUTHENTICATION_IN_PROGRESS,
            biometricAuthenticationNeededInfo.authenticationStatus,
        )
        verify {
            biometricUtils.bindBiometricsCredentialsPromptOrShowWarning(
                any(),
                any(),
                any(),
                any(),
                any(),
            )
        }
    }

    @Test
    fun `WHEN shouldShowPrompt is false and tab page is private THEN onTabPageClick calls tabs tray interactor only`() {
        every { fragment.view } returns mockk()
        every { fragment.requireComponents.core.store.state.privateTabs } returns listOf(
            mockk(),
            mockk(),
        )
        every { fragment.requireContext().settings().privateBrowsingLockedEnabled } returns false
        every { fragment.tabsTrayStore.state.selectedPage } returns Page.PrivateTabs

        val biometricAuthenticationNeededInfo =
            BiometricAuthenticationNeededInfo(authenticationStatus = AuthenticationStatus.AUTHENTICATED)
        val biometricUtils = mockk<BiometricUtils>(relaxed = true)
        val tabsTrayInteractor = mockk<TabsTrayInteractor>(relaxed = true)
        val page = Page.PrivateTabs
        fragment.onTabPageClick(
            biometricAuthenticationNeededInfo = biometricAuthenticationNeededInfo,
            biometricUtils = biometricUtils,
            tabsTrayInteractor = tabsTrayInteractor,
            page = page,
            isPrivateScreenLocked = false,
        )

        assertEquals(
            AuthenticationStatus.AUTHENTICATED, // unchanged
            biometricAuthenticationNeededInfo.authenticationStatus,
        )
        verify { tabsTrayInteractor.onTrayPositionSelected(page.ordinal, false) }
        verify(inverse = true) {
            biometricUtils.bindBiometricsCredentialsPromptOrShowWarning(
                any(),
                any(),
                any(),
                any(),
                any(),
            )
        }
    }

    @Test
    fun `WHEN shouldShowPrompt is false and not a private tab page THEN onTabPageClick sets the authentication status to not authenticated and calls tabs tray interactor`() {
        every { fragment.view } returns mockk()
        every { fragment.requireComponents.core.store.state.privateTabs } returns listOf(
            mockk(),
            mockk(),
        )
        every { fragment.requireContext().settings().privateBrowsingLockedEnabled } returns false
        every { fragment.tabsTrayStore.state.selectedPage } returns Page.NormalTabs

        val biometricAuthenticationNeededInfo =
            BiometricAuthenticationNeededInfo(authenticationStatus = AuthenticationStatus.AUTHENTICATED)
        val biometricUtils = mockk<BiometricUtils>(relaxed = true)
        val tabsTrayInteractor = mockk<TabsTrayInteractor>(relaxed = true)
        val page = Page.NormalTabs
        fragment.onTabPageClick(
            biometricAuthenticationNeededInfo = biometricAuthenticationNeededInfo,
            biometricUtils = biometricUtils,
            tabsTrayInteractor = tabsTrayInteractor,
            page = page,
            isPrivateScreenLocked = false,
        )

        assertEquals(
            AuthenticationStatus.NOT_AUTHENTICATED,
            biometricAuthenticationNeededInfo.authenticationStatus,
        )
        verify { tabsTrayInteractor.onTrayPositionSelected(0, false) }
        verify(inverse = true) {
            biometricUtils.bindBiometricsCredentialsPromptOrShowWarning(
                any(),
                any(),
                any(),
                any(),
                any(),
            )
        }
    }

    @Test
    fun `WHEN all conditions are met THEN shouldShowLockPbmBanner returns true`() {
        val result = testShouldShowLockPbmBanner()
        assertTrue(result)
    }

    @Test
    fun `WHEN isPrivateMode is false THEN shouldShowLockPbmBanner returns false`() {
        val result = testShouldShowLockPbmBanner(isPrivateMode = false)
        assertFalse(result)
    }

    @Test
    fun `WHEN hasPrivateTabs is false THEN shouldShowLockPbmBanner returns false`() {
        val result = testShouldShowLockPbmBanner(hasPrivateTabs = false)
        assertFalse(result)
    }

    @Test
    fun `WHEN biometricAvailable is false THEN shouldShowLockPbmBanner returns false`() {
        val result = testShouldShowLockPbmBanner(biometricAvailable = false)
        assertFalse(result)
    }

    @Test
    fun `WHEN privateLockEnabled is true THEN shouldShowLockPbmBanner returns false`() {
        val result = testShouldShowLockPbmBanner(privateLockEnabled = true)
        assertFalse(result)
    }

    @Test
    fun `WHEN shouldShowBanner is false THEN shouldShowLockPbmBanner returns false`() {
        val result = testShouldShowLockPbmBanner(shouldShowBanner = false)
        assertFalse(result)
    }

    private fun testShouldShowLockPbmBanner(
        isPrivateMode: Boolean = true,
        hasPrivateTabs: Boolean = true,
        biometricAvailable: Boolean = true,
        privateLockEnabled: Boolean = false,
        shouldShowBanner: Boolean = true,
    ): Boolean {
        return fragment.shouldShowLockPbmBanner(
            isPrivateMode = isPrivateMode,
            hasPrivateTabs = hasPrivateTabs,
            biometricAvailable = biometricAvailable,
            privateLockEnabled = privateLockEnabled,
            shouldShowBanner = shouldShowBanner,
        )
    }
}

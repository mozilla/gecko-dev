/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.view.LayoutInflater
import android.view.View
import androidx.navigation.NavController
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.databinding.FragmentTabTrayDialogBinding
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
        every { fragment.view } returns mockk()
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
    fun `GIVEN private screen is locked WHEN a private tab is clicked THEN the biometrics prompt is shown and the tabs tray page selected`() {
        var isBiometricsPromptCalled = false
        var isTabsTrayInteractorCalled = false
        val biometricUtils = buildTestBiometricUtils {
            isBiometricsPromptCalled = true
        }
        val testInteractor = buildTestInteractor(
            onTrayPositionSelected = {
                isTabsTrayInteractorCalled = true
            },
        )

        fragment.onTabPageClick(
            biometricUtils = biometricUtils,
            tabsTrayInteractor = testInteractor,
            page = Page.PrivateTabs,
            isPrivateScreenLocked = true,
        )

        assertFalse(isTabsTrayInteractorCalled)
        assertTrue(isBiometricsPromptCalled)
    }

    @Test
    fun `GIVEN private screen is unlocked WHEN a private tab is clicked THEN the biometrics prompt is not shown and the tabs tray page selected`() {
        var isBiometricsPromptCalled = false
        var isTabsTrayInteractorCalled = false
        val biometricUtils = buildTestBiometricUtils {
            isBiometricsPromptCalled = true
        }
        val testInteractor = buildTestInteractor(
            onTrayPositionSelected = {
                isTabsTrayInteractorCalled = true
            },
        )

        fragment.onTabPageClick(
            biometricUtils = biometricUtils,
            tabsTrayInteractor = testInteractor,
            page = Page.PrivateTabs,
            isPrivateScreenLocked = false,
        )

        assertTrue(isTabsTrayInteractorCalled)
        assertFalse(isBiometricsPromptCalled)
    }

    @Test
    fun `GIVEN private screen is locked WHEN a regular tab is clicked THEN the biometrics prompt is not shown and the tabs tray page selected`() {
        var isBiometricsPromptCalled = false
        var isTabsTrayInteractorCalled = false
        val biometricUtils = buildTestBiometricUtils {
            isBiometricsPromptCalled = true
        }
        val testInteractor = buildTestInteractor(
            onTrayPositionSelected = {
                isTabsTrayInteractorCalled = true
            },
        )

        fragment.onTabPageClick(
            biometricUtils = biometricUtils,
            tabsTrayInteractor = testInteractor,
            page = Page.NormalTabs,
            isPrivateScreenLocked = true,
        )

        assertTrue(isTabsTrayInteractorCalled)
        assertFalse(isBiometricsPromptCalled)
    }

    @Test
    fun `GIVEN private screen is unlocked WHEN a regular tab is clicked THEN the biometrics prompt is not shown and the tabs tray page selected`() {
        var isBiometricsPromptCalled = false
        var isTabsTrayInteractorCalled = false
        val biometricUtils = buildTestBiometricUtils {
            isBiometricsPromptCalled = true
        }
        val testInteractor = buildTestInteractor(
            onTrayPositionSelected = {
                isTabsTrayInteractorCalled = true
            },
        )

        fragment.onTabPageClick(
            biometricUtils = biometricUtils,
            tabsTrayInteractor = testInteractor,
            page = Page.NormalTabs,
            isPrivateScreenLocked = false,
        )

        assertTrue(isTabsTrayInteractorCalled)
        assertFalse(isBiometricsPromptCalled)
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

private fun buildTestBiometricUtils(
    onBiometricsPromptCalled: () -> Unit,
) = object : BiometricUtils {
    override fun bindBiometricsCredentialsPromptOrShowWarning(
        titleRes: Int,
        view: View,
        onShowPinVerification: (Intent) -> Unit,
        onAuthSuccess: () -> Unit,
        onAuthFailure: () -> Unit,
    ) {
        onBiometricsPromptCalled()
    }
}

private fun buildTestInteractor(
    onTrayPositionSelected: () -> Unit,
) = object : TabsTrayInteractor {
    override fun onTrayPositionSelected(position: Int, smoothScroll: Boolean) {
        onTrayPositionSelected()
    }

    // no-op
    override fun onDeletePrivateTabWarningAccepted(tabId: String, source: String?) {}
    override fun onDeleteSelectedTabsClicked() {}
    override fun onForceSelectedTabsAsInactiveClicked() {}
    override fun onBookmarkSelectedTabsClicked() {}
    override fun onAddSelectedTabsToCollectionClicked() {}
    override fun onShareSelectedTabs() {}
    override fun onTabsMove(tabId: String, targetId: String?, placeAfter: Boolean) {}
    override fun onRecentlyClosedClicked() {}
    override fun onMediaClicked(tab: TabSessionState) {}
    override fun onTabLongClicked(tab: TabSessionState): Boolean { return false }
    override fun onBackPressed(): Boolean { return false }
    override fun onTabUnselected(tab: TabSessionState) {}
    override fun onSyncedTabClicked(tab: Tab) {}
    override fun onSyncedTabClosed(deviceId: String, tab: Tab) {}
    override fun onTabSelected(tab: TabSessionState, source: String?) {}
    override fun onTabClosed(tab: TabSessionState, source: String?) {}
    override fun onInactiveTabsHeaderClicked(expanded: Boolean) {}
    override fun onInactiveTabClicked(tab: TabSessionState) {}
    override fun onInactiveTabClosed(tab: TabSessionState) {}
    override fun onDeleteAllInactiveTabsClicked() {}
    override fun onAutoCloseDialogCloseButtonClicked() {}
    override fun onEnableAutoCloseClicked() {}
    override fun onNormalTabsFabClicked() {}
    override fun onPrivateTabsFabClicked() {}
    override fun onSyncedTabsFabClicked() {}
}

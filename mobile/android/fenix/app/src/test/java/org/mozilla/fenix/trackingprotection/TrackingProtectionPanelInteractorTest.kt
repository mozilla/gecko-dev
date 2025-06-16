/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.content.Context
import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.NavDirections
import io.mockk.MockKAnnotations
import io.mockk.coVerify
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.cookiehandling.CookieBannersStorage
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings

class TrackingProtectionPanelInteractorTest {

    private lateinit var context: Context

    @MockK(relaxed = true)
    private lateinit var navController: NavController

    @MockK(relaxed = true)
    private lateinit var fragment: Fragment

    @MockK(relaxed = true)
    private lateinit var sitePermissions: SitePermissions

    @MockK(relaxed = true)
    private lateinit var store: ProtectionsStore

    private lateinit var interactor: TrackingProtectionPanelInteractor

    private lateinit var tab: TabSessionState

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    private var learnMoreClicked = false
    private var openSettings = false
    private var gravity = 54

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        learnMoreClicked = false

        context = mockk()
        tab = createTab("https://mozilla.org", id = "testID")
        val cookieBannersStorage: CookieBannersStorage = mockk(relaxed = true)

        interactor = TrackingProtectionPanelInteractor(
            context = context,
            fragment = fragment,
            store = store,
            ioScope = scope,
            cookieBannersStorage = cookieBannersStorage,
            navController = { navController },
            openTrackingProtectionSettings = { openSettings = true },
            openLearnMoreLink = { learnMoreClicked = true },
            sitePermissions = sitePermissions,
            gravity = gravity,
            getCurrentTab = { tab },
        )

        val trackingProtectionUseCases: TrackingProtectionUseCases = mockk(relaxed = true)

        every { fragment.context } returns context
        every { context.components.useCases.trackingProtectionUseCases } returns trackingProtectionUseCases
        every { context.components.appStore.state.isPrivateScreenLocked } returns true
    }

    @Test
    fun `WHEN openDetails is called THEN store should dispatch EnterDetailsMode action with the right category`() {
        interactor.openDetails(TrackingProtectionCategory.FINGERPRINTERS, true)

        verify {
            store.dispatch(
                ProtectionsAction.EnterDetailsMode(
                    TrackingProtectionCategory.FINGERPRINTERS,
                    true,
                ),
            )
        }

        interactor.openDetails(TrackingProtectionCategory.REDIRECT_TRACKERS, true)

        verify {
            store.dispatch(
                ProtectionsAction.EnterDetailsMode(
                    TrackingProtectionCategory.REDIRECT_TRACKERS,
                    true,
                ),
            )
        }
    }

    @Test
    fun `WHEN selectTrackingProtectionSettings is called THEN openTrackingProtectionSettings should be invoked`() {
        interactor.selectTrackingProtectionSettings()

        assertEquals(true, openSettings)
    }

    @Test
    fun `WHEN on the learn more link is clicked THEN onLearnMoreClicked should be invoked`() {
        interactor.onLearnMoreClicked()

        assertEquals(true, learnMoreClicked)
    }

    @Test
    fun `WHEN onBackPressed is called THEN call popBackStack and navigate`() = runTestOnMain {
        every { context.settings().shouldUseCookieBannerPrivateMode } returns false
        val directionsSlot = slot<NavDirections>()

        interactor.handleNavigationAfterCheck(tab, true)

        coVerify {
            navController.popBackStack()

            navController.navigate(capture(directionsSlot))
        }

        val capturedDirections = directionsSlot.captured

        assertTrue(directionsSlot.isCaptured)
        assertEquals(
            R.id.action_global_quickSettingsSheetDialogFragment,
            capturedDirections.actionId,
        )
    }

    @Test
    fun `WHEN onExitDetailMode is called THEN store should dispatch ExitDetailsMode action`() {
        interactor.onExitDetailMode()

        verify { store.dispatch(ProtectionsAction.ExitDetailsMode) }
    }
}

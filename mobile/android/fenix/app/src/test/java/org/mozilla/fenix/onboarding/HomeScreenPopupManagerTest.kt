package org.mozilla.fenix.onboarding

import io.mockk.every
import io.mockk.mockk
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class HomeScreenPopupManagerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Mock
    private lateinit var appStore: AppStore

    @Mock
    private var settings: Settings = mockk()

    private lateinit var homeScreenPopupManager: HomeScreenPopupManager

    @Before
    fun setUp() {
        appStore = AppStore()
        every { settings.shouldShowNavigationBarCFR } returns true
        homeScreenPopupManager = HomeScreenPopupManager(appStore, settings)
    }

    @Test
    fun `WHEN search dialog becomes visible THEN navbar CFR gets hidden`() {
        val searchDialogVisibility = true

        homeScreenPopupManager.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(false, homeScreenPopupManager.navBarCFRVisibility.value)
    }

    @Test
    fun `WHEN search dialog is not visible THEN navbar CFR is shown`() {
        val searchDialogVisibility = false

        homeScreenPopupManager.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(true, homeScreenPopupManager.navBarCFRVisibility.value)
    }
}

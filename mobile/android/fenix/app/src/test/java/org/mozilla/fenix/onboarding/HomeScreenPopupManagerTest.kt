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

    @Mock
    private var nimbusManager: HomeScreenPopupManagerNimbusManager = mockk(relaxed = true)

    @Before
    fun setUp() {
        appStore = AppStore()
    }

    @Test
    fun `WHEN search dialog becomes visible THEN navbar CFR gets hidden`() {
        every { settings.shouldShowNavigationBarCFR } returns true
        val homeScreenPopupManager = HomeScreenPopupManager(appStore, settings, nimbusManager)

        val searchDialogVisibility = true

        homeScreenPopupManager.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(false, homeScreenPopupManager.navBarCFRVisibility.value)
    }

    @Test
    fun `WHEN search dialog is not visible THEN navbar CFR is shown`() {
        every { settings.shouldShowNavigationBarCFR } returns true
        val homeScreenPopupManager = HomeScreenPopupManager(appStore, settings, nimbusManager)

        val searchDialogVisibility = false

        homeScreenPopupManager.start()
        appStore.dispatch(AppAction.UpdateSearchDialogVisibility(isVisible = searchDialogVisibility)).joinBlocking()

        assertEquals(true, homeScreenPopupManager.navBarCFRVisibility.value)
    }

    @Test
    fun `WHEN the nav CFR is disabled AND the search bar CFR is enabled THEN the search bar CFR will show`() {
        testSearchBar(
            navCfrEnabled = false,
            searchBarCfrEnabled = true,
            expectedToShow = true,
        )
    }

    @Test
    fun `WHEN the nav CFR is enabled AND the search bar CFR is enabled THEN the search bar CFR will not show`() {
        testSearchBar(
            navCfrEnabled = true,
            searchBarCfrEnabled = true,
            expectedToShow = false,
        )
    }

    @Test
    fun `WHEN the nav CFR is enabled AND the search bar CFR is disabled THEN the search bar CFR will not show`() {
        testSearchBar(
            navCfrEnabled = true,
            searchBarCfrEnabled = false,
            expectedToShow = false,
        )
    }

    @Test
    fun `WHEN the nav CFR is disabled AND the search bar CFR is disabled THEN the search bar CFR will not show`() {
        testSearchBar(
            navCfrEnabled = false,
            searchBarCfrEnabled = false,
            expectedToShow = false,
        )
    }

    private fun testSearchBar(
        navCfrEnabled: Boolean,
        searchBarCfrEnabled: Boolean,
        expectedToShow: Boolean,
    ) {
        every { settings.shouldShowNavigationBarCFR } returns navCfrEnabled
        every { settings.shouldShowSearchBarCFR } returns searchBarCfrEnabled
        every { settings.canShowCfr } returns true

        val homeScreenPopupManager = HomeScreenPopupManager(appStore, settings, nimbusManager)

        homeScreenPopupManager.start()

        assertEquals(expectedToShow, homeScreenPopupManager.searchBarCFRVisibility.value)
    }
}

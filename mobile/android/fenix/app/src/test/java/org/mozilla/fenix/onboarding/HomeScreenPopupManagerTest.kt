package org.mozilla.fenix.onboarding

import io.mockk.every
import io.mockk.mockk
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
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
    fun `WHEN search bar CFR is enabled THEN the search bar CFR will show`() {
        testSearchBar(
            searchBarCfrEnabled = true,
            expectedToShow = true,
        )
    }

    @Test
    fun `WHEN the search bar CFR is disabled THEN the search bar CFR will not show`() {
        testSearchBar(
            searchBarCfrEnabled = false,
            expectedToShow = false,
        )
    }

    @Test
    fun `WHEN search bar CFR is disabled THEN the search bar CFR will not show`() {
        testSearchBar(
            searchBarCfrEnabled = false,
            expectedToShow = false,
        )
    }

    private fun testSearchBar(
        searchBarCfrEnabled: Boolean,
        expectedToShow: Boolean,
    ) {
        every { settings.shouldShowSearchBarCFR } returns searchBarCfrEnabled
        every { settings.canShowCfr } returns true

        val homeScreenPopupManager = HomeScreenPopupManager(settings, nimbusManager)

        homeScreenPopupManager.start()

        assertEquals(expectedToShow, homeScreenPopupManager.searchBarCFRVisibility.value)
    }
}

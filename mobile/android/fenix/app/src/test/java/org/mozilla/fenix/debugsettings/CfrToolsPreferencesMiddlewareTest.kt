/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.flowOf
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.debugsettings.cfrs.CfrPreferencesRepository
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsAction
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsPreferencesMiddleware
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsState
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsStore

@RunWith(AndroidJUnit4::class)
class CfrToolsPreferencesMiddlewareTest {

    @get:Rule
    val mainCoroutineTestRule = MainCoroutineRule()

    private lateinit var cfrPreferencesRepository: CfrPreferencesRepository
    private lateinit var middleware: CfrToolsPreferencesMiddleware

    @Before
    fun setup() {
        cfrPreferencesRepository = spyk(FakeCfrPreferencesRepository())
        middleware = CfrToolsPreferencesMiddleware(cfrPreferencesRepository)
    }

    @Test
    fun `WHEN the store gets initialized THEN repository is initialized`() = runTestOnMain {
        var initCalled = false
        val repository = object : CfrPreferencesRepository {
            override val cfrPreferenceUpdates: Flow<CfrPreferencesRepository.CfrPreferenceUpdate>
                get() = flowOf(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.InactiveTabs,
                        value = false,
                    ),
                )

            override fun init() {
                initCalled = true
            }

            override fun updateCfrPreference(preferenceUpdate: CfrPreferencesRepository.CfrPreferenceUpdate) {}

            override fun resetLastCfrTimestamp() {}
        }

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = false,
            ),
            middlewares = listOf(
                CfrToolsPreferencesMiddleware(
                    cfrPreferencesRepository = repository,
                    coroutineScope = this,
                ),
            ),
        )

        assertTrue(initCalled)
        assertTrue(store.state.inactiveTabsShown)
    }

    @Test
    fun `WHEN a preference update is mapped THEN a corresponding action is returned`() {
        val preferenceUpdatesFalse = CfrPreferencesRepository.CfrPreference.entries.map {
            CfrPreferencesRepository.CfrPreferenceUpdate(
                preferenceType = it,
                value = false,
            )
        }

        val preferenceUpdatesTrue = CfrPreferencesRepository.CfrPreference.entries.map {
            CfrPreferencesRepository.CfrPreferenceUpdate(
                preferenceType = it,
                value = true,
            )
        }

        val preferenceUpdates = preferenceUpdatesFalse + preferenceUpdatesTrue

        preferenceUpdates.forEach {
            when (it.preferenceType) {
                CfrPreferencesRepository.CfrPreference.HomepageSearchBar -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.HomepageSearchbarCfrLoaded
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.TabAutoCloseBannerCfrLoaded
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.InactiveTabs -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.InactiveTabsCfrLoaded
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.OpenInApp -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.OpenInAppCfrLoaded
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
            }
        }
    }

    @Test
    fun `GIVEN the homepage searchbar CFR should not be shown WHEN the toggle homepage searchbar CFR action is dispatched THEN its preference is set to should be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSearchBarShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.HomepageSearchBarShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.HomepageSearchBar,
                    value = false,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the homepage searchbar CFR should be shown WHEN the toggle homepage searchbar CFR action is dispatched THEN its preference is set to should not be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSearchBarShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.HomepageSearchBarShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.HomepageSearchBar,
                    value = true,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the tab auto close banner CFR should not be shown WHEN the toggle tab auto close banner CFR action is dispatched THEN its preference is set to should be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.TabAutoCloseBannerShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner,
                    value = false,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the tab auto close banner CFR should be shown WHEN the toggle tab auto close banner CFR action is dispatched THEN its preference is set to should not be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.TabAutoCloseBannerShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner,
                    value = true,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the inactive tabs CFR should not be shown WHEN the toggle inactive tabs CFR action is dispatched THEN its preference is set to should be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.InactiveTabsShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.InactiveTabs,
                    value = false,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the inactive tabs CFR should be shown WHEN the toggle inactive tabs CFR action is dispatched THEN its preference is set to should not be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.InactiveTabsShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.InactiveTabs,
                    value = true,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the open in app CFR should not be shown WHEN the toggle open in app CFR action is dispatched THEN its preference is set to should be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.OpenInAppShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.OpenInApp,
                    value = false,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the open in app CFR should be shown WHEN the toggle open in app CFR action is dispatched THEN its preference is set to should not be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.OpenInAppShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.OpenInApp,
                    value = true,
                ),
            )
        }
    }

    @Test
    fun `WHEN the reset lastCfrShownTimeInMillis action is dispatched THEN lastCfrShownTimeInMillis should be set to at least 3 days ago`() {
        var resetCalled = false
        val repository = object : CfrPreferencesRepository {
            override val cfrPreferenceUpdates: Flow<CfrPreferencesRepository.CfrPreferenceUpdate> = flowOf()

            override fun init() {}

            override fun updateCfrPreference(preferenceUpdate: CfrPreferencesRepository.CfrPreferenceUpdate) {}

            override fun resetLastCfrTimestamp() {
                resetCalled = true
            }
        }

        val store = CfrToolsStore(
            middlewares = listOf(
                CfrToolsPreferencesMiddleware(
                    cfrPreferencesRepository = repository,
                ),
            ),
        )

        assertFalse(resetCalled)
        store.dispatch(CfrToolsAction.ResetLastCFRTimestampButtonClicked)
        assertTrue(resetCalled)
    }
}

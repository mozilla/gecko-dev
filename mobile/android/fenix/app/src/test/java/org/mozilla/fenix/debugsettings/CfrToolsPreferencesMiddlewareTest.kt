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
                CfrPreferencesRepository.CfrPreference.HomepageSync -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.HomepageSyncCfrUpdated
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.HomepageNavToolbar -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.HomepageNavToolbarCfrUpdated
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.NavButtons -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.NavButtonsCfrUpdated
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.AddPrivateTabToHome -> {
                    // Note that the new value is not inverted in this CFR because of the different
                    // logic for the pref key
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.AddPrivateTabToHomeCfrUpdated
                    val actualValue = actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.TabAutoCloseBannerCfrUpdated
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.InactiveTabs -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.InactiveTabsCfrUpdated
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
                CfrPreferencesRepository.CfrPreference.OpenInApp -> {
                    val actual = middleware.mapRepoUpdateToStoreAction(it) as CfrToolsAction.OpenInAppCfrUpdated
                    val actualValue = !actual.newValue
                    assertEquals(it.value, actualValue)
                }
            }
        }
    }

    @Test
    fun `GIVEN the homepage sync CFR should not be shown WHEN the toggle homepage sync CFR action is dispatched THEN its preference is set to should be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.HomepageSyncShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.HomepageSync,
                    value = false,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the homepage sync CFR should be shown WHEN the toggle homepage sync CFR action is dispatched THEN its preference is set to should not be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.HomepageSyncShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.HomepageSync,
                    value = true,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR should not be shown WHEN the toggle homepage nav toolbar CFR action is dispatched THEN its preference is set to should be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.HomepageNavToolbarShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.HomepageNavToolbar,
                    value = false,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR should be shown WHEN the toggle homepage nav toolbar CFR action is dispatched THEN its preference is set to should not be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.HomepageNavToolbarShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.HomepageNavToolbar,
                    value = true,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the nav buttons CFR should not be shown WHEN the toggle nav buttons CFR action is dispatched THEN its preference is set to should be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.NavButtonsShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.NavButtons,
                    value = false,
                ),
            )
        }
    }

    @Test
    fun `GIVEN the nav buttons CFR should be shown WHEN the toggle nav buttons CFR action is dispatched THEN its preference is set to should not be shown`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.NavButtonsShownToggled)
        verify {
            cfrPreferencesRepository.updateCfrPreference(
                CfrPreferencesRepository.CfrPreferenceUpdate(
                    preferenceType = CfrPreferencesRepository.CfrPreference.NavButtons,
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

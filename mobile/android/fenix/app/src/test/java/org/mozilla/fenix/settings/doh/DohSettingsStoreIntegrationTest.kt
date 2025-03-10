/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import androidx.navigation.NavController
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DohSettingsStoreIntegrationTest {

    private val settingsProvider = FakeDohSettingsProvider(
        exceptionsList = listOf(
            "example1.com",
            "example2.com",
            "example3.com",
        ),
    )
    private lateinit var navController: NavController
    private lateinit var middleware: DohSettingsMiddleware

    @Before
    fun setUp() {
        navController = mock()

        middleware = DohSettingsMiddleware(
            getNavController = { navController },
            getSettingsProvider = { settingsProvider },
            getHomeActivity = { mock() },
            exitDohSettings = { mock() },
        )
    }

    @Test
    fun `WHEN Init action is dispatched, THEN the state should be updated with DoH Settings from the provider`() {
        val store = middleware.makeStore()

        // When Init is dispatched
        store.dispatch(Init)

        // Then assert that the UI state contains expected state
        val expectedState = DohSettingsState(
            allProtectionLevels = settingsProvider.getProtectionLevels(),
            selectedProtectionLevel = settingsProvider.getSelectedProtectionLevel(),
            providers = settingsProvider.getDefaultProviders(),
            selectedProvider = settingsProvider.getSelectedProvider(),
            exceptionsList = settingsProvider.getExceptions(),
        )

        assertEquals(expectedState, store.state)
    }

    @Test
    fun `WHEN DohOptionSelected action is dispatched with an Off protection level, THEN the protection level should be updated accordingly and selectedProvider should be null`() {
        val store = middleware.makeStore()

        store.dispatch(
            DohSettingsRootAction.DohOptionSelected(
                protectionLevel = ProtectionLevel.Off,
                provider = Provider.BuiltIn(url = "dummy.dummy", name = "Dummy"),
            ),
        )

        // Then verify selectedProtectionLevel is Off in the state
        assertEquals(
            ProtectionLevel.Off,
            store.state.selectedProtectionLevel,
        )

        // Then verify selectedProvider is null in the state
        assertNull(
            store.state.selectedProvider,
        )
    }

    @Test
    fun `WHEN DohOptionSelected action is dispatched with a Default protection level, THEN the protection level should be updated accordingly and selectedProvider should be null`() {
        val store = middleware.makeStore()

        store.dispatch(
            DohSettingsRootAction.DohOptionSelected(
                protectionLevel = ProtectionLevel.Default,
                provider = Provider.BuiltIn(url = "dummy.dummy", name = "Dummy"),
            ),
        )

        // Then verify selectedProtectionLevel is Default in the state
        assertEquals(
            ProtectionLevel.Default,
            store.state.selectedProtectionLevel,
        )

        // Then verify selectedProvider is null in the state
        assertNull(
            store.state.selectedProvider,
        )
    }

    @Test
    fun `WHEN DohOptionSelected action is dispatched with an Increased protection level, THEN the protection level and selectedProvider should be updated accordingly`() {
        val store = middleware.makeStore()

        store.dispatch(
            DohSettingsRootAction.DohOptionSelected(
                protectionLevel = ProtectionLevel.Increased,
                provider = settingsProvider.getBuiltInProvider(),
            ),
        )

        // Then verify selectedProtectionLevel is Increased in the state
        assertEquals(
            ProtectionLevel.Increased,
            store.state.selectedProtectionLevel,
        )

        // Then verify selectedProvider is updated in the state
        assertEquals(
            settingsProvider.getBuiltInProvider(),
            store.state.selectedProvider,
        )
    }

    @Test
    fun `WHEN DohOptionSelected action is dispatched with a Max protection level, THEN the protection level and selectedProvider should be updated accordingly`() {
        val store = middleware.makeStore()

        store.dispatch(
            DohSettingsRootAction.DohOptionSelected(
                protectionLevel = ProtectionLevel.Max,
                provider = settingsProvider.getBuiltInProvider(),
            ),
        )

        // Then verify selectedProtectionLevel is Max in the state
        assertEquals(
            ProtectionLevel.Max,
            store.state.selectedProtectionLevel,
        )

        // Then verify selectedProvider is updated in the state
        assertEquals(
            settingsProvider.getBuiltInProvider(),
            store.state.selectedProvider,
        )
    }

    @Test
    fun `WHEN CustomClicked action is dispatched, THEN the custom provider dialog should be on`() {
        val store = middleware.makeStore()

        store.dispatch(
            DohSettingsRootAction.CustomClicked,
        )

        // Then the custom provider dialog is on
        assertTrue(
            store.state.isCustomProviderDialogOn,
        )
    }

    @Test
    fun `WHEN AddCustomClicked action is received with valid url, THEN a valid url should be dispatched, the custom provider dialog is off, and the custom url is updated`() {
        val store = middleware.makeStore()

        // A valid https url
        val newUrl = "https://foo.bar"

        // When we dispatch the add custom click action
        store.dispatch(
            DohSettingsRootAction.DohCustomProviderDialogAction.AddCustomClicked(
                customProvider = settingsProvider.getCustomProvider(),
                url = newUrl,
            ),
        )

        // Then verify CustomProviderErrorState is "Valid"
        assertEquals(
            CustomProviderErrorState.Valid,
            store.state.customProviderErrorState,
        )

        // Then the dialog should be off
        assertFalse(
            store.state.isCustomProviderDialogOn,
        )

        // The custom provider url in the state must be updated
        assertEquals(
            newUrl,
            store.state.selectedProvider?.url,
        )
    }

    @Test
    fun `WHEN AddCustomClicked action is received with non-https url, THEN customProviderErrorState should be set to NonHttps and the custom provider dialog is still on`() {
        val store = middleware.makeStore()

        // When we dispatch the add custom click action with a non-https url
        store.dispatch(
            DohSettingsRootAction.DohCustomProviderDialogAction.AddCustomClicked(
                customProvider = settingsProvider.getCustomProvider(),
                url = "http://foo.bar",
            ),
        )

        // Then verify the CustomProviderErrorState is a "NonHttps" error
        assertEquals(
            CustomProviderErrorState.NonHttps,
            store.state.customProviderErrorState,
        )

        // Then the dialog should still be on
        assertTrue(
            store.state.isCustomProviderDialogOn,
        )
    }

    @Test
    fun `WHEN RemoveClicked action is received with invalid url, THEN customProviderErrorState should be set to Invalid and the custom provider dialog is still on`() {
        val store = middleware.makeStore()

        // When we dispatch the add custom click action with a url with an invalid character
        store.dispatch(
            DohSettingsRootAction.DohCustomProviderDialogAction.AddCustomClicked(
                customProvider = settingsProvider.getCustomProvider(),
                url = "https://@.bar",
            ),
        )

        // Then verify the CustomProviderErrorState is an "Invalid" error
        assertEquals(
            CustomProviderErrorState.Invalid,
            store.state.customProviderErrorState,
        )

        // Then the dialog should still be on
        assertTrue(
            store.state.isCustomProviderDialogOn,
        )
    }

    @Test
    fun `WHEN RemoveClicked is dispatched with an exception site, THEN that corresponding exception should be removed`() {
        val store = middleware.makeStore()

        // Exception to remove
        val exceptionUrl = settingsProvider.getExceptions().first()

        // When RemoveClicked is dispatched with an exception site
        store.dispatch(
            ExceptionsAction.RemoveClicked(
                url = exceptionUrl,
            ),
        )

        // Then verify the exceptionUrl is removed from the exceptions
        assertFalse(
            exceptionUrl in settingsProvider.getExceptions(),
        )
    }

    @Test
    fun `WHEN RemoveClicked is dispatched with an exception site but it does not exist, THEN nothing should happen`() {
        val store = middleware.makeStore()

        // An exception that does not exist
        val exceptionUrl = "foo.bar"
        val prevExceptionsList = settingsProvider.getExceptions().toList()
        assertFalse(
            exceptionUrl in prevExceptionsList,
        )

        // When RemoveClicked is dispatched with an exception site but it does not exist
        store.dispatch(
            ExceptionsAction.RemoveClicked(
                url = exceptionUrl,
            ),
        )

        // Then nothing should happen
        assertEquals(
            settingsProvider.getExceptions(),
            prevExceptionsList,
        )
    }

    @Test
    fun `WHEN RemoveAllClicked is dispatched, THEN exceptions should become empty`() {
        val store = middleware.makeStore()

        store.dispatch(
            ExceptionsAction.RemoveAllClicked,
        )

        // Then exceptions should become empty
        assertTrue(
            settingsProvider.getExceptions().isEmpty(),
        )
    }

    @Test
    fun `WHEN RemoveAllClicked is dispatched twice (emptied list is emptied again), THEN exceptions should stay empty`() {
        val store = middleware.makeStore()

        store.dispatch(
            ExceptionsAction.RemoveAllClicked,
        )
        assertTrue(
            settingsProvider.getExceptions().isEmpty(),
        )
        store.dispatch(
            ExceptionsAction.RemoveAllClicked,
        )

        // Then exceptions should become empty
        assertTrue(
            settingsProvider.getExceptions().isEmpty(),
        )
    }

    @Test
    fun `WHEN SaveClicked is dispatched with an invalid url, THEN isUserExceptionValid should be set to false`() {
        val store = middleware.makeStore()

        // A url with an invalid character
        store.dispatch(
            ExceptionsAction.SaveClicked(
                url = "@e.com",
            ),
        )

        assertTrue(
            store.state.isUserExceptionValid,
        )
    }

    @Test
    fun `WHEN SaveClicked is dispatched with a valid exception site that does not exist, THEN it should be added to the exceptions`() {
        val store = middleware.makeStore()

        // An exception that does not exist
        val exceptionUrl = "foo.bar"
        val prevExceptionsList = settingsProvider.getExceptions().toList()
        assertFalse(
            exceptionUrl in prevExceptionsList,
        )

        // When SaveClicked is dispatched with an exception site that does not exist
        store.dispatch(
            ExceptionsAction.SaveClicked(
                url = exceptionUrl,
            ),
        )

        // exceptionUrl is appended to the exceptions
        assertEquals(
            settingsProvider.getExceptions(),
            prevExceptionsList + exceptionUrl,
        )
    }

    @Test
    fun `WHEN SaveClicked is dispatched with a valid exception site that already exists, THEN nothing should happen to the exceptions`() {
        val store = middleware.makeStore()

        val prevExceptionsList = settingsProvider.getExceptions().toList()

        // When SaveClicked is dispatched with an exception site that already exists
        store.dispatch(
            ExceptionsAction.SaveClicked(
                url = prevExceptionsList.first(),
            ),
        )

        // Then nothing should happen to the exceptions
        assertEquals(
            settingsProvider.getExceptions(),
            prevExceptionsList,
        )
    }

    private fun DohSettingsMiddleware.makeStore(
        initialState: DohSettingsState = DohSettingsState(),
    ) = DohSettingsStore(
        initialState = initialState,
        middleware = listOf(this),
    ).also {
        it.waitUntilIdle()
    }
}

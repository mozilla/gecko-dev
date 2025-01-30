package org.mozilla.focus.state

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class AppReducerTest {

    @Test
    fun `test showFirstRun() should return the correct state when in other screen than FirstRun`() {
        val initialState = spy(AppState(screen = Screen.Home))
        val expectedState = showFirstRun(initialState)

        verify(initialState).copy(screen = Screen.FirstRun)
        assertEquals(Screen.FirstRun, expectedState.screen)
    }

    @Test
    fun `test showFirstRun() should return the correct state when already in FirstRun`() {
        val initialState = spy(AppState(screen = Screen.FirstRun))
        val expectedState = showFirstRun(initialState)

        verify(initialState, never()).copy(screen = Screen.FirstRun)
        assertEquals(Screen.FirstRun, expectedState.screen)
    }

    @Test
    fun `test showOnBoardingSecondScreen() should return the correct state when in other screen than OnboardingSecondScreen`() {
        val initialState = spy(AppState(screen = Screen.Home))
        val expectedState = showOnBoardingSecondScreen(initialState)

        verify(initialState).copy(screen = Screen.OnboardingSecondScreen)
        assertEquals(Screen.OnboardingSecondScreen, expectedState.screen)
    }

    @Test
    fun `test showOnBoardingSecondScreen() should return the correct state when already in OnboardingSecondScreen`() {
        val initialState = spy(AppState(screen = Screen.OnboardingSecondScreen))
        val expectedState = showOnBoardingSecondScreen(initialState)

        verify(initialState, never()).copy(screen = Screen.OnboardingSecondScreen)
        assertEquals(Screen.OnboardingSecondScreen, expectedState.screen)
    }

    @Test
    fun `test showHomeScreen() should return the correct state when in other screen than Home`() {
        val initialState = spy(AppState(screen = Screen.Browser(tabId = "tab1", showTabs = true)))
        val expectedState = showHomeScreen(initialState)

        verify(initialState).copy(screen = Screen.Home)
        assertEquals(Screen.Home, expectedState.screen)
    }

    @Test
    fun `test showHomeScreen() should return the correct state when already in Home`() {
        val initialState = spy(AppState(screen = Screen.Home))
        val expectedState = showHomeScreen(initialState)

        verify(initialState, never()).copy(screen = Screen.Home)
        assertEquals(Screen.Home, expectedState.screen)
    }

    @Test
    fun `test lock() should return the correct state when in other screen than Locked`() {
        val initialState = spy(AppState(screen = Screen.Home))
        val action = AppAction.Lock()
        val expectedState = lock(initialState, action)

        verify(initialState).copy(screen = Screen.Locked())
        assertEquals(Screen.Locked(bundle = null), expectedState.screen)
    }

    @Test
    fun `test lock() should return the correct state when already in Locked`() {
        val initialState = spy(AppState(screen = Screen.Locked()))
        val action = AppAction.Lock()
        val expectedState = lock(initialState, action)

        verify(initialState, never()).copy(screen = Screen.Locked())
        assertEquals(Screen.Locked(), expectedState.screen)
    }
}

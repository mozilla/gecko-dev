package org.mozilla.fenix.onboarding.store

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.onboarding.view.ToolbarOptionType

class OnboardingToolbarStoreTest {

    @Test
    fun `WHEN init action is dispatched THEN toolbar state selected is toolbar top`() {
        val store = OnboardingToolbarStore()

        store.dispatch(OnboardingToolbarAction.Init).joinBlocking()
        assertEquals(ToolbarOptionType.TOOLBAR_TOP, store.state.selected)
    }

    @Test
    fun `WHEN update selected action is dispatched THEN the toolbar state selected value is updated`() {
        val store = OnboardingToolbarStore()

        store.dispatch(OnboardingToolbarAction.UpdateSelected(ToolbarOptionType.TOOLBAR_BOTTOM))
            .joinBlocking()
        assertEquals(ToolbarOptionType.TOOLBAR_BOTTOM, store.state.selected)

        store.dispatch(OnboardingToolbarAction.UpdateSelected(ToolbarOptionType.TOOLBAR_TOP))
            .joinBlocking()
        assertEquals(ToolbarOptionType.TOOLBAR_TOP, store.state.selected)
    }
}

package org.mozilla.fenix.onboarding.store

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.onboarding.view.OnboardingAddOn
import org.mozilla.fenix.onboarding.view.ToolbarOptionType

class OnboardingStoreTest {

    @Test
    fun `WHEN init action is dispatched THEN state is updated as expected`() {
        val store = OnboardingStore()
        val addOns: List<OnboardingAddOn> = emptyList()

        store.dispatch(OnboardingAction.Init(addOns)).joinBlocking()

        val expected = OnboardingState(
            addOns = addOns,
            addOnInstallationInProcess = false,
            toolbarOptionSelected = ToolbarOptionType.TOOLBAR_TOP,
        )
        assertEquals(expected, store.state)
    }

    @Test
    fun `WHEN installation process action is dispatched THEN addOns and installationInProcess state is updated`() {
        val store = OnboardingStore()
        val addOns: List<OnboardingAddOn> = listOf(
            OnboardingAddOn(
                id = "add-on-1",
                iconRes = R.drawable.ic_extensions_onboarding,
                name = "test add-on 1",
                description = "test 1 add-on description",
                averageRating = "4.5",
                reviewCount = "134",
                installUrl = "url1",
                status = OnboardingAddonStatus.NOT_INSTALLED,
            ),
            OnboardingAddOn(
                id = "add-on-2",
                iconRes = R.drawable.ic_extensions_onboarding,
                name = "test add-on 2",
                description = "test 2 add-on description",
                averageRating = "4.5",
                reviewCount = "1,234",
                installUrl = "url2",
                status = OnboardingAddonStatus.NOT_INSTALLED,
            ),
        )

        store.dispatch(OnboardingAction.Init(addOns)).joinBlocking()

        assertEquals(
            addOns,
            store.state.addOns,
        )
        assertFalse(store.state.addOnInstallationInProcess)

        store.dispatch(
            OnboardingAction.OnboardingAddOnsAction.UpdateStatus(
                addOnId = "add-on-1",
                status = OnboardingAddonStatus.INSTALLED,
            ),
        ).joinBlocking()

        assertFalse(store.state.addOnInstallationInProcess)

        assertEquals(
            listOf(
                OnboardingAddOn(
                    id = "add-on-1",
                    iconRes = R.drawable.ic_extensions_onboarding,
                    name = "test add-on 1",
                    description = "test 1 add-on description",
                    averageRating = "4.5",
                    reviewCount = "134",
                    installUrl = "url1",
                    status = OnboardingAddonStatus.INSTALLED,
                ),
                OnboardingAddOn(
                    id = "add-on-2",
                    iconRes = R.drawable.ic_extensions_onboarding,
                    name = "test add-on 2",
                    description = "test 2 add-on description",
                    averageRating = "4.5",
                    reviewCount = "1,234",
                    installUrl = "url2",
                    status = OnboardingAddonStatus.NOT_INSTALLED,
                ),
            ),
            store.state.addOns,
        )
    }

    @Test
    fun `WHEN update selected toolbar action is dispatched THEN the toolbar state selected value is updated`() {
        val store = OnboardingStore()

        store.dispatch(OnboardingAction.OnboardingToolbarAction.UpdateSelected(ToolbarOptionType.TOOLBAR_BOTTOM))
            .joinBlocking()
        assertEquals(ToolbarOptionType.TOOLBAR_BOTTOM, store.state.toolbarOptionSelected)

        store.dispatch(OnboardingAction.OnboardingToolbarAction.UpdateSelected(ToolbarOptionType.TOOLBAR_TOP))
            .joinBlocking()
        assertEquals(ToolbarOptionType.TOOLBAR_TOP, store.state.toolbarOptionSelected)
    }
}

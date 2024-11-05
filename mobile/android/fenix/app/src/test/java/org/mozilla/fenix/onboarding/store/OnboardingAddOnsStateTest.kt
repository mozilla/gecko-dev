/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.onboarding.view.OnboardingAddOn

class OnboardingAddOnsStateTest {

    @Test
    fun `WHEN Init action is dispatched THEN addOns state is updated`() {
        val store = OnboardingAddOnsStore()
        val addOns: List<OnboardingAddOn> = emptyList()

        store.dispatch(OnboardingAddOnsAction.Init(addOns)).joinBlocking()

        assertEquals(
            addOns,
            store.state.addOns,
        )
    }

    @Test
    fun `WHEN INSTALLING UpdateStatus action is dispatched THEN addOns and installationInProcess state is updated`() {
        val store = OnboardingAddOnsStore()
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

        store.dispatch(OnboardingAddOnsAction.Init(addOns)).joinBlocking()

        assertEquals(
            addOns,
            store.state.addOns,
        )

        assertFalse(store.state.installationInProcess)

        store.dispatch(
            OnboardingAddOnsAction.UpdateStatus(
                addOnId = "add-on-1",
                status = OnboardingAddonStatus.INSTALLED,
            ),
        ).joinBlocking()

        assertFalse(store.state.installationInProcess)

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
}

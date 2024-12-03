/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.nimbus.ExtraCardData
import org.mozilla.fenix.nimbus.OnboardingCardData
import org.mozilla.fenix.nimbus.OnboardingCardType
import org.mozilla.fenix.nimbus.TermsOfServiceData

class DataChoicesFragmentTest {

    private val fragment = DataChoicesFragment()
    private val hasValidExtraData: (OnboardingCardData) -> Boolean = { true }
    private val noValidExtraData: (OnboardingCardData) -> Boolean = { false }

    @Test
    fun `GIVEN no cards WHEN shouldShowMarketingTelemetryPreference called THEN returns false`() {
        val result = fragment.shouldShowMarketingTelemetryPreference(listOf())

        assertFalse(result)
    }

    @Test
    fun `GIVEN cards contains valid type, no valid extra data WHEN shouldShowMarketingTelemetryPreference THEN returns false`() {
        val cardsWithAllCardTypes =
            OnboardingCardType.entries.map { OnboardingCardData(cardType = it) }

        val result = fragment.shouldShowMarketingTelemetryPreference(
            cards = cardsWithAllCardTypes,
            hasValidTermsOfServiceData = noValidExtraData,
        )

        assertFalse(result)
    }

    @Test
    fun `GIVEN cards contains valid type, has valid extra data WHEN shouldShowMarketingTelemetryPreference THEN returns true`() {
        val cardsWithAllCardTypes =
            OnboardingCardType.entries.map { OnboardingCardData(cardType = it) }

        val result = fragment.shouldShowMarketingTelemetryPreference(
            cards = cardsWithAllCardTypes,
            hasValidTermsOfServiceData = hasValidExtraData,
        )

        assertTrue(result)
    }

    @Test
    fun `GIVEN cards does not contain valid type, no valid extra data WHEN shouldShowMarketingTelemetryPreference THEN returns false`() {
        val cardsWithAllCardTypesNoTos = OnboardingCardType.entries
            .filter { it != OnboardingCardType.TERMS_OF_SERVICE }
            .map { OnboardingCardData(cardType = it) }

        val result = fragment.shouldShowMarketingTelemetryPreference(
            cards = cardsWithAllCardTypesNoTos,
            hasValidTermsOfServiceData = noValidExtraData,
        )

        assertFalse(result)
    }

    @Test
    fun `GIVEN cards does not contain valid type, has valid extra data WHEN shouldShowMarketingTelemetryPreference THEN returns false`() {
        val cardsWithAllCardTypesNoTos = OnboardingCardType.entries
            .filter { it != OnboardingCardType.TERMS_OF_SERVICE }
            .map { OnboardingCardData(cardType = it) }

        val result = fragment.shouldShowMarketingTelemetryPreference(
            cards = cardsWithAllCardTypesNoTos,
            hasValidTermsOfServiceData = hasValidExtraData,
        )

        assertFalse(result)
    }

    @Test
    fun `GIVEN extra data is null WHEN hasValidTermsOfServiceData THEN returns false`() {
        assertFalse(OnboardingCardData().hasValidTermsOfServiceData())
    }

    @Test
    fun `GIVEN extra data does not have tos data WHEN hasValidTermsOfServiceData THEN returns false`() {
        val card = OnboardingCardData(extraData = ExtraCardData())
        assertFalse(card.hasValidTermsOfServiceData())
    }

    @Test
    fun `GIVEN extra data has tos data WHEN hasValidTermsOfServiceData THEN returns true`() {
        val card =
            OnboardingCardData(extraData = ExtraCardData(termOfServiceData = TermsOfServiceData()))
        assertTrue(card.hasValidTermsOfServiceData())
    }
}

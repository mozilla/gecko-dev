/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.addresses

import mozilla.components.concept.storage.Address
import mozilla.components.concept.storage.CreditCard
import mozilla.components.concept.storage.CreditCardCrypto
import mozilla.components.concept.storage.CreditCardsAddressesStorage
import mozilla.components.concept.storage.NewCreditCardFields
import mozilla.components.concept.storage.UpdatableAddressFields
import mozilla.components.concept.storage.UpdatableCreditCardFields
import java.util.UUID

/**
 * Some randomly generated fake addresses that match the expected locale.
 */
internal fun String.generateFakeAddressForLangTag(): UpdatableAddressFields = when (this) {
    "en-CA" -> UpdatableAddressFields(
        name = "Tim Horton",
        organization = "",
        streetAddress = "Township Road 531",
        addressLevel3 = "",
        addressLevel2 = "Hamilton",
        addressLevel1 = "Ontario",
        postalCode = " L8R 2L2",
        country = "Canada",
        tel = " (905) 555-5555",
        email = "englishcanada@gmail.com",
    )
    "fr-CA" -> UpdatableAddressFields(
        name = "Jean Horton",
        organization = "",
        streetAddress = "73 Rue Prince Arthur Est",
        addressLevel3 = "",
        addressLevel2 = "Montréal",
        addressLevel1 = "Quebec",
        postalCode = "H2X 2Y3",
        country = "Canada",
        tel = "(514) 555-5555",
        email = "frenchcanada@gmail.com",
    )
    "de-DE" -> UpdatableAddressFields(
        name = "Max Mustermann",
        organization = "",
        streetAddress = "Flughafenstrasse 47",
        addressLevel3 = "",
        addressLevel2 = "Hamburg",
        addressLevel1 = "Hamburg",
        postalCode = "22415",
        country = "Germany",
        tel = "040 555020",
        email = "germangermany@gmail.com",
    )
    "fr-FR" -> UpdatableAddressFields(
        name = "Jean Dupont",
        organization = "",
        streetAddress = "17 Rue Vergniaud",
        addressLevel3 = "",
        addressLevel2 = "Paris",
        addressLevel1 = "Paris",
        postalCode = "75013",
        country = "France",
        tel = " 01 45 55 55 55",
        email = "frenchfrance@gmail.com",
    )
    else -> UpdatableAddressFields(
        name = "John Doe",
        organization = "",
        streetAddress = " 530 E McDowell Rd",
        addressLevel3 = "",
        addressLevel2 = "Phoenix",
        addressLevel1 = "Arizona",
        postalCode = "85003",
        country = "United States",
        tel = " (602) 555-5555",
        email = "englishunitedstates@gmail.com",
    )
}

internal class FakeCreditCardsAddressesStorage : CreditCardsAddressesStorage {
    val addresses = getAllPossibleLocaleLangTags().map {
        it.generateFakeAddressForLangTag().toAddress()
    }.toMutableList()

    override suspend fun addCreditCard(creditCardFields: NewCreditCardFields): CreditCard {
        throw UnsupportedOperationException()
    }

    override suspend fun updateCreditCard(
        guid: String,
        creditCardFields: UpdatableCreditCardFields,
    ) {
        throw UnsupportedOperationException()
    }

    override suspend fun getCreditCard(guid: String): CreditCard? {
        throw UnsupportedOperationException()
    }

    override suspend fun getAllCreditCards(): List<CreditCard> {
        throw UnsupportedOperationException()
    }

    override suspend fun deleteCreditCard(guid: String): Boolean {
        throw UnsupportedOperationException()
    }

    override suspend fun touchCreditCard(guid: String) {
        throw UnsupportedOperationException()
    }

    override suspend fun addAddress(addressFields: UpdatableAddressFields): Address =
        addressFields.toAddress().also {
            addresses.add(it)
        }

    override suspend fun getAddress(guid: String): Address? {
        return addresses.find { it.guid == guid }
    }

    override suspend fun getAllAddresses(): List<Address> {
        return addresses
    }

    override suspend fun updateAddress(guid: String, address: UpdatableAddressFields) {
        throw UnsupportedOperationException()
    }

    override suspend fun deleteAddress(guid: String): Boolean {
        addresses.find { it.guid == guid }?.let {
            addresses.remove(it)
        }
        return true
    }

    override suspend fun touchAddress(guid: String) {
        throw UnsupportedOperationException()
    }

    override fun getCreditCardCrypto(): CreditCardCrypto {
        throw UnsupportedOperationException()
    }

    override suspend fun scrubEncryptedData() {
        throw UnsupportedOperationException()
    }

    private fun UpdatableAddressFields.toAddress() =
        Address(
            guid = UUID.randomUUID().toString(),
            organization = organization,
            name = name,
            streetAddress = streetAddress,
            addressLevel1 = addressLevel1,
            addressLevel2 = addressLevel2,
            addressLevel3 = addressLevel3,
            postalCode = postalCode,
            country = country,
            tel = tel,
            email = email,
        )

    companion object {
        fun getAllPossibleLocaleLangTags(): List<String> = listOf(
            "en-US",
            "en-CA",
            "fr-CA",
        ) + DebugLocale.entries.map { it.langTag }
    }
}

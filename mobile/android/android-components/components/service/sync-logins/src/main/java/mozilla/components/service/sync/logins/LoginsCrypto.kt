/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.sync.logins

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import mozilla.appservices.logins.KeyRegenerationEventReason
import mozilla.appservices.logins.checkCanary
import mozilla.appservices.logins.createCanary
import mozilla.appservices.logins.recordKeyRegenerationEvent
import mozilla.components.concept.storage.KeyGenerationReason
import mozilla.components.concept.storage.KeyManager
import mozilla.components.lib.dataprotect.SecureAbove22Preferences

/**
 * A class that knows how to encrypt & decrypt strings, backed by application-services' logins lib.
 * Used for protecting usernames/passwords at rest.
 *
 * This class manages creation and storage of the encryption key.
 * It also keeps track of abnormal events, such as managed key going missing or getting corrupted.
 *
 * @param context [Context] used for obtaining [SharedPreferences] for managing internal prefs.
 * @param securePrefs A [SecureAbove22Preferences] instance used for storing the managed key.
 */
class LoginsCrypto(
    private val context: Context,
    private val securePrefs: SecureAbove22Preferences,
    private val storage: SyncableLoginsStorage,
) : KeyManager() {
    private val plaintextPrefs by lazy { context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE) }

    override suspend fun recoverFromKeyLoss(reason: KeyGenerationReason.RecoveryNeeded) {
        val telemetryEventReason = when (reason) {
            is KeyGenerationReason.RecoveryNeeded.Lost -> KeyRegenerationEventReason.Lost
            is KeyGenerationReason.RecoveryNeeded.Corrupt -> KeyRegenerationEventReason.Corrupt
            is KeyGenerationReason.RecoveryNeeded.AbnormalState -> KeyRegenerationEventReason.Other
        }
        recordKeyRegenerationEvent(telemetryEventReason)
        storage.getStorage().wipeLocal()
    }

    override fun getStoredCanary(): String? {
        return plaintextPrefs.getString(CANARY_PHRASE_CIPHERTEXT_KEY, null)
    }

    override fun getStoredKey(): String? {
        return securePrefs.getString(LOGINS_KEY)
    }

    override fun storeKeyAndCanary(key: String) {
        // To consider: should this be a non-destructive operation, just in case?
        // e.g. if we thought we lost the key, but actually did not, that would let us recover data later on.
        // otherwise, if we mess up and override a perfectly good key, the data is gone for good.
        securePrefs.putString(LOGINS_KEY, key)
        // To detect key corruption or absence, use the newly generated key to encrypt a known string.
        // See isKeyValid below.
        plaintextPrefs.edit {
            putString(CANARY_PHRASE_CIPHERTEXT_KEY, createCanary(CANARY_PHRASE_PLAINTEXT, key))
        }
    }

    override fun createKey(): String {
        return mozilla.appservices.logins.createKey()
    }

    override fun isKeyRecoveryNeeded(rawKey: String, canary: String): KeyGenerationReason.RecoveryNeeded? {
        return try {
            if (checkCanary(canary, CANARY_PHRASE_PLAINTEXT, rawKey)) {
                null
            } else {
                // A bad key should trigger a IncorrectKey, but check this branch just in case.
                KeyGenerationReason.RecoveryNeeded.Corrupt
            }
        } catch (e: InvalidKey) {
            KeyGenerationReason.RecoveryNeeded.Corrupt
        }
    }

    companion object {
        const val PREFS_NAME = "loginsCrypto"
        const val LOGINS_KEY = "loginsKey"
        const val CANARY_PHRASE_CIPHERTEXT_KEY = "canaryPhrase"
        const val CANARY_PHRASE_PLAINTEXT = "a string for checking validity of the key"
    }
}

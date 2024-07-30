/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa

import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.concept.sync.DeviceCommandOutgoing

/**
 * High-level exception class for the exceptions thrown in the Rust library.
 */
typealias FxaException = mozilla.appservices.fxaclient.FxaException

/**
 * Thrown on a network error.
 */
typealias FxaNetworkException = mozilla.appservices.fxaclient.FxaException.Network

/**
 * Thrown when the Rust library hits an assertion or panic (this is always a bug).
 */
typealias FxaPanicException = mozilla.appservices.fxaclient.FxaException.Panic

/**
 * Thrown when the operation requires additional authorization.
 */
typealias FxaUnauthorizedException = mozilla.appservices.fxaclient.FxaException.Authentication

/**
 * Thrown when we try opening paring link from a Firefox configured to use a different content server
 */
typealias FxaOriginMismatchException = mozilla.appservices.fxaclient.FxaException.OriginMismatch

/**
 * Thrown if the application attempts to complete an OAuth flow when no OAuth flow has been
 * initiated. This may indicate a user who navigated directly to the OAuth `redirect_uri` for the
 * application.
 */
typealias FxaNoExistingAuthFlow = mozilla.appservices.fxaclient.FxaException.NoExistingAuthFlow

/**
 * Thrown when a scoped key was missing in the server response when requesting the OLD_SYNC scope.
 */
typealias FxaSyncScopedKeyMissingException =
    mozilla.appservices.fxaclient.FxaException.SyncScopedKeyMissingInServerResponse

/**
 * Thrown when the Rust library hits an unexpected error that isn't a panic.
 * This may indicate library misuse, network errors, etc.
 */
typealias FxaUnspecifiedException = mozilla.appservices.fxaclient.FxaException.Other

/**
 * @return 'true' if this exception should be re-thrown and eventually crash the app.
 */
fun FxaException.shouldPropagate(): Boolean {
    return when (this) {
        // Throw on panics
        is FxaPanicException -> true
        // Don't throw for recoverable errors.
        is FxaNetworkException,
        is FxaUnauthorizedException,
        is FxaUnspecifiedException,
        is FxaOriginMismatchException,
        is FxaNoExistingAuthFlow,
        -> false
        // Throw on newly encountered exceptions.
        // If they're actually recoverable and you see them in crash reports, update this check.
        else -> true
    }
}

/**
 * Exceptions related to the account manager.
 */
sealed class AccountManagerException(message: String) : Exception(message) {
    /**
     * Hit a circuit-breaker during auth recovery flow.
     * @param operation An operation which triggered an auth recovery flow that hit a circuit breaker.
     */
    class AuthRecoveryCircuitBreakerException(operation: String) : AccountManagerException(
        "Auth recovery circuit breaker triggered by: $operation",
    )

    /**
     * Unexpectedly encountered an access token without a key.
     * @param operation An operation which triggered this state.
     */
    class MissingKeyFromSyncScopedAccessToken(operation: String) : AccountManagerException(
        "Encountered an access token without a key: $operation",
    )

    /**
     * Failure when running side effects to complete the authentication process.
     */
    class AuthenticationSideEffectsFailed : AccountManagerException(
        "Failure when running side effects to complete authentication",
    )
}

/** A recoverable error encountered when sending a command to another device. */
sealed class SendCommandException : Exception {
    constructor(message: String) : super(message)
    constructor(cause: Throwable) : super(cause)

    /**
     * An exception thrown when one or more
     * [DeviceCommandOutgoing.CloseTab.urls] couldn't be sent.
     *
     * The caller should back off and retry sending the [urls] in
     * a new [DeviceCommandOutgoing.CloseTab] command.
     */
    class TabsNotClosed(val urls: List<String>) :
        SendCommandException("Couldn't send all URLs in close tabs command")

    /**
     * An exception used for [CrashReporting] when a command couldn't be sent
     * for any other reason.
     */
    class Other(cause: Throwable) : SendCommandException(cause)
}

/**
 * Thrown if we saw a keyed access token without a key (e.g. obtained for SCOPE_SYNC).
 */
internal class AccessTokenUnexpectedlyWithoutKey : Exception()

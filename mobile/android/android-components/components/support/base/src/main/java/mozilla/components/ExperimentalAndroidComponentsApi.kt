/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components

/**
 * Items annotated with [ExperimentalAndroidComponentsApi] are subject to change or else
 * inherently more dangerous or unstable.
 *
 * <p>This annotation covers:
 *
 * <ul>
 *   <li>API signatures that may change without notice or deprecation period.
 *   <li>API behavior and interactions that may change without notice.
 *   <li>Features that are under heavy development and are considered unstable.
 *   <li>Features that have inherent risk or potentially destabilize other parts of the app.
 * </ul>
 *
 *  Only use an API annotated with this if you fully understand the API and accept the risks.
 */
@MustBeDocumented
@Retention(value = AnnotationRetention.BINARY)
@RequiresOptIn(
    level = RequiresOptIn.Level.ERROR,
    message = "This API is experimental and should only be used with caution. " +
        "Annotate with @OptIn to accept the risk.",
)
@Target(AnnotationTarget.CLASS, AnnotationTarget.FUNCTION, AnnotationTarget.CONSTRUCTOR)
annotation class ExperimentalAndroidComponentsApi

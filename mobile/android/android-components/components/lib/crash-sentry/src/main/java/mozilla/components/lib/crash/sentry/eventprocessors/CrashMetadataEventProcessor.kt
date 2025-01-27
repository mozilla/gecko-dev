/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.sentry.eventprocessors

import io.sentry.EventProcessor
import io.sentry.Hint
import io.sentry.SentryEvent
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter

/**
 * This [EventProcessor] will retain a reference to the [Crash] that has been most recently reported,
 * allowing us to attach metadata from it to a [SentryEvent] as it is being processed. This allows us to,
 * for example, add runtime information from [Crash.runtimeTags].
 */
class CrashMetadataEventProcessor : EventProcessor {
    internal var crashToProcess: Crash? = null

    override fun process(event: SentryEvent, hint: Hint): SentryEvent {
        crashToProcess?.let {
            it.runtimeTags[CrashReporter.RELEASE_RUNTIME_TAG]?.let { crashRelease ->
                event.release = crashRelease
            }
        }
        return event.also {
            crashToProcess = null
        }
    }
}

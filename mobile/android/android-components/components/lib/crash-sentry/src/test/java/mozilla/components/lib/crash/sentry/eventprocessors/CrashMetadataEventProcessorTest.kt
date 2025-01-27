/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.sentry.eventprocessors

import io.sentry.SentryEvent
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test

class CrashMetadataEventProcessorTest {
    @Test
    fun `GIVEN no crash attached to processor WHEN processed THEN event unchanged`() {
        val event = SentryEvent()
        val processor = CrashMetadataEventProcessor()

        val result = processor.process(event, mock())

        assertEquals(event, result)
    }

    @Test
    fun `GIVEN crash with no release metadata attached to processor WHEN processed THEN event unchanged and crash unattached`() {
        val event = SentryEvent()
        val processor = CrashMetadataEventProcessor()

        processor.crashToProcess = Crash.NativeCodeCrash(
            timestamp = System.currentTimeMillis(),
            minidumpPath = null,
            extrasPath = null,
            processType = null,
            remoteType = null,
            breadcrumbs = arrayListOf(),
        )
        val result = processor.process(event, mock())

        assertEquals(event, result)
    }

    @Test
    fun `GIVEN a crash with release metadata is currently being reported WHEN processed THEN release metadata is attached and crash unattached`() {
        val event = SentryEvent()
        event.release = "a fake release"
        val actualRelease = "136.0.1"
        val processor = CrashMetadataEventProcessor()

        processor.crashToProcess = Crash.NativeCodeCrash(
            timestamp = System.currentTimeMillis(),
            minidumpPath = null,
            extrasPath = null,
            processType = null,
            remoteType = null,
            breadcrumbs = arrayListOf(),
            runtimeTags = mapOf(CrashReporter.RELEASE_RUNTIME_TAG to actualRelease),
        )
        val result = processor.process(event, mock())

        event.release = actualRelease
        assertEquals(event, result)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import mozilla.components.support.base.log.logger.Logger

/**
 * Invoke the native minidump analyzer to get stack trace information for native crashes.
 */
internal class MinidumpAnalyzer {
    companion object {
        private val logger = Logger("crash/MinidumpAnalyzer")

        /**
         * Load the minidump analyzer. If the native library is not found, returns null.
         */
        fun load(): MinidumpAnalyzer? {
            try {
                System.loadLibrary("minidump_analyzer")
                logger.debug("loaded minidump_analyzer native library")
            } catch (e: UnsatisfiedLinkError) {
                logger.info("failed to load minidump_analyzer native library: $e")
                return null
            }

            return MinidumpAnalyzer()
        }
    }

    /**
     * Run the minidump analyzer. If an error occurs, it is logged but nothing else is done (because
     * the minidump analyzer is best-effort to augment crash information.
     */
    fun run(minidumpPath: String, extrasPath: String, allThreads: Boolean) {
        analyze(minidumpPath, extrasPath, allThreads)?.let {
            logger.error("error running minidump analyzer: $it")
        }
    }

    private external fun analyze(minidumpPath: String, extrasPath: String, allThreads: Boolean): String?
}

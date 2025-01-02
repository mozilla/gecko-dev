/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ext

import androidx.annotation.WorkerThread
import mozilla.components.concept.fetch.Client
import mozilla.components.concept.fetch.Request
import mozilla.components.concept.fetch.Response
import mozilla.components.concept.fetch.isSuccess
import mozilla.components.support.base.log.logger.Logger
import java.io.IOException

/**
 * Fetches a resource from the network as described by the [Request] object, and returns
 * response body or null if the request could not executed.
 * This call is synchronous.
 *
 * @param request The request to be executed by this [Client].
 * @param logger Optional [Logger] to use for logging.
 * @return the [String] contained within the response body for the given [request] or null on error.
 */
@WorkerThread // synchronous network call.
fun Client.fetchBodyOrNull(request: Request, logger: Logger? = null): String? {
    val response: Response? = try {
        fetch(request)
    } catch (e: IOException) {
        logger?.debug("network error", e)
        null
    }

    return response?.use { if (response.isSuccess) response.body.string(Charsets.UTF_8) else null }
}

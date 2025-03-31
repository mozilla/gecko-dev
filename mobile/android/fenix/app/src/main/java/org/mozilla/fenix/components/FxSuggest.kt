/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import mozilla.components.feature.fxsuggest.FxSuggestIngestionScheduler
import mozilla.components.feature.fxsuggest.FxSuggestStorage
import mozilla.components.support.remotesettings.RemoteSettingsService
import org.mozilla.fenix.perf.lazyMonitored

/**
 * Component group for Firefox Suggest.
 *
 * @param context The Android application context.
 * @param remoteSettingsService: Remote settings service to get suggestions from
 */
class FxSuggest(context: Context, remoteSettingsService: RemoteSettingsService) {
    val storage by lazyMonitored {
        FxSuggestStorage(
            context,
            remoteSettingsService,
        )
    }

    val ingestionScheduler by lazyMonitored {
        FxSuggestIngestionScheduler(context)
    }
}

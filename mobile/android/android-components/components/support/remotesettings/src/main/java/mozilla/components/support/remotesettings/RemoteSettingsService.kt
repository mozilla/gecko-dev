/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.remotesettings

import android.content.Context
import mozilla.appservices.remotesettings.RemoteSettingsConfig2
import mozilla.appservices.remotesettings.RemoteSettingsServer
import mozilla.appservices.remotesettings.RemoteSettingsService

/**
 * Wrapper around the app-services RemoteSettingsServer
 *
 * @param context [Context] that we get the storage directory from.  This is where cached records
 * get stored.
 * @param remoteSettingsServer [RemoteSettingsServer] to download from.
 */
class RemoteSettingsService(
    context: Context,
    server: RemoteSettingsServer,
) {
    val remoteSettingsService: RemoteSettingsService by lazy {
        val databasePath = context.getDir("remote-settings", Context.MODE_PRIVATE).absolutePath
        RemoteSettingsService(databasePath, RemoteSettingsConfig2(server = server))
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.permissions.permissionoptions

import android.Manifest
import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import org.mozilla.focus.R

@Parcelize
enum class SitePermission(
    val androidPermissionsList: Array<String>,
    val labelRes: Int,
) : Parcelable {
    CAMERA(
        arrayOf(Manifest.permission.CAMERA),
        R.string.preference_phone_feature_camera,
    ),
    LOCATION(
        arrayOf(
            Manifest.permission.ACCESS_COARSE_LOCATION,
            Manifest.permission.ACCESS_FINE_LOCATION,
        ),
        R.string.preference_phone_feature_location,
    ),
    MICROPHONE(
        arrayOf(Manifest.permission.RECORD_AUDIO),
        R.string.preference_phone_feature_microphone,
    ),
    NOTIFICATION(
        emptyArray(),
        R.string.preference_phone_feature_notification,
    ),
    AUTOPLAY(
        emptyArray(),
        R.string.preference_autoplay,
    ),
    AUTOPLAY_AUDIBLE(
        emptyArray(),
        R.string.preference_autoplay,
    ),
    AUTOPLAY_INAUDIBLE(
        emptyArray(),
        R.string.preference_autoplay,
    ),
    MEDIA_KEY_SYSTEM_ACCESS(
        emptyArray(),
        R.string.preference_phone_feature_media_key_system_access,
    ),
}

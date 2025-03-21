/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.SharedPreferences
import androidx.core.content.edit

/**
 * Clear everything in shared preferences and commit changes immediately.
 */
fun SharedPreferences.clearAndCommit() = this.edit(commit = true) { clear() }

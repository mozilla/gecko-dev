/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import android.content.Context
import mozilla.components.support.utils.ext.isLandscape
import org.mozilla.fenix.components.toolbar.IncompleteRedesignToolbarFeature
import org.mozilla.fenix.ext.isTablet
import org.mozilla.fenix.ext.settings

/**
 * Returns true if navigation bar should be displayed. The returned value depends on the feature state, as well as the
 * device type and orientation – we don't show the navigation bar for tablets and in landscape mode.
 * NB: don't use it with the app context – it doesn't get recreated when a foldable changes its modes.
 */
fun Context.shouldAddNavigationBar() =
    IncompleteRedesignToolbarFeature(settings()).isEnabled && !isLandscape() && !isTablet()

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.navigation

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SwipeDirection

sealed class NavigationStep {
    data class Click(val selector: Selector) : NavigationStep()
    data class Swipe(val selector: Selector, val direction: SwipeDirection = SwipeDirection.UP) : NavigationStep()
    data class OpenNotificationsTray(val openNotificationsTrayAction: () -> Unit) : NavigationStep()
    data class EnterText(val selector: Selector) : NavigationStep()
    data class PressEnter(val selector: Selector) : NavigationStep()
}

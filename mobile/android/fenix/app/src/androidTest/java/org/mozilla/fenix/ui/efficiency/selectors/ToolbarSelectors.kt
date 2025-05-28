/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object ToolbarSelectors {
    val TOOLBAR = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "toolbar",
        description = "Toolbar",
        groups = listOf("requiredForPage"),
    )

    val TAB_COUNTER = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "tab_button",
        description = "Tab counter button",
        groups = listOf("requiredForPage"),
    )

    val URL_BAR_PLACE_HOLDER = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT,
        value = "Search or enter address",
        description = "URL bar place holder",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        TOOLBAR,
        TAB_COUNTER,
    )
}

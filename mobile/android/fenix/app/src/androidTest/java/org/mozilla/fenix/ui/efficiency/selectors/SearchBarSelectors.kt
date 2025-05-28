/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SearchBarSelectors {
    val EDIT_SEARCHBAR_VIEW = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "mozac_browser_toolbar_edit_url_view",
        description = "Empty edit search bar",
        groups = listOf(),
    )

    val URL_TEXT = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "mozac_browser_toolbar_url_view",
        description = "Page URL",
        groups = listOf("requiredForBrowserPage"),
    )

    val SEARCH_ENGINE_SELECTOR = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "search_selector",
        description = "Search selector button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        EDIT_SEARCHBAR_VIEW,
        URL_TEXT,
        SEARCH_ENGINE_SELECTOR,
    )
}

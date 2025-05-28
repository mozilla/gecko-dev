/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object BrowserPageSelectors {
    val ENGINE_VIEW = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "engineView",
        description = "Engine view",
        groups = listOf("requiredForPage"),
    )

    val PAGE_CONTENT = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT,
        value = "mozilla",
        description = "Page content",
        groups = listOf(""),
    )

    val all = listOf(
        ENGINE_VIEW,
        PAGE_CONTENT,
    )
}

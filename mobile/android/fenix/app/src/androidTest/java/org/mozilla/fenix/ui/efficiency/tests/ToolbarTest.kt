/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class ToolbarTest : BaseTest() {

    @Test
    fun toolbarItemsTest() {
        // Given: App is loaded with default settings
        // on = AndroidComposeTestRule<HomeActivityIntentTestRule, *> with app defaults

        // When: We navigate to the Homepage page with the Toolbar component visible
        on.home.navigateToPage()
        on.toolbar.navigateToPage()

        // Then: the toolbar elements should load
        on.toolbar.mozVerifyElementsByGroup("requiredForPage")
    }
}

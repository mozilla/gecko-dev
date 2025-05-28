/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class BrowserTest : BaseTest() {

    @Test
    fun browserPageItemsTest() {
        // Given: App is loaded with default settings
        // on = AndroidComposeTestRule<HomeActivityIntentTestRule, *> with app defaults

        // When: We navigate to a Browser page
        on.home.navigateToPage()
        on.browserPage.navigateToPage("mozilla.com")

        // Then: the browser screen elements should load
        on.browserPage.mozVerifyElementsByGroup("requiredForPage")
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class SearchTest : BaseTest() {

    @Test
    fun homeSearchBarItemsTest() {
        // Given: App is loaded with default settings
        // on = AndroidComposeTestRule<HomeActivityIntentTestRule, *> with app defaults

        // When: We navigate to the Homepage page with the SearchBar component visible
        on.home.navigateToPage()
        on.searchBar.navigateToPage()

        // Then: the toolbar elements should load
        on.searchBar.mozVerifyElementsByGroup("requiredForPage")
    }

    // TestRail link:
    @Test
    fun browserSearchBarItemsTest() {
        // Given: App is loaded with default settings
        // on = AndroidComposeTestRule<HomeActivityIntentTestRule, *> with app defaults

        // When: We navigate to the Browser page with the SearchBar component visible
        on.home.navigateToPage()
        on.browserPage.navigateToPage("mozilla.com")
        on.searchBar.navigateToPage()

        // Then: the search bar elements should load
        on.searchBar.mozVerifyElementsByGroup("requiredForPage")
    }
}

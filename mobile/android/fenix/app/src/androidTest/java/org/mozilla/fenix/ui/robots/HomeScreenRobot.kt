/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.ui.robots

import android.util.Log
import android.view.View
import android.widget.TextView
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assert
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertIsNotDisplayed
import androidx.compose.ui.test.assertIsNotSelected
import androidx.compose.ui.test.assertIsSelected
import androidx.compose.ui.test.filter
import androidx.compose.ui.test.hasAnyChild
import androidx.compose.ui.test.hasAnySibling
import androidx.compose.ui.test.hasContentDescription
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.hasText
import androidx.compose.ui.test.junit4.ComposeTestRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onChildAt
import androidx.compose.ui.test.onFirst
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.performScrollToNode
import androidx.compose.ui.test.performTouchInput
import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.PositionAssertions.isCompletelyAbove
import androidx.test.espresso.assertion.PositionAssertions.isPartiallyBelow
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.RootMatchers
import androidx.test.espresso.matcher.ViewMatchers
import androidx.test.espresso.matcher.ViewMatchers.Visibility
import androidx.test.espresso.matcher.ViewMatchers.isDisplayed
import androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility
import androidx.test.espresso.matcher.ViewMatchers.withId
import androidx.test.espresso.matcher.ViewMatchers.withText
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiScrollable
import androidx.test.uiautomator.UiSelector
import androidx.test.uiautomator.Until
import org.hamcrest.CoreMatchers.allOf
import org.junit.Assert.assertTrue
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.Constants.RETRY_COUNT
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityComposeTestRule
import org.mozilla.fenix.helpers.MatcherHelper.assertItemIsChecked
import org.mozilla.fenix.helpers.MatcherHelper.assertUIObjectExists
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithClassNameAndIndex
import org.mozilla.fenix.helpers.MatcherHelper.itemWithIndex
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResId
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndIndex
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdAndText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithResIdContainingText
import org.mozilla.fenix.helpers.MatcherHelper.itemWithText
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTime
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeLong
import org.mozilla.fenix.helpers.TestAssetHelper.waitingTimeShort
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.helpers.TestHelper.mDevice
import org.mozilla.fenix.helpers.TestHelper.packageName
import org.mozilla.fenix.helpers.TestHelper.scrollToElementByText
import org.mozilla.fenix.helpers.click
import org.mozilla.fenix.helpers.ext.waitNotNull
import org.mozilla.fenix.home.topsites.TopSitesTestTag
import org.mozilla.fenix.home.topsites.TopSitesTestTag.TOP_SITE_CARD_FAVICON
import org.mozilla.fenix.home.ui.HomepageTestTag.HOMEPAGE
import org.mozilla.fenix.home.ui.HomepageTestTag.HOMEPAGE_WORDMARK_LOGO
import org.mozilla.fenix.home.ui.HomepageTestTag.HOMEPAGE_WORDMARK_TEXT
import org.mozilla.fenix.home.ui.HomepageTestTag.PRIVATE_BROWSING_HOMEPAGE_BUTTON
import org.mozilla.fenix.tabstray.TabsTrayTestTag

/**
 * Implementation of Robot Pattern for the home screen menu.
 */
class HomeScreenRobot {
    fun verifyNavigationToolbar() = assertUIObjectExists(navigationToolbar())

    fun verifyHomeScreen() = assertUIObjectExists(homeScreen())

    fun verifyPrivateBrowsingHomeScreenItems() {
        verifyHomeScreenAppBarItems()
        assertUIObjectExists(
            itemContainingText(
                "$appName clears your search and browsing history from private tabs when you close them" +
                    " or quit the app. While this doesn’t make you anonymous to websites or your internet" +
                    " service provider, it makes it easier to keep what you do online private from anyone" +
                    " else who uses this device.",
            ),
        )
        verifyCommonMythsLink()
    }

    fun verifyHomeScreenAppBarItems() =
        assertUIObjectExists(homeScreen(), privateBrowsingButton(), homepageWordmarkLogo(), homepageWordmarkText())

    fun verifyHomePrivateBrowsingButton() = assertUIObjectExists(privateBrowsingButton())
    fun verifyHomeMenuButton() = assertUIObjectExists(menuButton())
    fun verifyTabButton() {
        Log.i(TAG, "verifyTabButton: Trying to verify tab counter button is visible")
        onView(allOf(withId(R.id.tab_button), isDisplayed())).check(
            matches(
                withEffectiveVisibility(
                    Visibility.VISIBLE,
                ),
            ),
        )
        Log.i(TAG, "verifyTabButton: Verified tab counter button is visible")
    }
    fun verifyCollectionsHeader(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyCollectionsHeader: Trying to verify collections header is visible")
        composeTestRule.onNodeWithText(getStringResource(R.string.collections_header)).assertIsDisplayed()
        Log.i(TAG, "verifyCollectionsHeader: Verified collections header is visible")
    }
    fun verifyNoCollectionsText(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyNoCollectionsText: Trying to verify empty collections placeholder text is displayed")
        composeTestRule.onNodeWithText(getStringResource(R.string.no_collections_description2)).assertIsDisplayed()
        Log.i(TAG, "verifyNoCollectionsText: Verified empty collections placeholder text is displayed")
    }

    fun verifyHomeWordmark() {
        Log.i(TAG, "verifyHomeWordmark: Trying to scroll 3x to the beginning of the home screen")
        homeScreenList().scrollToBeginning(3)
        Log.i(TAG, "verifyHomeWordmark: Scrolled 3x to the beginning of the home screen")
        assertUIObjectExists(homepageWordmarkLogo(), homepageWordmarkText())
    }
    fun verifyHomeComponent(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyHomeComponent: Trying to verify home screen view is visible")
        composeTestRule.onNodeWithTag(HOMEPAGE).assertIsDisplayed()
        Log.i(TAG, "verifyHomeComponent: Verified home screen view is visible")
    }

    fun verifyTabCounter(numberOfOpenTabs: String) =
        onView(
            allOf(
                withId(R.id.counter_text),
                withText(numberOfOpenTabs),
                withEffectiveVisibility(Visibility.VISIBLE),
            ),
        ).check(matches(isDisplayed()))

    fun verifyWallpaperImageApplied(isEnabled: Boolean) =
        assertUIObjectExists(itemWithResId("$packageName:id/wallpaperImageView"), exists = isEnabled)

    fun verifyFirstOnboardingCard(composeTestRule: ComposeTestRule) {
        composeTestRule.also {
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding screen title exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_title_nimbus_2),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding screen title exists")
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding screen description exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_description_nimbus_3),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding screen description exists")
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding \"Set as default browser\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_positive_button),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding \"Set as default browser\" button exists")
            Log.i(TAG, "verifyFirstOnboardingCard: Trying to verify that the first onboarding \"Not now\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_negative_button),
            ).assertExists()
            Log.i(TAG, "verifyFirstOnboardingCard: Verified that the first onboarding \"Not now\" button exists")
        }
    }

    fun verifySecondOnboardingCard(composeTestRule: ComposeTestRule) {
        composeTestRule.also {
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the second onboarding screen title exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_add_search_widget_title),
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the second onboarding screen title exists")
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the  second onboarding screen description exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_add_search_widget_description),
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the second onboarding screen description exists")
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the first onboarding \"Sign in\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_add_search_widget_positive_button),
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the first onboarding \"Add Firefox widget\" button exists")
            Log.i(TAG, "verifySecondOnboardingCard: Trying to verify that the second onboarding \"Not now\" button exists")
            it.onNodeWithTag(
                getStringResource(R.string.juno_onboarding_add_search_widget_title) + "onboarding_card.negative_button",
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the second onboarding \"Not now\" button exists")
        }
    }

    fun verifyThirdOnboardingCard(composeTestRule: ComposeTestRule) {
        composeTestRule.also {
            Log.i(TAG, "verifyThirdOnboardingCard: Trying to verify that the third onboarding screen title exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_title_2),
            ).assertExists()
            Log.i(TAG, "verifyThirdOnboardingCard: Verified that the third onboarding screen title exists")
            Log.i(TAG, "verifyThirdOnboardingCard: Trying to verify that the  third onboarding screen description exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_description_3),
            ).assertExists()
            Log.i(TAG, "verifyThirdOnboardingCard: Verified that the third onboarding screen description exists")
            Log.i(TAG, "verifyThirdOnboardingCard: Trying to verify that the first onboarding \"Sign in\" button exists")
            it.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_positive_button),
            ).assertExists()
            Log.i(TAG, "verifyThirdOnboardingCard: Verified that the first onboarding \"Sign in\" button exists")
            Log.i(TAG, "verifyThirdOnboardingCard: Trying to verify that the third onboarding \"Not now\" button exists")
            it.onNodeWithTag(
                getStringResource(R.string.juno_onboarding_sign_in_title_2) + "onboarding_card.negative_button",
            ).assertExists()
            Log.i(TAG, "verifySecondOnboardingCard: Verified that the third onboarding \"Not now\" button exists")
        }
    }

    fun clickDefaultCardNotNowOnboardingButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "clickNotNowOnboardingButton: Trying to click \"Not now\" onboarding button")
        composeTestRule.onNodeWithTag(
            getStringResource(R.string.juno_onboarding_default_browser_title_nimbus_2) + "onboarding_card.negative_button",
        ).performClick()
        Log.i(TAG, "clickNotNowOnboardingButton: Clicked \"Not now\" onboarding button")
    }

    fun clickAddSearchWidgetNotNowOnboardingButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "clickNotNowOnboardingButton: Trying to click \"Not now\" onboarding button")
        composeTestRule.onNodeWithTag(
            getStringResource(R.string.juno_onboarding_add_search_widget_title) + "onboarding_card.negative_button",
        ).performClick()
        Log.i(TAG, "clickNotNowOnboardingButton: Clicked \"Not now\" onboarding button")
    }

    fun clickSyncSignInWidgetNotNowOnboardingButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "clickNotNowOnboardingButton: Trying to click \"Not now\" onboarding button")
        composeTestRule.onNodeWithTag(
            getStringResource(R.string.juno_onboarding_sign_in_title_2) + "onboarding_card.negative_button",
        ).performClick()
        Log.i(TAG, "clickNotNowOnboardingButton: Clicked \"Not now\" onboarding button")
    }

    fun swipeSecondOnboardingCardToRight() {
        Log.i(TAG, "swipeSecondOnboardingCardToRight: Trying to perform swipe right action on second onboarding card")
        mDevice.findObject(
            UiSelector().textContains(
                getStringResource(R.string.juno_onboarding_sign_in_title_2),
            ),
        ).swipeRight(3)
        Log.i(TAG, "swipeSecondOnboardingCardToRight: Performed swipe right action on second onboarding card")
    }

    fun clickGetStartedButton(testRule: ComposeTestRule) {
        Log.i(TAG, "clickGetStartedButton: Trying to click \"Get started\" onboarding button")
        testRule.onNodeWithText(getStringResource(R.string.onboarding_home_get_started_button))
            .performClick()
        Log.i(TAG, "clickGetStartedButton: Clicked \"Get started\" onboarding button")
    }

    fun clickCloseButton(testRule: ComposeTestRule) {
        Log.i(TAG, "clickCloseButton: Trying to click close onboarding button")
        testRule.onNode(hasContentDescription("Close")).performClick()
        Log.i(TAG, "clickCloseButton: Clicked close onboarding button")
    }

    fun clickSkipButton(testRule: ComposeTestRule) {
        Log.i(TAG, "clickSkipButton: Trying to click \"Skip\" onboarding button")
        testRule
            .onNodeWithText(getStringResource(R.string.onboarding_home_skip_button))
            .performClick()
        Log.i(TAG, "clickSkipButton: Clicked \"Skip\" onboarding button")
    }

    fun verifyCommonMythsLink() =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.private_browsing_common_myths)))

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSitesList(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyExistingTopSitesList: Waiting for $waitingTime ms until the top sites list exists")
        composeTestRule.waitUntilAtLeastOneExists(hasTestTag(TopSitesTestTag.TOP_SITES), timeoutMillis = waitingTime)
        Log.i(TAG, "verifyExistingTopSitesList: Waited for $waitingTime ms until the top sites list to exists")
        Log.i(TAG, "verifyExistingTopSitesList: Trying to verify that the top sites list is displayed")
        composeTestRule.onNodeWithTag(TopSitesTestTag.TOP_SITES).assertIsDisplayed()
        Log.i(TAG, "verifyExistingTopSitesList: Verified that the top sites list is displayed")
    }

    fun verifyNotExistingTopSiteItem(composeTestRule: ComposeTestRule, vararg titles: String) {
        titles.forEach { title ->
            Log.i(TAG, "verifyNotExistingTopSiteItem: Waiting for $waitingTime ms for top site with title: $title to exist")
            itemContainingText(title).waitForExists(waitingTime)
            Log.i(TAG, "verifyNotExistingTopSiteItem: Waited for $waitingTime ms for top site with title: $title to exist")
            Log.i(TAG, "verifyNotExistingTopSiteItem: Trying to verify that top site with title: $title does not exist")
            composeTestRule.topSiteItem(title).assertDoesNotExist()
            Log.i(TAG, "verifyNotExistingTopSiteItem: Verified that top site with title: $title does not exist")
        }
    }

    fun verifySponsoredShortcutDoesNotExist(sponsoredShortcutTitle: String, position: Int) =
        assertUIObjectExists(
            itemWithResIdAndIndex("$packageName:id/top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .textContains(sponsoredShortcutTitle),
                ),
            exists = false,
        )
    fun verifyNotExistingSponsoredTopSitesList() =
        assertUIObjectExists(
            mDevice.findObject(UiSelector().resourceId("top_sites_list.top_site_item"))
                .getChild(
                    UiSelector().textContains(getStringResource(R.string.top_sites_sponsored_label)),
                ),
            exists = false,
        )

    @OptIn(ExperimentalTestApi::class)
    fun verifyExistingTopSitesTabs(composeTestRule: ComposeTestRule, vararg titles: String) {
        titles.forEach { title ->
            Log.i(TAG, "verifyExistingTopSiteItem: Waiting for $waitingTime ms until the top site with title: $title exists")
            composeTestRule.waitUntilAtLeastOneExists(
                hasTestTag(TopSitesTestTag.TOP_SITE_ITEM_ROOT).and(hasAnyChild(hasText(title))),
                timeoutMillis = waitingTimeLong,
            )
            Log.i(TAG, "verifyExistingTopSiteItem: Waited for $waitingTimeLong ms until the top site with title: $title exists")
            Log.i(TAG, "verifyExistingTopSiteItem: Trying to verify that the top site with title: $title exists")
            composeTestRule.topSiteItem(title).assertExists()
            Log.i(TAG, "verifyExistingTopSiteItem: Verified that the top site with title: $title exists")
        }
    }

    fun verifySponsoredShortcutDetails(sponsoredShortcutTitle: String, position: Int) {
        assertUIObjectExists(
            itemWithResIdAndIndex(resourceId = "top_sites_list.top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .resourceId(TOP_SITE_CARD_FAVICON),
                ),
        )
        assertUIObjectExists(
            itemWithResIdAndIndex(resourceId = "top_sites_list.top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .textContains(sponsoredShortcutTitle),
                ),
        )
        assertUIObjectExists(
            itemWithResIdAndIndex(resourceId = "top_sites_list.top_site_item", index = position - 1)
                .getChild(
                    UiSelector()
                        .textContains(getStringResource(R.string.top_sites_sponsored_label)),
                ),
        )
    }
    fun verifyTopSiteContextMenuItems(composeTestRule: ComposeTestRule) {
        verifyTopSiteContextMenuOpenInPrivateTabButton(composeTestRule)
        verifyTopSiteContextMenuRemoveButton(composeTestRule)
        verifyTopSiteContextMenuEditButton(composeTestRule)
    }

    fun verifyTopSiteContextMenuOpenInPrivateTabButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyTopSiteContextMenuOpenInPrivateTabButton: Trying to verify that the \"Open in private tab\" menu button exists")
        composeTestRule.contextMenuItemOpenInPrivateTab().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuOpenInPrivateTabButton: Verified that the \"Open in private tab\" menu button exists")
    }

    fun verifyTopSiteContextMenuEditButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyTopSiteContextMenuEditButton: Trying to verify that the \"Edit\" menu button exists")
        composeTestRule.contextMenuItemEdit().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuEditButton: Verified that the \"Edit\" menu button exists")
    }

    fun verifyTopSiteContextMenuRemoveButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyTopSiteContextMenuRemoveButton: Trying to verify that the \"Remove\" menu button exists")
        composeTestRule.contextMenuItemRemove().assertExists()
        Log.i(TAG, "verifyTopSiteContextMenuRemoveButton: Verified that the \"Remove\" menu button exists")
    }

    fun verifyTopSiteContextMenuUrlErrorMessage() {
        assertUIObjectExists(itemContainingText(getStringResource(R.string.top_sites_edit_dialog_url_error)))
    }

    fun verifyJumpBackInSectionIsDisplayed() {
        scrollToElementByText(getStringResource(R.string.recent_tabs_header))
        assertUIObjectExists(itemContainingText(getStringResource(R.string.recent_tabs_header)))
    }
    fun verifyJumpBackInSectionIsNotDisplayed(composeTestRule: ComposeTestRule) =
        composeTestRule.onNodeWithText(getStringResource(R.string.recent_tabs_header)).assertIsNotDisplayed()

    fun verifyJumpBackInItemTitle(testRule: ComposeTestRule, itemTitle: String) {
        Log.i(TAG, "verifyJumpBackInItemTitle: Trying to verify jump back in item with title: $itemTitle")
        testRule.onNodeWithTag("recent.tab.title", useUnmergedTree = true)
            .assert(hasText(itemTitle))
        Log.i(TAG, "verifyJumpBackInItemTitle: Verified jump back in item with title: $itemTitle")
    }
    fun verifyJumpBackInItemWithUrl(testRule: ComposeTestRule, itemUrl: String) {
        Log.i(TAG, "verifyJumpBackInItemWithUrl: Trying to verify jump back in item with URL: $itemUrl")
        testRule.onNodeWithTag("recent.tab.url", useUnmergedTree = true).assert(hasText(itemUrl))
        Log.i(TAG, "verifyJumpBackInItemWithUrl: Verified jump back in item with URL: $itemUrl")
    }
    fun verifyJumpBackInShowAllButton() = assertUIObjectExists(itemContainingText(getStringResource(R.string.recent_tabs_show_all)))
    fun verifyRecentlyVisitedSectionIsDisplayed(exists: Boolean) =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.history_metadata_header_2)), exists = exists)
    fun verifyBookmarksSectionIsDisplayed(exists: Boolean) =
        assertUIObjectExists(itemContainingText(getStringResource(R.string.home_bookmarks_title)), exists = exists)

    fun verifyRecentlyVisitedSearchGroupDisplayed(composeTestRule: ComposeTestRule, shouldBeDisplayed: Boolean, searchTerm: String, groupSize: Int) {
        // checks if the search group exists in the Recently visited section
        if (shouldBeDisplayed) {
            Log.i(TAG, "verifyRecentlyVisitedSearchGroupDisplayed: Trying to verify that the \"Recently visited\" section is displayed")
            composeTestRule.onNodeWithText("Recently visited").assertIsDisplayed()
            Log.i(TAG, "verifyRecentlyVisitedSearchGroupDisplayed: Verified that the \"Recently visited\" section is displayed")
            Log.i(TAG, "verifyRecentlyVisitedSearchGroupDisplayed: Trying to verify that the search group: $searchTerm has $groupSize pages")
            composeTestRule.onNodeWithText(searchTerm, useUnmergedTree = true).assert(hasAnySibling(hasText("$groupSize pages")))
            Log.i(TAG, "verifyRecentlyVisitedSearchGroupDisplayed: Verified that the search group: $searchTerm has $groupSize pages")
        } else {
            Log.i(TAG, "verifyRecentlyVisitedSearchGroupDisplayed: Trying to verify that the search group: $searchTerm is not displayed")
            composeTestRule.onNodeWithText(searchTerm, useUnmergedTree = true).assertIsNotDisplayed()
            Log.i(TAG, "verifyRecentlyVisitedSearchGroupDisplayed: Verified that the search group: $searchTerm is not displayed")
        }
    }

    // Collections elements
    @OptIn(ExperimentalTestApi::class)
    fun verifyCollectionIsDisplayed(composeTestRule: ComposeTestRule, title: String, collectionExists: Boolean = true) {
        if (collectionExists) {
            composeTestRule.waitUntilExactlyOneExists(hasText(title), waitingTime)
            Log.i(TAG, "verifyCollectionIsDisplayed: Trying to verify that collection with title: $title is displayed")
            composeTestRule.onNodeWithText(title).assertIsDisplayed()
            Log.i(TAG, "verifyCollectionIsDisplayed: Verified that collection with title: $title is displayed")
        } else {
            Log.i(TAG, "verifyCollectionIsDisplayed: Trying to verify that collection with title: $title is not displayed")
            composeTestRule.onNodeWithText(title).assertIsNotDisplayed()
            Log.i(TAG, "verifyCollectionIsDisplayed: Verified that collection with title: $title is not displayed")
        }
    }

    fun togglePrivateBrowsingModeOnOff(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "togglePrivateBrowsingModeOnOff: Trying to click private browsing home screen button")
        composeTestRule.onNodeWithContentDescription(getStringResource(R.string.content_description_private_browsing)).performClick()
        Log.i(TAG, "togglePrivateBrowsingModeOnOff: Clicked private browsing home screen button")
    }

    fun verifyThoughtProvokingStories(enabled: Boolean) {
        if (enabled) {
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_header_2)))
        } else {
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_header_2)), exists = false)
        }
    }

    fun scrollToPocketProvokingStories() {
        Log.i(TAG, "scrollToPocketProvokingStories: Trying to scroll into view the featured pocket stories")
        homeScreenList().scrollIntoView(
            mDevice.findObject(UiSelector().resourceId("pocket.recommended.story").index(2)),
        )
        Log.i(TAG, "scrollToPocketProvokingStories: Scrolled into view the featured pocket stories")
    }

    fun verifyPocketRecommendedStoriesItems(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyPocketRecommendedStoriesItems: Trying to scroll into view the \"Stories\" pocket section")
        composeTestRule.onNodeWithTag("homepage.view").performScrollToNode(hasTestTag("pocket.stories"))
        Log.i(TAG, "verifyPocketRecommendedStoriesItems: Scrolled into view the \"Stories\" pocket section")
        for (position in 0..8) {
            Log.i(TAG, "verifyPocketRecommendedStoriesItems: Trying to scroll into view the featured pocket story from position: $position")
            pocketStoriesList().scrollIntoView(UiSelector().index(position))
            Log.i(TAG, "verifyPocketRecommendedStoriesItems: Scrolled into view the featured pocket story from position: $position")
            assertUIObjectExists(itemWithIndex(position))
        }
    }

    // Temporarily not in use because Sponsored Pocket stories are only advertised for a limited time.
    // See also known issue https://bugzilla.mozilla.org/show_bug.cgi?id=1828629
//    fun verifyPocketSponsoredStoriesItems(vararg positions: Int) {
//        positions.forEach {
//            pocketStoriesList
//                .scrollIntoView(UiSelector().resourceId("pocket.sponsored.story").index(it - 1))
//
//            assertTrue(
//                "Pocket story item at position $it not found.",
//                mDevice.findObject(UiSelector().index(it - 1).resourceId("pocket.sponsored.story"))
//                    .waitForExists(waitingTimeShort),
//            )
//        }
//    }

    fun verifyDiscoverMoreStoriesButton(composeTestRule: ComposeTestRule) {
        Log.i(TAG, "verifyDiscoverMoreStoriesButton: Trying to scroll into view the \"Stories\" pocket section")
        composeTestRule.onNodeWithTag("homepage.view").performScrollToNode(hasTestTag("pocket.stories"))
        Log.i(TAG, "verifyDiscoverMoreStoriesButton: Scrolled into view the \"Stories\" pocket section")
        Log.i(TAG, "verifyDiscoverMoreStoriesButton: Trying to scroll into view the Pocket \"Discover more\" button")
        composeTestRule.onNodeWithTag("pocket.stories").performScrollToNode(hasText("Discover more"))
        Log.i(TAG, "verifyDiscoverMoreStoriesButton: Scrolled into view the Pocket \"Discover more\" button")
        assertUIObjectExists(itemWithText("Discover more"))
    }

    fun verifyStoriesByTopic(enabled: Boolean) {
        if (enabled) {
            scrollToElementByText(getStringResource(R.string.pocket_stories_categories_header))
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_categories_header)))
        } else {
            assertUIObjectExists(itemContainingText(getStringResource(R.string.pocket_stories_categories_header)), exists = false)
        }
    }

    fun verifyStoriesByTopicItems() {
        Log.i(TAG, "verifyStoriesByTopicItems: Trying to scroll into view the stories by topic home screen section")
        homeScreenList().scrollIntoView(UiSelector().resourceId("pocket.categories"))
        Log.i(TAG, "verifyStoriesByTopicItems: Scrolled into view the stories by topic home screen section")
        Log.i(TAG, "verifyStoriesByTopicItems: Trying to verify that there are more than 1 \"Stories by topic\" categories")
        assertTrue(mDevice.findObject(UiSelector().resourceId("pocket.categories")).childCount > 1)
        Log.i(TAG, "verifyStoriesByTopicItems: Verified that there are more than 1 \"Stories by topic\" categories")
    }

    fun verifyStoriesByTopicItemState(composeTestRule: ComposeTestRule, isSelected: Boolean, position: Int) {
        Log.i(TAG, "verifyStoriesByTopicItemState: Trying to scroll into view \"Stories by topic\" home screen section")
        homeScreenList().scrollIntoView(mDevice.findObject(UiSelector().resourceId("pocket.header")))
        Log.i(TAG, "verifyStoriesByTopicItemState: Scrolled into view \"Stories by topic\" home screen section")

        if (isSelected) {
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying verify that the stories by topic home screen section is displayed")
            composeTestRule.onNodeWithTag("pocket.categories").assertIsDisplayed()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic home screen section is displayed")
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying verify that the stories by topic item at position: $position is selected")
            storyByTopicItem(composeTestRule, position).assertIsSelected()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic item at position: $position is selected")
        } else {
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying verify that the stories by topic home screen section is displayed")
            composeTestRule.onNodeWithTag("pocket.categories").assertIsDisplayed()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic home screen section is displayed")
            Log.i(TAG, "verifyStoriesByTopicItemState: Trying to verify that the stories by topic item at position: $position is not selected")
            storyByTopicItem(composeTestRule, position).assertIsNotSelected()
            Log.i(TAG, "verifyStoriesByTopicItemState: Verified that the stories by topic item at position: $position is not selected")
        }
    }

    fun clickStoriesByTopicItem(composeTestRule: ComposeTestRule, position: Int) {
        Log.i(TAG, "clickStoriesByTopicItem: Trying to click stories by topic item from position: $position")
        storyByTopicItem(composeTestRule, position).performClick()
        Log.i(TAG, "clickStoriesByTopicItem: Clicked stories by topic item from position: $position")
    }

    fun verifyCustomizeHomepageButton(composeTestRule: ComposeTestRule, enabled: Boolean) {
        if (enabled) {
            Log.i(TAG, "verifyCustomizeHomepageButton: Trying to perform scroll to the \"Customize homepage\" button")
            composeTestRule.onNodeWithTag(HOMEPAGE).performScrollToNode(hasText("Customize homepage"))
            Log.i(TAG, "verifyCustomizeHomepageButton: Performed scroll to the \"Customize homepage\" button")
            Log.i(TAG, "verifyCustomizeHomepageButton: Trying to verify that the \"Customize homepage\" button is displayed")
            composeTestRule.onNodeWithText("Customize homepage").assertIsDisplayed()
            Log.i(TAG, "verifyCustomizeHomepageButton: Verified that the \"Customize homepage\" button is displayed")
        } else {
            Log.i(TAG, "verifyCustomizeHomepageButton: Trying to verify that the \"Customize homepage\" button is not displayed")
            composeTestRule.onNodeWithText("Customize homepage").assertIsNotDisplayed()
            Log.i(TAG, "verifyCustomizeHomepageButton: Verified that the \"Customize homepage\" button is not displayed")
        }
    }

    fun getProvokingStoryPublisher(position: Int): String {
        val publisher = mDevice.findObject(
            UiSelector()
                .resourceId("pocket.recommended.story")
                .index(position - 1),
        ).getChild(
            UiSelector()
                .className("android.widget.TextView")
                .index(1),
        ).text

        return publisher
    }

    fun verifyAddressBarPosition(bottomPosition: Boolean) {
        Log.i(TAG, "verifyAddressBarPosition: Trying to verify toolbar is set to top: $bottomPosition")
        onView(withId(R.id.toolbarLayout))
            .check(
                if (bottomPosition) {
                    isPartiallyBelow(withId(R.id.homepageView))
                } else {
                    isCompletelyAbove(withId(R.id.homeAppBar))
                },
            )
        Log.i(TAG, "verifyAddressBarPosition: Verified toolbar position is set to top: $bottomPosition")
    }

    fun verifyNavigationToolbarIsSetToTheBottomOfTheHomeScreen() {
        Log.i(TAG, "verifyAddressBarPosition: Trying to verify that the navigation toolbar is set to bottom")
        onView(withId(R.id.toolbar_navbar_container)).check(isPartiallyBelow(withId(R.id.homepageView)))
        Log.i(TAG, "verifyAddressBarPosition: Verified that the navigation toolbar is set to bottom")
    }

    fun verifyNimbusMessageCard(title: String, text: String, action: String) {
        val textView = UiSelector()
            .className(ComposeView::class.java)
            .className(View::class.java)
            .className(TextView::class.java)
        assertTrue(
            mDevice.findObject(textView.textContains(title)).waitForExists(waitingTime),
        )
        assertTrue(
            mDevice.findObject(textView.textContains(text)).waitForExists(waitingTime),
        )
        assertTrue(
            mDevice.findObject(textView.textContains(action)).waitForExists(waitingTime),
        )
    }

    fun verifyIfInPrivateOrNormalMode(privateBrowsingEnabled: Boolean) {
        Log.i(TAG, "verifyIfInPrivateOrNormalMode: Trying to verify private browsing mode is enabled")
        assert(isPrivateModeEnabled() == privateBrowsingEnabled)
        Log.i(TAG, "verifyIfInPrivateOrNormalMode: Verified private browsing mode is enabled: $privateBrowsingEnabled")
    }

    fun verifySetAsDefaultBrowserDialogWhileFirefoxIsNotSetAsDefaultBrowser() {
        assertUIObjectExists(
            itemContainingText("Set Firefox Fenix as your default browser app?"),
            itemContainingText(appName),
            itemContainingText("Cancel"),
            itemContainingText("Set as default"),
        )
        assertItemIsChecked(
            firefoxOptionSetAsDefaultBrowserDialogRadioButton(),
            isChecked = false,
        )
    }

    class Transition {

        fun openTabDrawerFromRedesignedToolbar(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            for (i in 1..RETRY_COUNT) {
                try {
                    Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Started try #$i")
                    assertUIObjectExists(tabsCounterFromRedesignedToolbar())
                    Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Trying to click the tab counter button")
                    tabsCounter().click()
                    Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Clicked the tab counter button")
                    Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Trying to verify the tabs tray exists")
                    composeTestRule.onNodeWithTag(TabsTrayTestTag.TABS_TRAY).assertExists()
                    Log.i(TAG, "openTabDrawer: Verified the tabs tray exists")

                    break
                } catch (e: AssertionError) {
                    Log.i(TAG, "openTabDrawerFromRedesignedToolbar: AssertionError caught, executing fallback methods")
                    if (i == RETRY_COUNT) {
                        throw e
                    } else {
                        Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Waiting for device to be idle")
                        mDevice.waitForIdle()
                        Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Waited for device to be idle")
                    }
                }
            }
            Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Trying to verify the tabs tray new tab FAB button exists")
            composeTestRule.onNodeWithTag(TabsTrayTestTag.FAB).assertExists()
            Log.i(TAG, "openTabDrawerFromRedesignedToolbar: Verified the tabs tray new tab FAB button exists")

            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun openTabDrawer(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "openTabDrawer: Waiting for device to be idle for $waitingTime ms")
            mDevice.waitForIdle(waitingTime)
            Log.i(TAG, "openTabDrawer: Device was idle for $waitingTime ms")
            Log.i(TAG, "openTabDrawer: Trying to click tab counter button")
            onView(withId(R.id.tab_button)).click()
            Log.i(TAG, "openTabDrawer: Clicked tab counter button")
            Log.i(TAG, "openTabDrawer: Trying to verify the tabs tray exists")
            composeTestRule.onNodeWithTag(TabsTrayTestTag.TABS_TRAY).assertExists()
            Log.i(TAG, "openTabDrawer: Verified the tabs tray exists")

            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun openThreeDotMenu(interact: ThreeDotMenuMainRobot.() -> Unit): ThreeDotMenuMainRobot.Transition {
            // Issue: https://github.com/mozilla-mobile/fenix/issues/21578
            try {
                Log.i(TAG, "openThreeDotMenu: Try block")
                mDevice.waitNotNull(
                    Until.findObject(By.res("$packageName:id/menuButton")),
                    waitingTime,
                )
            } catch (e: AssertionError) {
                Log.i(TAG, "openThreeDotMenu: Catch block")
                Log.i(TAG, "openThreeDotMenu: Trying to click device back button")
                mDevice.pressBack()
                Log.i(TAG, "openThreeDotMenu: Clicked device back button")
            } finally {
                Log.i(TAG, "openThreeDotMenu: Finally block")
                Log.i(TAG, "openThreeDotMenu: Trying to click main menu button")
                threeDotButton().perform(click())
                Log.i(TAG, "openThreeDotMenu: Clicked main menu button")
            }

            ThreeDotMenuMainRobot().interact()
            return ThreeDotMenuMainRobot.Transition()
        }

        fun openThreeDotMenu(composeTestRule: ComposeTestRule, interact: ThreeDotMenuMainRobotCompose.() -> Unit): ThreeDotMenuMainRobotCompose.Transition {
            Log.i(TAG, "openThreeDotMenuFromRedesignedToolbar: Trying to click main menu button")
            itemWithResId("$packageName:id/menuButton").click()
            Log.i(TAG, "openThreeDotMenuFromRedesignedToolbar: Clicked main menu button")

            ThreeDotMenuMainRobotCompose(composeTestRule).interact()
            return ThreeDotMenuMainRobotCompose.Transition(composeTestRule)
        }

        fun openSearch(interact: SearchRobot.() -> Unit): SearchRobot.Transition {
            Log.i(TAG, "openSearch: Waiting for $waitingTime ms for the navigation toolbar to exist")
            navigationToolbar().waitForExists(waitingTime)
            Log.i(TAG, "openSearch: Waited for $waitingTime ms for the navigation toolbar to exist")
            Log.i(TAG, "openSearch: Trying to click navigation toolbar")
            navigationToolbar().click()
            Log.i(TAG, "openSearch: Clicked navigation toolbar")
            Log.i(TAG, "openSearch: Waiting for device to be idle")
            mDevice.waitForIdle()
            Log.i(TAG, "openSearch: Device was idle")

            SearchRobot().interact()
            return SearchRobot.Transition()
        }

        fun clickUpgradingUserOnboardingSignInButton(
            testRule: ComposeTestRule,
            interact: SyncSignInRobot.() -> Unit,
        ): SyncSignInRobot.Transition {
            Log.i(TAG, "clickUpgradingUserOnboardingSignInButton: Trying to click the upgrading user onboarding \"Sign in\" button")
            testRule.onNodeWithText("Sign in").performClick()
            Log.i(TAG, "clickUpgradingUserOnboardingSignInButton: Clicked the upgrading user onboarding \"Sign in\" button")

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }

        fun togglePrivateBrowsingMode(switchPBModeOn: Boolean = true) {
            // Switch to private browsing homescreen
            if (switchPBModeOn && !isPrivateModeEnabled()) {
                Log.i(TAG, "togglePrivateBrowsingMode: Waiting for $waitingTime ms for private browsing button to exist")
                privateBrowsingButton().waitForExists(waitingTime)
                Log.i(TAG, "togglePrivateBrowsingMode: Waited for $waitingTime ms for private browsing button to exist")
                Log.i(TAG, "togglePrivateBrowsingMode: Trying to click private browsing button")
                privateBrowsingButton().click()
                Log.i(TAG, "togglePrivateBrowsingMode: Clicked private browsing button")
            }

            // Switch to normal browsing homescreen
            if (!switchPBModeOn && isPrivateModeEnabled()) {
                Log.i(TAG, "togglePrivateBrowsingMode: Waiting for $waitingTime ms for private browsing button to exist")
                privateBrowsingButton().waitForExists(waitingTime)
                Log.i(TAG, "togglePrivateBrowsingMode: Waited for $waitingTime ms for private browsing button to exist")
                Log.i(TAG, "togglePrivateBrowsingMode: Trying to click private browsing button")
                privateBrowsingButton().click()
                privateBrowsingButton().click()
                Log.i(TAG, "togglePrivateBrowsingMode: Clicked private browsing button")
            }
        }

        fun triggerPrivateBrowsingShortcutPrompt(interact: AddToHomeScreenRobot.() -> Unit): AddToHomeScreenRobot.Transition {
            // Loop to press the PB icon for 5 times to display the Add the Private Browsing Shortcut CFR
            for (i in 1..5) {
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Waiting for $waitingTime ms for private browsing button to exist")
                mDevice.findObject(UiSelector().resourceId("$packageName:id/privateBrowsingButton"))
                    .waitForExists(
                        waitingTime,
                    )
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Waited for $waitingTime ms for private browsing button to exist")
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Trying to click private browsing button")
                privateBrowsingButton().click()
                Log.i(TAG, "triggerPrivateBrowsingShortcutPrompt: Clicked private browsing button")
            }

            AddToHomeScreenRobot().interact()
            return AddToHomeScreenRobot.Transition()
        }

        fun pressBack() {
            Log.i(TAG, "pressBack: Trying to click device back button")
            onView(ViewMatchers.isRoot()).perform(ViewActions.pressBack())
            Log.i(TAG, "pressBack: Clicked device back button")
        }

        fun openNavigationToolbar(interact: NavigationToolbarRobot.() -> Unit): NavigationToolbarRobot.Transition {
            Log.i(TAG, "openNavigationToolbar: Waiting for $waitingTime ms for navigation the toolbar to exist")
            mDevice.findObject(UiSelector().resourceId("$packageName:id/toolbar"))
                .waitForExists(waitingTime)
            Log.i(TAG, "openNavigationToolbar: Waited for $waitingTime ms for the navigation toolbar to exist")
            Log.i(TAG, "openNavigationToolbar: Trying to click the navigation toolbar")
            navigationToolbar().click()
            Log.i(TAG, "openNavigationToolbar: Clicked the navigation toolbar")

            NavigationToolbarRobot().interact()
            return NavigationToolbarRobot.Transition()
        }

        fun openContextMenuOnTopSitesWithTitle(
            composeTestRule: ComposeTestRule,
            title: String,
            interact: HomeScreenRobot.() -> Unit,
        ): Transition {
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Trying to scroll to top site with title: $title")
            composeTestRule.topSiteItem(title).performScrollTo()
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Scrolled to top site with title: $title")
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Trying to long click top site with title: $title")
            composeTestRule.topSiteItem(title).performTouchInput { longClick() }
            Log.i(TAG, "openContextMenuOnTopSitesWithTitle: Long clicked top site with title: $title")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteTabWithTitle(
            composeTestRule: ComposeTestRule,
            title: String,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteTabWithTitle: Trying to scroll to top site with title: $title")
            composeTestRule.topSiteItem(title).performScrollTo()
            Log.i(TAG, "openTopSiteTabWithTitle: Scrolled to top site with title: $title")
            Log.i(TAG, "openTopSiteTabWithTitle: Trying to click top site with title: $title")
            composeTestRule.topSiteItem(title).performClick()
            Log.i(TAG, "openTopSiteTabWithTitle: Clicked top site with title: $title")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun editTopSite(
            composeTestRule: ComposeTestRule,
            title: String,
            url: String,
            interact: HomeScreenRobot.() -> Unit,
        ): Transition {
            Log.i(TAG, "editTopSite: Trying to click the \"Edit\" menu button")
            composeTestRule.contextMenuItemEdit().performClick()
            Log.i(TAG, "editTopSite: Clicked the \"Edit\" menu button")
            itemWithResId("$packageName:id/top_site_title")
                .also {
                    Log.i(TAG, "editTopSite: Waiting for $waitingTimeShort ms for top site name text box to exist")
                    it.waitForExists(waitingTimeShort)
                    Log.i(TAG, "editTopSite: Waited for $waitingTimeShort ms for top site name text box to exist")
                    Log.i(TAG, "editTopSite: Trying to set top site name text box text to: $title")
                    it.setText(title)
                    Log.i(TAG, "editTopSite: Top site name text box text was set to: $title")
                }
            itemWithResId("$packageName:id/top_site_url")
                .also {
                    Log.i(TAG, "editTopSite: Waiting for $waitingTimeShort ms for top site url text box to exist")
                    it.waitForExists(waitingTimeShort)
                    Log.i(TAG, "editTopSite: Waited for $waitingTimeShort ms for top site url text box to exist")
                    Log.i(TAG, "editTopSite: Trying to set top site url text box text to: $url")
                    it.setText(url)
                    Log.i(TAG, "editTopSite: Top site url text box text was set to: $url")
                }
            Log.i(TAG, "editTopSite: Trying to click the \"Save\" dialog button")
            itemWithResIdContainingText("android:id/button1", "Save").click()
            Log.i(TAG, "editTopSite: Clicked the \"Save\" dialog button")

            HomeScreenRobot().interact()
            return Transition()
        }

        @OptIn(ExperimentalTestApi::class)
        fun removeTopSite(composeTestRule: ComposeTestRule, interact: HomeScreenRobot.() -> Unit): Transition {
            Log.i(TAG, "removeTopSite: Trying to click the \"Remove\" menu button")
            composeTestRule.contextMenuItemRemove().performClick()
            Log.i(TAG, "removeTopSite: Clicked the \"Remove\" menu button")
            Log.i(TAG, "removeTopSite: Waiting for $waitingTime ms until the \"Remove\" menu button does not exist")
            composeTestRule.waitUntilDoesNotExist(hasTestTag(TopSitesTestTag.REMOVE), waitingTime)
            Log.i(TAG, "removeTopSite: Waited for $waitingTime ms until the \"Remove\" menu button does not exist")

            HomeScreenRobot().interact()
            return Transition()
        }

        fun openTopSiteInPrivateTab(
            composeTestRule: ComposeTestRule,
            interact: BrowserRobot.() -> Unit,
        ): BrowserRobot.Transition {
            Log.i(TAG, "openTopSiteInPrivateTab: Trying to click the \"Open in private tab\" menu button")
            composeTestRule.contextMenuItemOpenInPrivateTab().performClick()
            Log.i(TAG, "openTopSiteInPrivateTab: Clicked the \"Open in private tab\" menu button")
            composeTestRule.waitForIdle()

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSponsorsAndPrivacyButton(composeTestRule: ComposeTestRule, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickSponsorsAndPrivacyButton: Trying to click \"Our sponsors & your privacy\" context menu button and wait for $waitingTime ms for a new window")
            composeTestRule.onNodeWithText(getStringResource(R.string.top_sites_menu_sponsor_privacy)).performClick()
            Log.i(TAG, "clickSponsorsAndPrivacyButton: Clicked \"Our sponsors & your privacy\" context menu button and waited for $waitingTime ms for a new window")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSponsoredShortcutsSettingsButton(composeTestRule: ComposeTestRule, interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            Log.i(TAG, "clickSponsoredShortcutsSettingsButton: Trying to click \"Settings\" context menu button and wait for $waitingTime for a new window")
            composeTestRule.onNodeWithText(getStringResource(R.string.top_sites_menu_settings)).performClick()
            Log.i(TAG, "clickSponsoredShortcutsSettingsButton: Clicked \"Settings\" context menu button and waited for $waitingTime for a new window")

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun openCommonMythsLink(interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "openCommonMythsLink: Trying to click private browsing home screen common myths link")
            mDevice.findObject(
                UiSelector()
                    .textContains(
                        getStringResource(R.string.private_browsing_common_myths),
                    ),
            ).also { it.click() }
            Log.i(TAG, "openCommonMythsLink: Clicked private browsing home screen common myths link")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSaveTabsToCollectionButton(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickSaveTabsToCollectionButton: Trying to click save tabs to collection button")
            saveTabsToCollectionButton(composeTestRule).performClick()
            Log.i(TAG, "clickSaveTabsToCollectionButton: Clicked save tabs to collection button")
            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun expandCollection(composeTestRule: ComposeTestRule, title: String, interact: CollectionRobot.() -> Unit): CollectionRobot.Transition {
            Log.i(TAG, "expandCollection: Trying to click collection with title: $title")
            composeTestRule.onNodeWithText(title).performClick()
            Log.i(TAG, "expandCollection: Clicked collection with title: $title")

            CollectionRobot().interact()
            return CollectionRobot.Transition()
        }

        fun openRecentlyVisitedSearchGroupHistoryList(composeTestRule: ComposeTestRule, title: String, interact: HistoryRobot.() -> Unit): HistoryRobot.Transition {
            Log.i(TAG, "openRecentlyVisitedSearchGroupHistoryList: Trying to click recently visited search group with title: $title")
            composeTestRule.onNodeWithText(title).performClick()
            Log.i(TAG, "openRecentlyVisitedSearchGroupHistoryList: Clicked recently visited search group with title: $title")

            HistoryRobot().interact()
            return HistoryRobot.Transition()
        }

        fun openCustomizeHomepage(composeTestRule: ComposeTestRule, interact: SettingsSubMenuHomepageRobot.() -> Unit): SettingsSubMenuHomepageRobot.Transition {
            Log.i(TAG, "openCustomizeHomepage: Trying to perform scroll to the \"Customize homepage\" button")
            composeTestRule.onNodeWithTag(HOMEPAGE).performScrollToNode(hasText("Customize homepage"))
            Log.i(TAG, "openCustomizeHomepage: Performed scroll to the \"Customize homepage\" button")
            Log.i(TAG, "openCustomizeHomepage: Trying to click \"Customize homepage\" button")
            composeTestRule.onNodeWithText("Customize homepage").performClick()
            Log.i(TAG, "openCustomizeHomepage: Clicked \"Customize homepage\" button")

            SettingsSubMenuHomepageRobot().interact()
            return SettingsSubMenuHomepageRobot.Transition()
        }

        fun clickJumpBackInShowAllButton(composeTestRule: HomeActivityComposeTestRule, interact: TabDrawerRobot.() -> Unit): TabDrawerRobot.Transition {
            Log.i(TAG, "clickJumpBackInShowAllButton: Trying to click \"Show all\" button and wait for $waitingTime ms for a new window")
            mDevice
                .findObject(
                    UiSelector()
                        .textContains(getStringResource(R.string.recent_tabs_show_all)),
                ).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "clickJumpBackInShowAllButton: Clicked \"Show all\" button and wait for $waitingTime ms for a new window")

            TabDrawerRobot(composeTestRule).interact()
            return TabDrawerRobot.Transition(composeTestRule)
        }

        fun clickPocketStoryItem(publisher: String, position: Int, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickPocketStoryItem: Trying to click pocket story item published by: $publisher at position: $position and wait for $waitingTime ms for a new window")
            mDevice.findObject(
                UiSelector()
                    .className("android.view.View")
                    .index(position - 1),
            ).getChild(
                UiSelector()
                    .className("android.widget.TextView")
                    .index(1)
                    .textContains(publisher),
            ).clickAndWaitForNewWindow(waitingTime)
            Log.i(TAG, "clickPocketStoryItem: Clicked pocket story item published by: $publisher at position: $position and wait for $waitingTime ms for a new window")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickPocketDiscoverMoreButton(composeTestRule: ComposeTestRule, interact: BrowserRobot.() -> Unit): BrowserRobot.Transition {
            Log.i(TAG, "clickPocketDiscoverMoreButton: Trying to scroll into view the \"Discover more\" button")
            pocketStoriesList().scrollToEnd(3)
            Log.i(TAG, "clickPocketDiscoverMoreButton: Scrolled into view the \"Discover more\" button")

            Log.i(TAG, "clickPocketDiscoverMoreButton: Trying to click the \"Discover more\" button")
            composeTestRule.onNodeWithTag("pocket.discover.more.story").performClick()
            Log.i(TAG, "clickPocketDiscoverMoreButton: Clicked the \"Discover more\" button")

            BrowserRobot().interact()
            return BrowserRobot.Transition()
        }

        fun clickSetAsDefaultBrowserOnboardingButton(
            composeTestRule: ComposeTestRule,
            interact: SettingsRobot.() -> Unit,
        ): SettingsRobot.Transition {
            Log.i(TAG, "clickSetAsDefaultBrowserOnboardingButton: Trying to click \"Set as default browser\" onboarding button")
            composeTestRule.onNodeWithText(
                getStringResource(R.string.juno_onboarding_default_browser_positive_button),
            ).performClick()
            Log.i(TAG, "clickSetAsDefaultBrowserOnboardingButton: Clicked \"Set as default browser\" onboarding button")

            SettingsRobot().interact()
            return SettingsRobot.Transition()
        }

        fun clickSignInOnboardingButton(
            composeTestRule: ComposeTestRule,
            interact: SyncSignInRobot.() -> Unit,
        ): SyncSignInRobot.Transition {
            Log.i(TAG, "clickSignInOnboardingButton: Trying to click \"Sign in\" onboarding button")
            composeTestRule.onNodeWithText(
                getStringResource(R.string.juno_onboarding_sign_in_positive_button),
            ).performClick()
            Log.i(TAG, "clickSignInOnboardingButton: Clicked \"Sign in\" onboarding button")

            SyncSignInRobot().interact()
            return SyncSignInRobot.Transition()
        }
    }
}

fun homeScreen(interact: HomeScreenRobot.() -> Unit): HomeScreenRobot.Transition {
    HomeScreenRobot().interact()
    return HomeScreenRobot.Transition()
}

private fun homeScreenList() =
    UiScrollable(
        UiSelector()
            .resourceId(HOMEPAGE)
            .scrollable(true),
    ).setAsVerticalList()

private fun threeDotButton() = onView(allOf(withId(R.id.menuButton)))

private fun saveTabsToCollectionButton(composeTestRule: ComposeTestRule) =
    composeTestRule.onNodeWithText(getStringResource(R.string.tabs_menu_save_to_collection1))

private fun tabsCounterFromRedesignedToolbar() = itemWithResId("$packageName:id/counter_box")

private fun tabsCounter() =
    mDevice.findObject(By.res("$packageName:id/counter_root"))

private fun sponsoredShortcut(sponsoredShortcutTitle: String) =
    onView(
        allOf(
            withId(R.id.top_site_title),
            withText(sponsoredShortcutTitle),
        ),
    )

private fun storyByTopicItem(composeTestRule: ComposeTestRule, position: Int) =
    composeTestRule.onNodeWithTag("pocket.categories").onChildAt(position - 1)

private fun homeScreen() =
    itemWithResId("$packageName:id/homepageView")
private fun privateBrowsingButton() =
    itemWithResId(PRIVATE_BROWSING_HOMEPAGE_BUTTON)

private fun isPrivateModeEnabled(): Boolean =
    itemWithResId(PRIVATE_BROWSING_HOMEPAGE_BUTTON).isChecked

private fun homepageWordmarkLogo() =
    itemWithResId(HOMEPAGE_WORDMARK_LOGO)

private fun homepageWordmarkText() =
    itemWithResId(HOMEPAGE_WORDMARK_TEXT)

private fun navigationToolbar() =
    itemWithResId("$packageName:id/toolbar")
private fun menuButton() =
    itemWithResId("$packageName:id/menuButton")
private fun tabCounter(numberOfOpenTabs: String) =
    itemWithResIdAndText("$packageName:id/counter_text", numberOfOpenTabs)

fun deleteFromHistory() =
    onView(
        allOf(
            withId(R.id.simple_text),
            withText(R.string.delete_from_history),
        ),
    ).inRoot(RootMatchers.isPlatformPopup())

private fun pocketStoriesList() =
    UiScrollable(UiSelector().resourceId("pocket.stories")).setAsHorizontalList()

private fun firefoxOptionSetAsDefaultBrowserDialogRadioButton() =
    itemWithClassNameAndIndex(
        className = "android.widget.RadioButton",
        index = 2,
    ).getFromParent(
        UiSelector().className("android.widget.LinearLayout").index(1),
    )

private fun ComposeTestRule.topSiteItem(title: String) =
    onAllNodesWithTag(TopSitesTestTag.TOP_SITE_ITEM_ROOT).filter(hasAnyChild(hasText(title))).onFirst()

private fun ComposeTestRule.contextMenuItemOpenInPrivateTab() = onAllNodesWithTag(TopSitesTestTag.OPEN_IN_PRIVATE_TAB).onFirst()

private fun ComposeTestRule.contextMenuItemEdit() = onAllNodesWithTag(TopSitesTestTag.EDIT).onFirst()

private fun ComposeTestRule.contextMenuItemRemove() = onAllNodesWithTag(TopSitesTestTag.REMOVE).onFirst()

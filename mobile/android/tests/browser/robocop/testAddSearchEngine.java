/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.io.File;
import java.util.ArrayList;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.Actions;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.home.HomePager;
import org.mozilla.gecko.home.SearchEngineBar;
import org.mozilla.gecko.R;

import android.widget.ImageView;
import android.widget.ListView;

import com.jayway.android.robotium.solo.Condition;

/**
 * Test adding a search engine from an input field context menu.
 * 1. Get the number of existing search engines from the SearchEngine:Data event and as displayed in about:home.
 * 2. Load a page with a text field, open the context menu and add a search engine from the page.
 * 3. Get the number of search engines after adding the new one and verify it has increased by 1.
 */
public class testAddSearchEngine extends AboutHomeTest {
    private final int MAX_WAIT_TEST_MS = 5000;
    private final String SEARCH_TEXT = "Firefox for Android";
    private final String ADD_SEARCHENGINE_OPTION_TEXT = "Add as Search Engine";

    public void testAddSearchEngine() {
        String blankPageURL = getAbsoluteUrl(mStringHelper.ROBOCOP_BLANK_PAGE_01_URL);
        String searchEngineURL = getAbsoluteUrl(mStringHelper.ROBOCOP_SEARCH_URL);

        blockForGeckoReady();
        int height = mDriver.getGeckoTop() + 150;
        int width = mDriver.getGeckoLeft() + 150;

        inputAndLoadUrl(blankPageURL);
        waitForText(mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);

        // Get the searchengine data by clicking the awesomebar - this causes Gecko to send Java the list
        // of search engines.
        Actions.EventExpecter searchEngineDataEventExpector = mActions.expectGeckoEvent("SearchEngines:Data");
        focusUrlBar();
        mActions.sendKeys(SEARCH_TEXT);
        String eventData = searchEngineDataEventExpector.blockForEventData();
        searchEngineDataEventExpector.unregisterListener();

        ArrayList<String> searchEngines;
        try {
            // Parse the data to get the number of searchengines.
            searchEngines = getSearchEnginesNames(eventData);
        } catch (JSONException e) {
            mAsserter.ok(false, "Fatal exception in testAddSearchEngine while decoding JSON search engine string from Gecko prior to addition of new engine.", e.toString());
            return;
        }
        final int initialNumSearchEngines = searchEngines.size();
        mAsserter.dumpLog("Search Engines list = " + searchEngines.toString());

        // Verify that the number of displayed search engines is the same as the one received through the SearchEngines:Data event.
        verifyDisplayedSearchEnginesCount(initialNumSearchEngines);

        // Load the page for the search engine to add.
        inputAndLoadUrl(searchEngineURL);
        verifyUrlBarTitle(searchEngineURL);

        // Used to long-tap on the search input box for the search engine to add.
        getInstrumentation().waitForIdleSync();
        mAsserter.dumpLog("Long Clicking at width = " + String.valueOf(width) + " and height = " + String.valueOf(height));
        mSolo.clickLongOnScreen(width,height);

        ImageView view = waitForViewWithDescription(ImageView.class, ADD_SEARCHENGINE_OPTION_TEXT);
        mAsserter.isnot(view, null, "The action mode was opened");

        // Add the search engine
        mSolo.clickOnView(view);
        waitForText("Cancel");
        clickOnButton("OK");
        mAsserter.ok(!mSolo.searchText(ADD_SEARCHENGINE_OPTION_TEXT), "Adding the Search Engine", "The add Search Engine pop-up has been closed");
        waitForText(mStringHelper.ROBOCOP_SEARCH_TITLE); // Make sure the pop-up is closed and we are back at the searchengine page

        // Load Robocop Blank 1 again to give the time for the searchengine to be added
        // TODO: This is a potential source of intermittent oranges - it's a race condition!
        loadUrl(blankPageURL);
        waitForText(mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);

        // Load search engines again and check that the quantity of engines has increased by 1.
        searchEngineDataEventExpector = mActions.expectGeckoEvent("SearchEngines:Data");
        focusUrlBar();
        mActions.sendKeys(SEARCH_TEXT);
        eventData = searchEngineDataEventExpector.blockForEventData();

        try {
            // Parse the data to get the number of searchengines
            searchEngines = getSearchEnginesNames(eventData);
        } catch (JSONException e) {
            mAsserter.ok(false, "Fatal exception in testAddSearchEngine while decoding JSON search engine string from Gecko after adding of new engine.", e.toString());
            return;
        }

        mAsserter.dumpLog("Search Engines list = " + searchEngines.toString());
        mAsserter.is(searchEngines.size(), initialNumSearchEngines + 1, "Checking the number of Search Engines has increased");

        // Verify that the number of displayed searchengines is the same as the one received through the SearchEngines:Data event.
        verifyDisplayedSearchEnginesCount(initialNumSearchEngines + 1);
        searchEngineDataEventExpector.unregisterListener();

        // Verify that the search plugin XML file for the new engine ended up where we expected it to.
        // This file name is created in nsSearchService.js based on the name of the new engine.
        final File f = GeckoProfile.get(getActivity()).getFile("searchplugins/robocop-search-engine.xml");
        mAsserter.ok(f.exists(), "Checking that new search plugin file exists", "");
    }

    /**
     * Helper method to decode a list of search engine names from the provided search engine information
     * JSON string sent from Gecko.
     * @param searchEngineData The JSON string representing the search engine array to process
     * @return An ArrayList<String> containing the names of all the search engines represented in
     *         the provided JSON message.
     * @throws JSONException In the event that the JSON provided cannot be decoded.
     */
    public ArrayList<String> getSearchEnginesNames(String searchEngineData) throws JSONException {
        JSONObject data = new JSONObject(searchEngineData);
        JSONArray engines = data.getJSONArray("searchEngines");

        ArrayList<String> searchEngineNames = new ArrayList<String>();
        for (int i = 0; i < engines.length(); i++) {
            JSONObject engineJSON = engines.getJSONObject(i);
            searchEngineNames.add(engineJSON.getString("name"));
        }
        return searchEngineNames;
    }

    /**
     * Method to verify that the displayed number of search engines matches the expected number.
     * @param expectedCount The expected number of search engines.
     */
    public void verifyDisplayedSearchEnginesCount(final int expectedCount) {
        mSolo.clearEditText(0);
        mActions.sendKeys(SEARCH_TEXT);
        boolean correctNumSearchEnginesDisplayed = waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                ListView searchResultList = findListViewWithTag(HomePager.LIST_TAG_BROWSER_SEARCH);
                if (searchResultList == null || searchResultList.getAdapter() == null) {
                    return false;
                }

                SearchEngineBar searchEngineBar = (SearchEngineBar) mSolo.getView(R.id.search_engine_bar);
                if (searchEngineBar == null || searchEngineBar.getAdapter() == null) {
                    return false;
                }

                return (searchResultList.getAdapter().getCount() + searchEngineBar.getAdapter().getCount() == expectedCount);
            }
        }, MAX_WAIT_TEST_MS);

        // Exit about:home
        mSolo.goBack();
        waitForText(mStringHelper.ROBOCOP_BLANK_PAGE_01_TITLE);
        mAsserter.ok(correctNumSearchEnginesDisplayed, expectedCount + " Search Engines should be displayed" , "The correct number of Search Engines has been displayed");
    }
}

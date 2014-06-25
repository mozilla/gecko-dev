package org.mozilla.gecko.tests;

import org.mozilla.gecko.Element;
import org.mozilla.gecko.R;

import android.app.Activity;
import android.view.View;

import com.jayway.android.robotium.solo.Condition;

/* A simple test that creates 2 new tabs and checks that the tab count increases. */
public class testNewTab extends BaseTest {
    private Element tabCount = null;
    private Element tabs = null;
    private Element closeTab = null;
    private int tabCountInt = 0;

    public void testNewTab() {
        String url = getAbsoluteUrl("/robocop/robocop_blank_01.html");
        String url2 = getAbsoluteUrl("/robocop/robocop_blank_02.html");

        blockForGeckoReady();

        Activity activity = getActivity();
        tabCount = mDriver.findElement(activity, R.id.tabs_counter);
        tabs = mDriver.findElement(activity, R.id.tabs);
        mAsserter.ok(tabCount != null && tabs != null,
                     "Checking elements", "all elements present");

        int expectedTabCount = 1;
        getTabCount(expectedTabCount);
        mAsserter.is(tabCountInt, expectedTabCount, "Initial number of tabs correct");

        addTab(url);
        expectedTabCount++;
        getTabCount(expectedTabCount);
        mAsserter.is(tabCountInt, expectedTabCount, "Number of tabs increased");

        addTab(url2);
        expectedTabCount++;
        getTabCount(expectedTabCount);
        mAsserter.is(tabCountInt, expectedTabCount, "Number of tabs increased");

        // cleanup: close all opened tabs
        //closeTabs();
    }

    private void closeTabs() {
        final int closeTabId = closeTab.getId();
        String tabCountText = null;

        // open tabs panel
        boolean clicked = tabs.click();
        if (!clicked) {
            mAsserter.ok(clicked != false, "checking that tabs clicked", "tabs element clicked");
        }

        // wait for closeTab to appear (this is usually immediate)
        boolean success = waitForTest(new BooleanTest() {
            @Override
            public boolean test() {
                View closeTabView = getActivity().findViewById(closeTabId);
                if (closeTabView == null) {
                    return false;
                }
                return true;
            }
        }, MAX_WAIT_MS);
        if (!success) {
            mAsserter.ok(success != false, "waiting for close tab view", "close tab view available");
        }

        // close tabs until only one remains
        tabCountText = tabCount.getText();
        tabCountInt = Integer.parseInt(tabCountText);
        while (tabCountInt > 1) {
            clicked = closeTab.click();
            if (!clicked) {
                mAsserter.ok(clicked != false, "checking that close_tab clicked", "close_tab element clicked");
            }

            success = waitForCondition(new Condition() {
                @Override
                public boolean isSatisfied() {
                    String newTabCountText = tabCount.getText();
                    int newTabCount = Integer.parseInt(newTabCountText);
                    if (newTabCount < tabCountInt) {
                        tabCountInt = newTabCount;
                        return true;
                    }
                    return false;
                }
            }, MAX_WAIT_MS);
            mAsserter.ok(success, "Checking tab closed", "number of tabs now "+tabCountInt);
        }
    }

    private void getTabCount(final int expected) {
        waitForCondition(new Condition() {
            @Override
            public boolean isSatisfied() {
                String newTabCountText = tabCount.getText();
                tabCountInt = Integer.parseInt(newTabCountText);
                if (tabCountInt == expected) {
                    return true;
                }
                return false;
            }
        }, MAX_WAIT_MS);
    }
}

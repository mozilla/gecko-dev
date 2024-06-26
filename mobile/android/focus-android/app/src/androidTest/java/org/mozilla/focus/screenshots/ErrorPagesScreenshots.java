/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.screenshots;

import android.os.Build;

import androidx.test.espresso.web.webdriver.Locator;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiSelector;

import org.junit.ClassRule;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.R;
import org.mozilla.focus.helpers.TestHelper;

import tools.fastlane.screengrab.Screengrab;
import tools.fastlane.screengrab.locale.LocaleTestRule;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasFocus;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.web.sugar.Web.onWebView;
import static androidx.test.espresso.web.webdriver.DriverAtoms.findElement;
import static androidx.test.espresso.web.webdriver.DriverAtoms.webClick;
import static androidx.test.espresso.web.webdriver.DriverAtoms.webScrollIntoView;
import static junit.framework.Assert.assertTrue;

@Ignore("See: https://github.com/mozilla-mobile/mobile-test-eng/issues/305")
@RunWith(AndroidJUnit4.class)
public class ErrorPagesScreenshots extends ScreenshotTest {

    @ClassRule
    public static final LocaleTestRule localeTestRule = new LocaleTestRule();

    private enum ErrorTypes {
        ERROR_UNKNOWN (-1),
        ERROR_HOST_LOOKUP (-2),
        ERROR_CONNECT (-6),
        ERROR_TIMEOUT (-8),
        ERROR_REDIRECT_LOOP (-9),
        ERROR_UNSUPPORTED_SCHEME (-10),
        ERROR_FAILED_SSL_HANDSHAKE (-11),
        ERROR_BAD_URL (-12),
        ERROR_TOO_MANY_REQUESTS (-15);
        private int value;

        ErrorTypes(int value) {
            this.value = value;
        }
    }

    @Test
    public void takeScreenshotsOfErrorPages() {
        for (ErrorTypes error: ErrorTypes.values()) {
            onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
                    .check(matches(isDisplayed()))
                    .check(matches(hasFocus()))
                    .perform(click(), replaceText("error:" + error.value), pressImeActionButton());

            assertTrue(TestHelper.webView.waitForExists(waitingTime));
            assertTrue(TestHelper.progressBar.waitUntilGone(waitingTime));

            // Android O has an issue with using Locator.ID
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                UiObject tryAgainBtn = device.findObject(new UiSelector()
                        .resourceId("errorTryAgain")
                        .clickable(true));
                assertTrue(tryAgainBtn.waitForExists(waitingTime));
            } else {
                onWebView()
                        .withElement(findElement(Locator.ID, "errorTitle"))
                        .perform(webClick());

                onWebView()
                        .withElement(findElement(Locator.ID, "errorTryAgain"))
                        .perform(webScrollIntoView());
            }

            Screengrab.screenshot(error.name());

            onView(withId(R.id.mozac_browser_toolbar_edit_url_view))
                    .check(matches(isDisplayed()))
                    .perform(click());
        }
    }
}

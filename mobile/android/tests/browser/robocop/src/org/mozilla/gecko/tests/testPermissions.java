/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import java.util.ArrayList;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.PaintedSurface;

import android.widget.CheckBox;

public class testPermissions extends PixelTest {
    public void testPermissions() {
        blockForGeckoReady();

        geolocationTest();
    }

    private void geolocationTest() {
        Actions.RepeatedEventExpecter paintExpecter;

        // Test geolocation notification
        loadAndPaint(getAbsoluteUrl(mStringHelper.ROBOCOP_GEOLOCATION_URL));
        waitForText("wants your location");

        // Uncheck the "Don't ask again for this site" checkbox
        ArrayList<CheckBox> checkBoxes = mSolo.getCurrentViews(CheckBox.class);
        mAsserter.ok(checkBoxes.size() == 1, "checkbox count", "only one checkbox visible");
        mAsserter.ok(mSolo.isCheckBoxChecked(0), "checkbox checked", "checkbox is checked");
        mSolo.clickOnCheckBox(0);
        mAsserter.ok(!mSolo.isCheckBoxChecked(0), "checkbox not checked", "checkbox is not checked");

        // Test "Share" button functionality with unchecked checkbox
        paintExpecter = mActions.expectPaint();
        mSolo.clickOnText("Share");
        PaintedSurface painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            mAsserter.ispixel(painted.getPixelAt(10, 10), 0, 0x80, 0, "checking page background is green");
        } finally {
            painted.close();
        }

        // Re-trigger geolocation notification
        reloadAndPaint();
        waitForText("wants your location");

        // Make sure the checkbox is checked this time
        mAsserter.ok(mSolo.isCheckBoxChecked(0), "checkbox checked", "checkbox is checked");

        // Test "Share" button functionality with checked checkbox
        paintExpecter = mActions.expectPaint();
        mSolo.clickOnText("Share");
        painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            mAsserter.ispixel(painted.getPixelAt(10, 10), 0, 0x80, 0, "checking page background is green");
        } finally {
            painted.close();
        }

        // When we reload the page, location should be automatically shared
        painted = reloadAndGetPainted();
        try {
            mAsserter.ispixel(painted.getPixelAt(10, 10), 0, 0x80, 0, "checking page background is green");
        } finally {
            painted.close();
        }
    }
}

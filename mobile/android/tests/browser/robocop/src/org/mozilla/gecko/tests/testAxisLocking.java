/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.Actions;
import org.mozilla.gecko.PaintedSurface;

/**
 * Basic test for axis locking behaviour.
 * - Load page and verify it draws
 * - Drag page upwards 100 pixels at a 5-degree angle off the vertical axis
 * - Verify that the 5-degree angle was thrown out and it dragged vertically
 * - Drag page upwards at a 45-degree angle
 * - Verify that the 45-degree angle was not thrown out and it dragged diagonally
 */
public class testAxisLocking extends PixelTest {
    public void testAxisLocking() {
        String url = getAbsoluteUrl(mStringHelper.ROBOCOP_BOXES_URL);

        MotionEventHelper meh = new MotionEventHelper(getInstrumentation(), mDriver.getGeckoLeft(), mDriver.getGeckoTop());

        blockForGeckoReady();

        // load page and check we're at 0,0
        loadAndVerifyBoxes(url);

        // drag page upwards by 100 pixels with a slight angle. verify that
        // axis locking prevents any horizontal scrolling
        Actions.RepeatedEventExpecter paintExpecter = mActions.expectPaint();
        meh.dragSync(20, 150, 10, 50);
        PaintedSurface painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 0, 100);
            // since checkScrollWithBoxes only checks 4 points, it may not pick up a
            // sub-100 pixel horizontal shift. so we check another point manually to make sure.
            int[] color = getBoxColorAt(0, 100);
            mAsserter.ispixel(painted.getPixelAt(99, 0), color[0], color[1], color[2], "Pixel at 99, 0 indicates no horizontal scroll");

            // now drag at a 45-degree angle to ensure we break the axis lock, and
            // verify that we have both horizontal and vertical scrolling
            paintExpecter = mActions.expectPaint();
            meh.dragSync(150, 150, 50, 50);
        } finally {
            painted.close();
        }

        painted = waitForPaint(paintExpecter);
        paintExpecter.unregisterListener();
        try {
            checkScrollWithBoxes(painted, 100, 200);
        } finally {
            painted.close();
        }
    }
}

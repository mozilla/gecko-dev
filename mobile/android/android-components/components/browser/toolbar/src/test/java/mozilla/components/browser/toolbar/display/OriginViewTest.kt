/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class OriginViewTest {
    @Test
    fun `URL text direction is always LTR`() {
        val originView = OriginView(testContext)
        originView.url = "ختار.ار/www.mozilla.org/1"
        assertEquals(View.TEXT_DIRECTION_LTR, originView.urlView.textDirection)
        assertEquals(View.LAYOUT_DIRECTION_LTR, originView.urlView.layoutDirection)
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.util

import android.util.DisplayMetrics
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import kotlin.math.roundToInt

@RunWith(AndroidJUnit4::class)
class DisplayMetricsTest {

    @Test
    fun `dp returns same value as manual conversion`() {
        val metrics = DisplayMetrics().apply {
            density = 2.75f
        }

        for (i in 1..10) {
            val expected = (i * 2.75).roundToInt()

            assertEquals(expected, i.dpToPx(metrics))
        }
    }
}

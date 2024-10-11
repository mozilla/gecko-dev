/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import mozilla.components.browser.engine.fission.GeckoWebContentIsolationMapper.intoWebContentIsolationStrategy
import mozilla.components.concept.engine.fission.WebContentIsolationStrategy
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoRuntimeSettings.STRATEGY_ISOLATE_EVERYTHING
import org.mozilla.geckoview.GeckoRuntimeSettings.STRATEGY_ISOLATE_HIGH_VALUE
import org.mozilla.geckoview.GeckoRuntimeSettings.STRATEGY_ISOLATE_NOTHING

@RunWith(AndroidJUnit4::class)
class GeckoWebContentIsolationMapperTest {
    @Test
    fun `WHEN given an Int WebContentIsolationStrategy THEN map to the corresponding AC WebContentIsolationStrategy`() {
        assertEquals(
            WebContentIsolationStrategy.ISOLATE_NOTHING,
            STRATEGY_ISOLATE_NOTHING.intoWebContentIsolationStrategy(),
        )
        assertEquals(
            WebContentIsolationStrategy.ISOLATE_EVERYTHING,
            STRATEGY_ISOLATE_EVERYTHING.intoWebContentIsolationStrategy(),
        )
        assertEquals(
            WebContentIsolationStrategy.ISOLATE_HIGH_VALUE,
            STRATEGY_ISOLATE_HIGH_VALUE.intoWebContentIsolationStrategy(),
        )
        assertEquals(
            WebContentIsolationStrategy.ISOLATE_HIGH_VALUE,
            8.intoWebContentIsolationStrategy(),

        )
    }

    @Test
    fun `WHEN given an AC WebContentIsolationStrategy THEN map to the correct Int`() {
        assertEquals(
            0,
            WebContentIsolationStrategy.ISOLATE_NOTHING.intoWebContentIsolationStrategy(),
        )
        assertEquals(
            1,
            WebContentIsolationStrategy.ISOLATE_EVERYTHING.intoWebContentIsolationStrategy(),
        )
        assertEquals(
            2,
            WebContentIsolationStrategy.ISOLATE_HIGH_VALUE.intoWebContentIsolationStrategy(),
        )
    }

    @Test
    fun `WHEN given an AC WebContentIsolationStrategy THEN check the correct enum ordinal and strategy`() {
        assertEquals(
            0,
            WebContentIsolationStrategy.ISOLATE_NOTHING.strategy,
        )
        assertEquals(
            WebContentIsolationStrategy.ISOLATE_NOTHING.ordinal,
            WebContentIsolationStrategy.ISOLATE_NOTHING.strategy,
        )
        assertEquals(
            1,
            WebContentIsolationStrategy.ISOLATE_EVERYTHING.strategy,
        )
        assertEquals(
            WebContentIsolationStrategy.ISOLATE_EVERYTHING.ordinal,
            WebContentIsolationStrategy.ISOLATE_EVERYTHING.strategy,
        )
        assertEquals(
            2,
            WebContentIsolationStrategy.ISOLATE_HIGH_VALUE.strategy,
        )
        assertEquals(
            WebContentIsolationStrategy.ISOLATE_HIGH_VALUE.ordinal,
            WebContentIsolationStrategy.ISOLATE_HIGH_VALUE.strategy,
        )
    }
}

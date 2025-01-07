/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.fission

import org.mozilla.geckoview.GeckoRuntimeSettings.STRATEGY_ISOLATE_EVERYTHING
import org.mozilla.geckoview.GeckoRuntimeSettings.STRATEGY_ISOLATE_HIGH_VALUE
import org.mozilla.geckoview.GeckoRuntimeSettings.STRATEGY_ISOLATE_NOTHING
import mozilla.components.concept.engine.fission.WebContentIsolationStrategy as EngineWebContentIsolationStrategy
import org.mozilla.geckoview.GeckoRuntimeSettings.WebContentIsolationStrategy as GeckoWebContentIsolationStrategy

/**
* Utility file for mapping functions related to the Gecko implementation of web content isolation strategy.
*/
object GeckoWebContentIsolationMapper {

    /**
     * Convenience method for mapping a GeckoView [GeckoWebContentIsolationStrategy],
     * which is an Int, to the Android Components defined type of [EngineWebContentIsolationStrategy].
     *
     * @return The corresponding [EngineWebContentIsolationStrategy] or else will return
     * [EngineWebContentIsolationStrategy.ISOLATE_HIGH_VALUE] as a reasonable default.
     */
    fun Int.intoWebContentIsolationStrategy(): EngineWebContentIsolationStrategy {
        return when (this) {
            STRATEGY_ISOLATE_NOTHING ->
                EngineWebContentIsolationStrategy.ISOLATE_NOTHING
            STRATEGY_ISOLATE_EVERYTHING ->
                EngineWebContentIsolationStrategy.ISOLATE_EVERYTHING
            STRATEGY_ISOLATE_HIGH_VALUE ->
                EngineWebContentIsolationStrategy.ISOLATE_HIGH_VALUE
            else -> {
                EngineWebContentIsolationStrategy.ISOLATE_HIGH_VALUE
            }
        }
    }

    /**
     * Convenience method for mapping a Android Components defined [EngineWebContentIsolationStrategy],
     * to the GeckoView [GeckoWebContentIsolationStrategy], which is an int.
     *
     * @return The corresponding [GeckoWebContentIsolationStrategy].
     */
    fun EngineWebContentIsolationStrategy.intoWebContentIsolationStrategy(): Int {
        return when (this) {
            EngineWebContentIsolationStrategy.ISOLATE_NOTHING ->
                STRATEGY_ISOLATE_NOTHING
            EngineWebContentIsolationStrategy.ISOLATE_EVERYTHING ->
                STRATEGY_ISOLATE_EVERYTHING
            EngineWebContentIsolationStrategy.ISOLATE_HIGH_VALUE ->
                STRATEGY_ISOLATE_HIGH_VALUE
        }
    }
}

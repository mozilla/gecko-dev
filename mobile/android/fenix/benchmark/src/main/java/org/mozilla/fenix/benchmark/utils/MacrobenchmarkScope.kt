/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.benchmark.utils

import androidx.benchmark.macro.MacrobenchmarkScope

fun MacrobenchmarkScope.isFirstIteration(benchmarking: Boolean) : Boolean {
    // Benchmarking starts at iteration 0 while baseline profile generation starts at iteration 1
    return if (benchmarking && iteration == 0) {
        true
    } else if (!benchmarking && iteration == 1) {
        true
    } else {
        false
    }
}

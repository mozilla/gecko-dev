/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.benchmark.baselineprofile

/**
 * A custom annotation to to identify Baseline Profile Generator tests.
 * All BaselineProfileGenerator tests can be run in a flank configuration with:
 *   test-targets:
 *     - annotation org.mozilla.fenix.benchmark.baselineprofile.BaselineProfileGenerator
 *
 * Please remember to update [flank-arm64-v8a-baseline-profile.yml](https://searchfox.org/mozilla-central/source/mobile/android/fenix/automation/taskcluster/androidTest/flank-arm64-v8a-baseline-profile.yml)
 * and any other use of this annotation if its name or package is changed.
 */
annotation class BaselineProfileGenerator

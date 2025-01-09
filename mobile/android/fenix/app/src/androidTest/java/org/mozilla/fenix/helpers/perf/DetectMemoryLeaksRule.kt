/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers.perf

import android.util.Log
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.platform.app.InstrumentationRegistry
import leakcanary.AppWatcher
import leakcanary.LeakAssertions
import leakcanary.TestDescriptionHolder
import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement
import org.mozilla.fenix.customannotations.DetectLeaks
import org.mozilla.fenix.helpers.Constants.TAG

/**
 * Junit [TestRule] to detect memory leaks when a test is annotated with
 * [DetectLeaks] annotation. It can also optionally ignore annotations and run the memory
 * leak checks on all the tests in the suite.
 *
 * When the test suite uses the [ActivityScenarioRule], the order of applying
 * the [DetectMemoryLeaksRule] is important. The [ActivityScenarioRule] finishes the activity at the
 * end of each test, so, in order to detect memory leaks in the activity, this test rule has to be
 * applied after the activity scenario rule, so that it can detect leaks after the activity
 * has been destroyed.
 *
 * See [https://square.github.io/leakcanary/ui-tests/#test-rule-chains](https://square.github.io/leakcanary/ui-tests/#test-rule-chains)
 * for more.
 *
 * Sample usage:
 *
 * ```kotlin
 * class MyFeatureTest {
 *
 *   @get:Rule
 *   val memoryLeaksRule = DetectMemoryLeaksRule()
 *
 *   @Test
 *   @DetectLeaks
 *   fun testMyFeature() {
 *     // test body
 *   }
 * }
 * ```
 *
 * @property tag Tag used to identify the calling code
 * @property assertLeaksIgnoringRunnerArgs Flag used to always "assert no leaks" regardless of
 * whether leak detection is enabled in the test runner or not. This value defaults to
 * "false" because we don't want to always detect memory leaks while running tests. For debugging,
 * you can enable the checks locally, by temporarily setting it to "true"
 */
class DetectMemoryLeaksRule(
    private val tag: String = DetectMemoryLeaksRule::class.java.simpleName,
    private val assertLeaksIgnoringRunnerArgs: Boolean = false,
) : TestRule {

    override fun apply(base: Statement, description: Description): Statement {
        val checkMemoryLeaks = leakDetectionEnabled(description)
        return if (checkMemoryLeaks) {
            TestDescriptionHolder.wrap(
                object : Statement() {
                    override fun evaluate() {
                        try {
                            base.evaluate()
                            LeakAssertions.assertNoLeaks(tag)
                        } finally {
                            AppWatcher.objectWatcher.clearWatchedObjects()
                        }
                    }
                },
                description,
            )
        } else {
            object : Statement() {
                override fun evaluate() {
                    base.evaluate()
                }
            }
        }
    }

    private fun Description.hasDetectLeaksAnnotation(): Boolean {
        val testClassAnnotated = testClass?.annotations?.any { it is DetectLeaks } ?: false
        val testMethodAnnotated = annotations.any { it is DetectLeaks }
        return testMethodAnnotated || testClassAnnotated
    }

    private fun hasDetectLeaksTestRunnerArg(): Boolean {
        val args = try {
            InstrumentationRegistry.getArguments()
        } catch (exception: IllegalStateException) {
            Log.e(TAG, "No instrumentation arguments registered", exception)
            null
        }

        return args?.getString(ARG_DETECT_LEAKS, "false") == "true"
    }

    /**
     * Determines whether or not leak detection is enabled
     *
     * @return true if the test is annotated with @DetectLeaks AND either the runner has the
     * "detect-leak" argument set to "true", or [assertLeaksIgnoringRunnerArgs] flag is true.
     */
    private fun leakDetectionEnabled(description: Description): Boolean {
        return description.hasDetectLeaksAnnotation() &&
            (assertLeaksIgnoringRunnerArgs || hasDetectLeaksTestRunnerArg())
    }

    private companion object {
        /**
         * Key identifying the test instrumentation runner argument to enable or disable
         * memory leak detection.
         */
        const val ARG_DETECT_LEAKS = "detect-leaks"
    }
}

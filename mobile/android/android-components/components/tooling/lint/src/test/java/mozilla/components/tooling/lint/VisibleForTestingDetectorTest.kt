/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.checks.infrastructure.TestMode
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class VisibleForTestingDetectorTest : LintDetectorTest() {
    override fun getDetector(): Detector? {
        return VisibleForTestingDetector()
    }

    override fun getIssues(): List<Issue> = listOf(
        VisibleForTestingDetector.ISSUE_VISIBLE_FOR_TESTING_ANNOTATION,
    )

    private val androidXAnnotationStub = kotlin(
        """
            package androidx.annotation

            @Target(AnnotationTarget.CLASS, AnnotationTarget.FUNCTION, AnnotationTarget.PROPERTY)
            @Retention(AnnotationRetention.RUNTIME)
            annotation class VisibleForTesting
        """.trimIndent(),
    )

    private val jetBrainsAnnotationStub = kotlin(
        """
            package org.jetbrains.annotations

            @Target(AnnotationTarget.CLASS, AnnotationTarget.FUNCTION, AnnotationTarget.PROPERTY)
            @Retention(AnnotationRetention.RUNTIME)
            annotation class VisibleForTesting
        """.trimIndent(),
    )

    private val randomAnnotationStub = kotlin(
        """
            package org.random.annotations

            @Target(AnnotationTarget.CLASS, AnnotationTarget.FUNCTION, AnnotationTarget.PROPERTY)
            @Retention(AnnotationRetention.RUNTIME)
            annotation class VisibleForTesting
        """.trimIndent(),
    ).indented()

    @Test
    fun `test valid VisibleForTesting usage`() {
        val validCode = """
            package mozilla.components.tooling.lint

            import androidx.annotation.VisibleForTesting

            class MyClass {
                @VisibleForTesting
                fun myMethod() {}
            }
        """.trimIndent()

        lint()
            .allowMissingSdk()
            .files(
                kotlin(validCode),
                androidXAnnotationStub,
            )
            .run()
            .expectClean()
    }

    @Test
    fun `test invalid JetBrains VisibleForTesting usage`() {
        val invalidCode = """
            package mozilla.components.tooling.lint

            import org.jetbrains.annotations.VisibleForTesting

            class MyClass {
                @VisibleForTesting
                fun myMethod() {}
            }
        """.trimIndent()

        lint()
            .allowMissingSdk()
            .skipTestModes(TestMode.IMPORT_ALIAS)
            .files(
                kotlin(invalidCode),
                jetBrainsAnnotationStub,
            )
            .run()
            .expect(
                """
src/mozilla/components/tooling/lint/MyClass.kt:3: Error: Invalid @VisibleForTesting annotation usage. Found org.jetbrains.annotations.VisibleForTesting. Please use androidx.annotation.VisibleForTesting instead. [VisibleForTestingAnnotation]
import org.jetbrains.annotations.VisibleForTesting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
                """.trimIndent(),
            )
    }

    @Test
    fun `test random other package VisibleForTesting usage`() {
        val invalidCode = """
            package mozilla.components.tooling.lint

            import org.random.annotations.VisibleForTesting

            class MyClass {
                @VisibleForTesting
                fun myMethod() {}
            }
        """.trimIndent()

        lint()
            .allowMissingSdk()
            .skipTestModes(TestMode.IMPORT_ALIAS)
            .files(
                kotlin(invalidCode),
                randomAnnotationStub,
            )
            .run()
            .expect(
                """
src/mozilla/components/tooling/lint/MyClass.kt:3: Error: Invalid @VisibleForTesting annotation usage. Found org.random.annotations.VisibleForTesting. Please use androidx.annotation.VisibleForTesting instead. [VisibleForTestingAnnotation]
import org.random.annotations.VisibleForTesting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
                """.trimIndent(),
            )
    }
}

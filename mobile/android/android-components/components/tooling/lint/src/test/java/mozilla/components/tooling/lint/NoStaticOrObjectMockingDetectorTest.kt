/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.checks.infrastructure.TestFiles
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class NoStaticOrObjectMockingDetectorTest : LintDetectorTest() {
    override fun getDetector(): Detector {
        return NoStaticOrObjectMockingDetector()
    }

    override fun getIssues(): List<Issue> = listOf(
        NoStaticOrObjectMockingDetector.ISSUE_NO_STATIC_MOCKING,
        NoStaticOrObjectMockingDetector.ISSUE_NO_OBJECT_MOCKING,
    )

    private val mockkStubs = TestFiles.kotlin(
        """
        package io.mockk

        annotation class JvmStatic

        fun <T> mockkStatic(vararg kClasses: Any) { /* stub */ }
        fun unmockkStatic(vararg kClasses: Any) { /* stub */ }
        fun clearStaticMock(vararg functions: KProperty<*>) { /* stub */ }

        fun <T> mockkObject(vararg objects: Any, recordPrivateCalls: Boolean = false) { /* stub */ }
        fun unmockkObject(vararg objects: Any) { /* stub */ }
        fun clearObjectMock(vararg objects: Any) { /* stub */ }

        fun <T> every(block: () -> T?): MockKStubScope<T?, T?> = MockKStubScope()
        fun <T> any(): T = null as T

        class MockKStubScope<T, R> {
            infix fun returns(value: R) { /* stub */ }
        }
        """,
    ).indented()

    private val mockitoStubs = TestFiles.kotlin(
        """
        package org.mockito

        object Mockito {
            @JvmStatic
            fun <T> mockStatic(classToMock: Class<T>?): StaticMockControl<T> { TODO() }
            // Add more overloads if needed, or simplify the return type if not crucial

            @JvmStatic
            fun <T> mockConstruction(classToMock: Class<T>?): ConstructionMockControl<T> { TODO() }
            // Add more overloads if needed

            // Stubs for control objects if their methods are ever relevant to your lint checks
            // For this detector, they likely aren't, but good for completeness if expanding.
        }
        interface StaticMockControl<T>
        interface ConstructionMockControl<T>

        """,
    ).indented()

    private val logStub = TestFiles.kotlin(
        """
        package android.util

        object Log {
            @JvmStatic fun d(tag: String, msg: String): Int = 0
            @JvmStatic fun w(tag: String, msg: String): Int = 0
            @JvmStatic fun d(tag: String, msg: String, tr: Throwable): Int = 0
        }
        """,
    ).indented()

    private val singletonObjectStub = TestFiles.kotlin(
        """
        package com.example.utils

        object MySingleton {
            fun doSomething() {}
            fun doSomethingElse(): String = "hello"
        }
        """,
    ).indented()

    // --- Test Files with violations ---

    private val mockkStaticUsage = TestFiles.kotlin(
        """
        package com.example.test

        import io.mockk.every
        import io.mockk.mockkStatic
        import android.util.Log

        class MyMockkStaticTest {
            init {
                mockkStatic(Log::class) // VIOLATION
                every { Log.d(any(), any()) } returns 0
            }
        }
        """,
    ).indented()

    private val unmockkStaticUsage = TestFiles.kotlin(
        """
        package com.example.test
        import io.mockk.unmockkStatic
        import android.util.Log
        class MyUnmockkStaticTest {
            fun cleanup() {
                unmockkStatic(Log::class) // VIOLATION
            }
        }
        """,
    ).indented()

    private val clearStaticMockUsage = TestFiles.kotlin(
        """
        package com.example.test
        import io.mockk.clearStaticMock
        import android.util.Log
        class MyClearStaticMockTest {
            fun cleanup() {
                clearStaticMock(Log::class) // VIOLATION
            }
        }
        """,
    ).indented()

    private val mockkObjectUsage = TestFiles.kotlin(
        """
        package com.example.test
        import io.mockk.mockkObject
        import io.mockk.every
        import com.example.utils.MySingleton // Needs singletonObjectStub

        class MyMockkObjectTest {
            fun setup() {
                mockkObject(MySingleton) // VIOLATION
                every { MySingleton.doSomethingElse() } returns "mocked"
            }
        }
        """,
    ).indented()

    private val unmockkObjectUsage = TestFiles.kotlin(
        """
        package com.example.test
        import io.mockk.unmockkObject
        import com.example.utils.MySingleton // Needs singletonObjectStub

        class MyUnmockkObjectTest {
            fun cleanup() {
                unmockkObject(MySingleton) // VIOLATION
            }
        }
        """,
    ).indented()

    private val clearObjectMockUsage = TestFiles.kotlin(
        """
        package com.example.test
        import io.mockk.clearObjectMock
        import com.example.utils.MySingleton // Needs singletonObjectStub

        class MyClearObjectMockTest {
            fun cleanup() {
                clearObjectMock(MySingleton) // VIOLATION
            }
        }
        """,
    ).indented()

    private val mockitoMockStaticUsage = TestFiles.kotlin(
        """
        package com.example.test
        import org.mockito.Mockito
        import android.util.Log

        class MyMockitoStaticTest {
            fun setup() {
                // val logMock = Mockito.mockStatic(Log::class.java) // VIOLATION
                // For some reason, lint type resolution with ::class.java can be tricky
                // in test files. Using a direct Class object often works better for stubs.
                Mockito.mockStatic(android.util.Log::class.java) // VIOLATION
            }
        }
        """,
    ).indented()

    private val mockitoMockConstructionUsage = TestFiles.kotlin(
        """
        package com.example.test
        import org.mockito.Mockito

        class SomeDependency {
            fun getValue() = "real"
        }
        class MyMockitoConstructorTest {
            fun setup() {
                Mockito.mockConstruction(SomeDependency::class.java) // VIOLATION
            }
        }
        """,
    ).indented()

    // --- Test File with NO violations ---
    private val acceptableMockingUsage = TestFiles.kotlin(
        """
        package com.example.test

        import io.mockk.every

        interface MyLogger {
            fun log(message: String)
        }

        class MyService(private val logger: MyLogger) {
            fun performAction() {
                logger.log("Action performed")
            }
        }

        class StandardMockingTest {
            fun test() {
                val mockLogger = mockk<MyLogger>() // OK - instance mock
                every { mockLogger.log(any()) } returns Unit
                val service = MyService(mockLogger)
                service.performAction()
            }
        }
        """,
    ).indented()

    @Test
    fun `mockkStatic usage reports NoStaticMocking`() {
        lint()
            .allowMissingSdk() // For Log, but logStub is safer
            .files(mockkStubs, logStub, mockkStaticUsage)
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of MockK's 'mockkStatic' for static mocking is discouraged.")
    }

    @Test
    fun `unmockkStatic usage reports NoStaticMocking`() {
        lint()
            .allowMissingSdk()
            .files(mockkStubs, logStub, unmockkStaticUsage)
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of MockK's 'unmockkStatic' for static mocking is discouraged.")
    }

    @Test
    fun `clearStaticMock usage reports NoStaticMocking`() {
        lint()
            .allowMissingSdk()
            .files(mockkStubs, logStub, clearStaticMockUsage)
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of MockK's 'clearStaticMock' for static mocking is discouraged.")
    }

    @Test
    fun `mockkObject usage reports NoObjectMocking`() {
        lint()
            .allowMissingSdk()
            .files(mockkStubs, singletonObjectStub, mockkObjectUsage)
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of MockK's 'mockkObject' for object/singleton mocking is discouraged.")
    }

    @Test
    fun `unmockkObject usage reports NoObjectMocking`() {
        lint()
            .allowMissingSdk()
            .files(mockkStubs, singletonObjectStub, unmockkObjectUsage)
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of MockK's 'unmockkObject' for object/singleton mocking is discouraged.")
    }

    @Test
    fun `clearObjectMock usage reports NoObjectMocking`() {
        lint()
            .allowMissingSdk()
            .files(mockkStubs, singletonObjectStub, clearObjectMockUsage)
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of MockK's 'clearObjectMock' for object/singleton mocking is discouraged.")
    }

    @Test
    fun `Mockito mockStatic usage reports NoStaticMocking`() {
        lint()
            .allowMissingSdk()
            .files(mockitoStubs, logStub, mockitoMockStaticUsage)
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of Mockito's 'mockStatic' for static/constructor mocking is discouraged.")
    }

    @Test
    fun `Mockito mockConstruction usage reports NoStaticMocking`() {
        lint()
            .allowMissingSdk()
            .files(
                mockitoStubs,
                TestFiles.kotlin(
                    """
                    package com.example.test
                    import org.mockito.Mockito

                    class SomeDependency { // Define SomeDependency here for self-containment
                        fun getValue() = "real"
                    }
                    class MyMockitoConstructorTest {
                        fun setup() {
                            Mockito.mockConstruction(SomeDependency::class.java) // VIOLATION
                        }
                    }
                """,
                ).indented(),
            )
            .run()
            .expectErrorCount(1)
            .expectContains("Usage of Mockito's 'mockConstruction' for static/constructor mocking is discouraged.")
    }

    @Test
    fun `acceptable instance mocking does not report any issues`() {
        lint()
            .allowMissingSdk()
            .files(
                mockkStubs,
                acceptableMockingUsage,
            )
            .run()
            .expectClean() // Expects 0 errors and 0 warnings
    }
}

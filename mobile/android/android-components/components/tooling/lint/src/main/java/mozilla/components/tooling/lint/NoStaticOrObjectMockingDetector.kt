/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.intellij.psi.PsiMethod
import mozilla.components.tooling.lint.NoStaticOrObjectMockingDetector.Companion.ISSUE_NO_OBJECT_MOCKING
import mozilla.components.tooling.lint.NoStaticOrObjectMockingDetector.Companion.ISSUE_NO_STATIC_MOCKING
import org.jetbrains.uast.UCallExpression
import java.util.EnumSet

/**
 * Detects the usage of static and object (singleton) mocking in test code.
 *
 * This detector aims to discourage practices that can lead to brittle and hard-to-maintain tests.
 * Static mocking and object mocking often indicate a design that could be improved for better
 * testability, for example, by using dependency injection.
 *
 * It checks for specific methods from popular mocking libraries like MockK and Mockito:
 * - **MockK:** `mockkStatic`, `unmockkStatic`, `clearStaticMock`,
 *              `mockkObject`, `unmockkObject`, `clearObjectMock`
 * - **Mockito:** `mockStatic`, `mockConstruction`
 *
 * When such methods are detected, it reports an issue suggesting refactoring the code
 * to avoid these types of mocking.
 *
 * Corresponding issues: [ISSUE_NO_STATIC_MOCKING], [ISSUE_NO_OBJECT_MOCKING].
 */
class NoStaticOrObjectMockingDetector : Detector(), Detector.UastScanner {
    /**
     * Companion object holding the lint issue definitions and detector implementation details.
     */
    companion object {
        private val Implementation = Implementation(
            NoStaticOrObjectMockingDetector::class.java,
            EnumSet.of(Scope.JAVA_FILE, Scope.TEST_SOURCES),
        )

        @JvmField
        val ISSUE_NO_STATIC_MOCKING: Issue = Issue.create(
            id = "NoStaticMocking",
            briefDescription = "Avoid static mocking",
            explanation = """
                Static mocking can lead to tests that are harder to understand, maintain,
                and can have wider-reaching side effects.
                Prefer refactoring code to use dependency injection or other patterns
                that allow for easier testing without static mocks.
            """.trimIndent(),
            category = Category.CORRECTNESS,
            priority = 7,
            severity = Severity.ERROR,
            implementation = Implementation,
        )

        @JvmField
        val ISSUE_NO_OBJECT_MOCKING: Issue = Issue.create(
            id = "NoObjectMocking",
            briefDescription = "Avoid object (singleton) mocking",
            explanation = """
                Mocking objects/singletons can hide dependencies and make tests brittle.
                Consider providing dependencies explicitly or using interfaces for better testability.
            """.trimIndent(),
            category = Category.TESTING,
            priority = 7,
            severity = Severity.ERROR,
            implementation = Implementation,
        )
    }

    override fun getApplicableMethodNames(): List<String>? = listOf(
        "mockkStatic",
        "unmockkStatic",
        "clearStaticMock",
        "mockkObject",
        "unmockkObject",
        "clearObjectMock",
        "mockStatic",
        "mockConstruction",
    )

    override fun visitMethodCall(context: JavaContext, node: UCallExpression, method: PsiMethod) {
        val methodName = method.name
        val evaluator = context.evaluator
        val containingClass = method.containingClass
        val className = containingClass?.qualifiedName ?: ""
        val packageName = if (className.contains(".")) className.substringBeforeLast('.') else ""

        // --- MockK Checks ---
        if (packageName.startsWith("io.mockk")) {
            if (methodName == "mockkStatic" || methodName == "unmockkStatic" || methodName == "clearStaticMock") {
                context.report(
                    ISSUE_NO_STATIC_MOCKING,
                    node,
                    context.getLocation(node),
                    "Usage of MockK's '$methodName' for static mocking is discouraged. " +
                        "Refactor to use dependency injection.",
                )
                return
            }
            if (methodName == "mockkObject" || methodName == "unmockkObject" || methodName == "clearObjectMock") {
                context.report(
                    ISSUE_NO_OBJECT_MOCKING,
                    node,
                    context.getLocation(node),
                    "Usage of MockK's '$methodName' for object/singleton mocking is discouraged. " +
                        "Consider providing dependencies explicitly.",
                )
                return
            }
        }

        // --- Mockito Checks ---
        if (evaluator.isMemberInClass(method, "org.mockito.Mockito") &&
            (methodName == "mockStatic" || methodName == "mockConstruction")
        ) {
            context.report(
                ISSUE_NO_STATIC_MOCKING,
                node,
                context.getLocation(node),
                "Usage of Mockito's '$methodName' for static/constructor mocking is discouraged. " +
                    "Refactor for better testability.",
            )
            return
        }
    }
}

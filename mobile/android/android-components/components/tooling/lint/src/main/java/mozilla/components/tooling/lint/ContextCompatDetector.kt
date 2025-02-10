/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.LintFix
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.SourceCodeScanner
import com.intellij.psi.PsiMethod
import org.jetbrains.uast.UCallExpression

/**
 * A lint checker to notify when we should be using the Compat version of certain Android APIs.
 */
class ContextCompatDetector : Detector(), SourceCodeScanner {

    @Suppress("UndocumentedPublicClass")
    companion object {

        const val FULLY_QUALIFIED_CONTEXT_COMPAT = "androidx.core.content.ContextCompat"

        private val Implementation = Implementation(
            ContextCompatDetector::class.java,
            Scope.JAVA_FILE_SCOPE,
        )

        val ISSUE_GET_DRAWABLE_CALL = Issue.create(
            id = "UnsafeCompatGetDrawable",
            briefDescription = "Prohibits using the ContextCompat.getDrawable method",
            explanation = "Using this method can lead to crashes in older Android versions as newer features might " +
                "not be available",
            category = Category.CORRECTNESS,
            severity = Severity.ERROR,
            implementation = Implementation,
        )

        val ISSUE_GET_COLOR_STATE_LIST_CALL = Issue.create(
            id = "UnsafeCompatGetColorStateList",
            briefDescription = "Prohibits using the ContextCompat.getColorStateList method",
            explanation = "Using this method can lead to crashes in older Android versions as newer features might " +
                "not be available",
            category = Category.CORRECTNESS,
            severity = Severity.ERROR,
            implementation = Implementation,
        )

        val ISSUES = listOf(
            ISSUE_GET_DRAWABLE_CALL,
            ISSUE_GET_COLOR_STATE_LIST_CALL,
        )
    }

    override fun getApplicableMethodNames(): List<String> = listOf(
        "getDrawable",
        "getColorStateList",
    )

    override fun visitMethodCall(context: JavaContext, node: UCallExpression, method: PsiMethod) {
        val evaluator = context.evaluator

        if (!evaluator.isMemberInClass(method, FULLY_QUALIFIED_CONTEXT_COMPAT)) {
            return
        }

        when (node.methodName) {
            "getDrawable" -> reportGetDrawableCall(context, node)
            "getColorStateList" -> reportGetColorStateListCall(context, node)
        }
    }

    private fun reportGetDrawableCall(context: JavaContext, node: UCallExpression) = context.report(
        ISSUE_GET_DRAWABLE_CALL,
        context.getLocation(node),
        "This call can lead to crashes, replace with AppCompatResources.getDrawable",
        replaceUnsafeGetDrawableQuickFix(node),
    )

    private fun reportGetColorStateListCall(context: JavaContext, node: UCallExpression) =
        context.report(
            ISSUE_GET_COLOR_STATE_LIST_CALL,
            context.getLocation(node),
            "This call can lead to crashes, replace with AppCompatResources.getColorStateList",
            replaceUnsafeGetColorStateListCallQuickFix(node),
        )

    private fun replaceUnsafeGetDrawableQuickFix(node: UCallExpression): LintFix {
        val arguments = node.valueArguments.joinToString { it.asSourceString() }
        val newText = "AppCompatResources.getDrawable($arguments)"

        return LintFix.create()
            .name("Replace with AppCompatResources.getDrawable")
            .replace()
            .all()
            .with(newText)
            .build()
    }

    private fun replaceUnsafeGetColorStateListCallQuickFix(node: UCallExpression): LintFix {
        val arguments = node.valueArguments.joinToString { it.asSourceString() }
        val newText = "AppCompatResources.getColorStateList($arguments)"

        return LintFix.create()
            .name("Replace with AppCompatResources.getColorStateList")
            .replace()
            .all()
            .with(newText)
            .build()
    }
}

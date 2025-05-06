/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.client.api.UElementHandler
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UImportStatement

const val VISIBLE_FOR_TESTING_ANNOTATION = "VisibleForTesting"
const val ANDROIDX_VISIBLE_FOR_TESTING_PACKAGE = "androidx.annotation"

/**
 * Custom lint check that ensures only `androidx.annotation.VisibleForTesting` is used.
 */
class VisibleForTestingDetector : Detector(), Detector.UastScanner {

    override fun getApplicableUastTypes() = listOf(UImportStatement::class.java)
    override fun createUastHandler(context: JavaContext) = InvalidImportHandler(context)

    /**
     * [InvalidImportHandler] is a UAST (Unified Abstract Syntax Tree) element handler that checks for
     * invalid import statements related to the `@VisibleForTesting` annotation.
     *
     * It specifically looks for imports of `@VisibleForTesting` that are *not* from the
     * `androidx.annotation` package (`androidx.annotation.VisibleForTesting`).
     * If it finds such an import, it reports a lint issue, suggesting the use of the
     * `androidx` version.
     *
     * This class extends [UElementHandler] to hook into the UAST visitor framework and process
     * `UImportStatement` nodes.
     *
     * @property context The [JavaContext] used to report lint issues.
     */
    class InvalidImportHandler(private val context: JavaContext) : UElementHandler() {
        override fun visitImportStatement(node: UImportStatement) {
            node.importReference?.let { importReference ->
                val importFqName = importReference.asSourceString()

                if (importFqName.contains(VISIBLE_FOR_TESTING_ANNOTATION) &&
                    !importFqName.startsWith(ANDROIDX_VISIBLE_FOR_TESTING_PACKAGE)
                ) {
                    reportUsage(context, node, importFqName)
                }
            }
        }

        private fun reportUsage(context: JavaContext, element: UElement, importSource: String) {
            context.report(
                ISSUE_VISIBLE_FOR_TESTING_ANNOTATION,
                element,
                context.getLocation(element),
                "Invalid @VisibleForTesting annotation usage. Found $importSource. " +
                    "Please use androidx.annotation.VisibleForTesting instead.".trimIndent(),
            )
        }
    }

    /**
     * Define the scope and detector class for the issue.
     */
    companion object {
        private val IMPLEMENTATION = Implementation(
            VisibleForTestingDetector::class.java,
            Scope.JAVA_FILE_SCOPE,
        )

        val ISSUE_VISIBLE_FOR_TESTING_ANNOTATION: Issue = Issue.create(
            id = "VisibleForTestingAnnotation",
            briefDescription = "Invalid @VisibleForTesting Annotation Usage",
            explanation = "The @VisibleForTesting annotation must be imported from androidx.annotation." +
                "Other packages such as org.jetbrains.annotations or com.google.common.annotations are not allowed." +
                "Please replace the import with androidx.annotation.VisibleForTesting.".trimIndent(),
            category = Category.CORRECTNESS,
            priority = 6,
            severity = Severity.ERROR,
            implementation = IMPLEMENTATION,
        )
    }
}

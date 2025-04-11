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
import com.android.tools.lint.detector.api.SourceCodeScanner
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UFile

class LicenseDetector : Detector(), SourceCodeScanner {

    companion object {

        private val Implementation = Implementation(
            LicenseDetector::class.java,
            Scope.JAVA_FILE_SCOPE,
        )

        val ISSUE_MISSING_LICENSE = Issue.create(
            id = "MissingLicense",
            briefDescription = "File doesn't start with the license comment",
            explanation = "Every file must start with the license comment:\n" +
                LicenseCommentChecker.ValidLicenseForKotlinFiles,
            category = Category.CORRECTNESS,
            severity = Severity.WARNING,
            implementation = Implementation,
        )

        val ISSUE_INVALID_LICENSE_FORMAT = Issue.create(
            id = "AbsentOrWrongFileLicense",
            briefDescription = "License isn't formatted correctly",
            explanation = "The license must be:\n${LicenseCommentChecker.ValidLicenseForKotlinFiles}",
            category = Category.CORRECTNESS,
            severity = Severity.WARNING,
            implementation = Implementation,
        )
    }

    override fun getApplicableUastTypes(): List<Class<out UElement>>? = listOf(UFile::class.java)

    override fun createUastHandler(context: JavaContext): UElementHandler? =
        LicenseCommentChecker(context)
}

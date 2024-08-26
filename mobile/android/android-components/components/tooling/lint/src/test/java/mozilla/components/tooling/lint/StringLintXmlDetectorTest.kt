/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class StringLintXmlDetectorTest : LintDetectorTest() {
    override fun getDetector(): Detector = StringLintXmlDetector()

    override fun getIssues(): MutableList<Issue> =
        mutableListOf(
            StringLintXmlDetector.ISSUE_BLANK_STRING,
            StringLintXmlDetector.ISSUE_INCORRECT_ELLIPSIS,
            StringLintXmlDetector.ISSUE_STRAIGHT_QUOTE_USAGE,
            StringLintXmlDetector.ISSUE_STRAIGHT_DOUBLE_QUOTE_USAGE,
            StringLintXmlDetector.ISSUE_BRAND_USAGE,
        )

    @Test
    fun `GIVEN an empty string WHEN the string resource is linted THEN report a blank string error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'></string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:3: Error: mozac_tooling_lint_test_string is blank. [BlankString]
    <string name='mozac_tooling_lint_test_string'></string>
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
            """,
            )
    }

    @Test
    fun `GIVEN a blank string WHEN the string resource is linted THEN report a blank string error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'>   </string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:3: Error: mozac_tooling_lint_test_string is blank. [BlankString]
    <string name='mozac_tooling_lint_test_string'>   </string>
                                                  ~~~
1 errors, 0 warnings
            """,
            )
    }

    @Test
    fun `GIVEN string contains the incorrect ellipsis character WHEN the string resource is linted THEN report incorrect ellipsis character error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'>Share with...</string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:3: Error: Incorrect ellipsis character `...`. Use `…` instead. [IncorrectEllipsisCharacter]
    <string name='mozac_tooling_lint_test_string'>Share with...</string>
                                                  ~~~~~~~~~~~~~
1 errors, 0 warnings
            """,
            )
    }

    @Test
    fun `GIVEN string contains the incorrect straight quote character WHEN the string resource is linted THEN report incorrect straight quote character error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'>Couldn't load site</string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:3: Error: Incorrect straight quote character `'`. use `’` instead. [IncorrectStraightQuote]
    <string name='mozac_tooling_lint_test_string'>Couldn't load site</string>
                                                  ~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
            """,
            )
    }

    @Test
    fun `GIVEN string contains the incorrect straight double quote character WHEN the string resource is linted THEN report incorrect straight double quote character error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'>Select a language to manage "always translate" and "never translate" preferences.</string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:3: Error: Incorrect straight double quote character `"`. use `“”` instead. [IncorrectStraightDoubleQuote]
    <string name='mozac_tooling_lint_test_string'>Select a language to manage "always translate" and "never translate" preferences.</string>
                                                  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
            """,
            )
    }

    @Test
    fun `GIVEN a string containing CDATA with a brand name WHEN the string resource is linted THEN report a brand error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'><![CDATA[Logging in to Firefox]]></string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:3: Error: Hard-coded brand Firefox in string. Use a variable instead. [BrandUsage]
    <string name='mozac_tooling_lint_test_string'><![CDATA[Logging in to Firefox]]></string>
                                                           ~~~~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
            """,
            )
    }

    @Test
    fun `GIVEN a string containing a brand name WHEN the string resource is linted THEN report a brand error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'>Logging in to Firefox></string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:3: Error: Hard-coded brand Firefox in string. Use a variable instead. [BrandUsage]
    <string name='mozac_tooling_lint_test_string'>Logging in to Firefox></string>
                                                  ~~~~~~~~~~~~~~~~~~~~~~
1 errors, 0 warnings
            """,
            )
    }

    @Test
    fun `GIVEN various correct string resources WHEN the string resource is linted THEN report no errors`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'>Share with…</string>
    <string name='mozac_tooling_lint_test_string'>Couldn’t load site</string>
    <string name='mozac_tooling_lint_test_string'>Select a language to manage ”always translate“ and ”never translate“ preferences.</string>
    <string name='mozac_tooling_lint_test_string'><![CDATA[Logging in]]></string>
    <string name='mozac_tooling_lint_test_string'>Logging in></string>
</resources>
""",
                ),
            )
            .run()
            .expectClean()
    }
}

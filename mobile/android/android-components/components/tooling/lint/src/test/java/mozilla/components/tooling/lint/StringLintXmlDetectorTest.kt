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
            StringLintXmlDetector.ISSUE_PLACEHOLDER_COMMENT,
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

    @Test
    fun `GIVEN strings without placeholders WHEN the string resource is linted THEN report no errors`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <string name='mozac_tooling_lint_test_string'>Share with…</string>
</resources>
""",
                ),
            )
            .run()
            .expectClean()
    }

    @Test
    fun `GIVEN strings with placeholders and complete comments WHEN the string resource is linted THEN report no errors`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <!-- Title to show in alert when a lot of tabs are to be opened
    %d is a placeholder for the number of tabs that will be opened -->
    <string name="open_all_warning_title">Open %d tabs?</string>
    <!-- Content description (not visible, for screen readers etc.): Close tab <title> button. %s is tab title  -->
    <string name="close_tab_title">Close tab %s</string>
    <!-- Postfix for private WebApp titles, %1${'$'}s is replaced with app name -->
    <string name="pwa_site_controls_title_private">%1${'$'}s (Private Mode)</string>
    <!-- History multi select title in app bar
    %1${'$'}d is the number of bookmarks selected -->
    <string name="history_multi_select_title">%1${'$'}d selected</string>
    <!-- The content of the notification, this will be shown to the user when one newly supported add-on is available.
    %1${'$'}s is the add-on name and %2${'$'}s is the app name (in most cases Firefox). -->
    <string name="mozac_feature_addons_supported_checker_notification_content_one">Add %1${'$'}s to %2${'$'}s</string>
    <!-- Accessibility content description for the amount of stars that add-on has, where %1${'$'}.02f will be the amount of stars. -->
    <string name="mozac_feature_addons_rating_content_description_2">Rating: %1${'$'}.02f out of 5</string>
    <!-- %1${'$'}s will be replaced with the name of the app (e.g. "Firefox Focus"). -->
    <string name="errorpage_httpsonly_message2"><![CDATA[%1${'$'}s tries to use an HTTPS connection whenever possible for more security.]]></string>
</resources>
""",
                ),
            )
            .run()
            .expectClean()
    }

    @Test
    fun `GIVEN strings with placeholders and incomplete comments WHEN the string resource is linted THEN report a placeholder error`() {
        lint()
            .files(
                xml(
                    "res/values/string.xml",
                    """
<resources>
    <!-- Title to show in alert when a lot of tabs are to be opened. -->
    <string name="open_all_warning_title">Open %d tabs?</string>
    <!-- Content description (not visible, for screen readers etc.): Close tab <title> button. -->
    <string name="close_tab_title">Close tab %s</string>
    <!-- Postfix for private WebApp titles. -->
    <string name="pwa_site_controls_title_private">%1${'$'}s (Private Mode)</string>
    <!-- History multi select title in app bar. -->
    <string name="history_multi_select_title">%1${'$'}d selected</string>
    <!-- The content of the notification, this will be shown to the user when one newly supported add-on is available.
    %1${'$'}s is the add-on name. -->
    <string name="mozac_feature_addons_supported_checker_notification_content_one">Add %1${'$'}s to %2${'$'}s</string>
    <!-- Accessibility content description for the amount of stars that add-on has. -->
    <string name="mozac_feature_addons_rating_content_description_2">Rating: %1${'$'}.02f out of 5</string>
    <!-- Text for classic wallpapers title. %s is the Firefox name. -->
    <string name="wallpaper_classic_title">Classic %s</string>
    <string name="mozac_feature_addons_failed_to_install">Failed to install %s</string>
    <!-- Just a comment. -->
    <string name="errorpage_httpsonly_message2"><![CDATA[%1${'$'}s tries to use an HTTPS connection whenever possible for more security.]]></string>
</resources>
""",
                ),
            )
            .run()
            .expect(
                """
res/values/string.xml:4: Error: The comment is missing or doesn't include references to
the following placeholders: %d [PlaceholderComment]
    <string name="open_all_warning_title">Open %d tabs?</string>
                                          ~~~~~~~~~~~~~
res/values/string.xml:6: Error: The comment is missing or doesn't include references to
the following placeholders: %s [PlaceholderComment]
    <string name="close_tab_title">Close tab %s</string>
                                   ~~~~~~~~~~~~
res/values/string.xml:8: Error: The comment is missing or doesn't include references to
the following placeholders: %1＄s [PlaceholderComment]
    <string name="pwa_site_controls_title_private">%1＄s (Private Mode)</string>
                                                   ~~~~~~~~~~~~~~~~~~~
res/values/string.xml:10: Error: The comment is missing or doesn't include references to
the following placeholders: %1＄d [PlaceholderComment]
    <string name="history_multi_select_title">%1＄d selected</string>
                                              ~~~~~~~~~~~~~
res/values/string.xml:13: Error: The comment is missing or doesn't include references to
the following placeholders: %2＄s [PlaceholderComment]
    <string name="mozac_feature_addons_supported_checker_notification_content_one">Add %1＄s to %2＄s</string>
                                                                                   ~~~~~~~~~~~~~~~~
res/values/string.xml:15: Error: The comment is missing or doesn't include references to
the following placeholders: %1＄.02f [PlaceholderComment]
    <string name="mozac_feature_addons_rating_content_description_2">Rating: %1＄.02f out of 5</string>
                                                                     ~~~~~~~~~~~~~~~~~~~~~~~~
res/values/string.xml:18: Error: The comment is missing or doesn't include references to
the following placeholders: %s [PlaceholderComment]
    <string name="mozac_feature_addons_failed_to_install">Failed to install %s</string>
                                                          ~~~~~~~~~~~~~~~~~~~~
res/values/string.xml:20: Error: The comment is missing or doesn't include references to
the following placeholders: %1＄s [PlaceholderComment]
    <string name="errorpage_httpsonly_message2"><![CDATA[%1＄s tries to use an HTTPS connection whenever possible for more security.]]></string>
                                                         ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
8 errors, 0 warnings
            """,
            )
    }
}

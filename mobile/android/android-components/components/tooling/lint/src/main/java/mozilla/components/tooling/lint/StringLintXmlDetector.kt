/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.tooling.lint

import com.android.resources.ResourceFolderType
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.ResourceXmlDetector
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import com.android.tools.lint.detector.api.XmlContext
import org.w3c.dom.Element
import org.w3c.dom.Node
import org.w3c.dom.Node.COMMENT_NODE
import org.w3c.dom.Node.ELEMENT_NODE

/**
 * Custom lint check that ensures the string resources are correctly formatted.
 */
class StringLintXmlDetector : ResourceXmlDetector() {

    override fun appliesTo(folderType: ResourceFolderType): Boolean {
        return folderType == ResourceFolderType.VALUES
    }

    override fun getApplicableElements(): Collection<String> {
        return setOf("string")
    }

    override fun visitElement(context: XmlContext, element: Element) {
        val stringName = element.getAttribute("name")

        if (element.firstChild == null) {
            context.report(
                issue = ISSUE_BLANK_STRING,
                scope = element,
                location = context.getLocation(element),
                message = "$stringName is blank.",
            )

            return
        }

        // Get the text or CDATA section node from a <string> element.
        val node = element.firstChild

        if (listOf(Node.TEXT_NODE, Node.CDATA_SECTION_NODE).none { node.nodeType == it }) {
            return
        }

        val stringText = node.nodeValue

        if (stringText.isBlank()) {
            context.report(
                issue = ISSUE_BLANK_STRING,
                scope = node,
                location = context.getLocation(node),
                message = "$stringName is blank.",
            )

            return
        }

        // Check for hard-coded brand names.
        for (brand in BRANDS) {
            if (brand in stringText) {
                context.report(
                    issue = ISSUE_BRAND_USAGE,
                    scope = node,
                    location = context.getLocation(node),
                    message = "Hard-coded brand $brand in string. Use a variable instead.",
                )
            }
        }

        checkPunctuation(context, node, stringText)
        checkPlaceholders(context, node, element, stringText)
    }

    private fun checkPunctuation(
        context: XmlContext,
        node: Node,
        stringText: String,
    ) {
        if (stringText.contains("...")) {
            context.report(
                issue = ISSUE_INCORRECT_ELLIPSIS,
                scope = node,
                location = context.getLocation(node),
                message = "Incorrect ellipsis character \\`...\\`. Use \\`…\\` instead.",
            )
        }

        if (stringText.contains("'")) {
            context.report(
                issue = ISSUE_STRAIGHT_QUOTE_USAGE,
                scope = node,
                location = context.getLocation(node),
                message = "Incorrect straight quote character \\`'\\`. use \\`’\\` instead.",
            )
        }

        if (stringText.contains('"')) {
            context.report(
                issue = ISSUE_STRAIGHT_DOUBLE_QUOTE_USAGE,
                scope = node,
                location = context.getLocation(node),
                message = "Incorrect straight double quote character \\`\"\\`. use \\`“”\\` instead.",
            )
        }
    }

    private fun checkPlaceholders(context: XmlContext, node: Node, element: Element, stringText: String) {
        val placeholderRegex = Regex("%(\\d+\\$)?(?:\\.[0-9]+)?[sdf]")
        val xmlPlaceholders = placeholderRegex.findAll(stringText)
            .map { it.value }
            .toSet()

        // Try to extract the comment for this string.
        val commentText = extractCommentText(element)

        // Only check the comment if the XML string actually contains placeholders.
        if (xmlPlaceholders.isNotEmpty()) {
            val commentPlaceholders = placeholderRegex.findAll(commentText)
                .map { it.value }
                .toSet()

            val missingPlaceholders = xmlPlaceholders - commentPlaceholders
            if (missingPlaceholders.isNotEmpty()) {
                context.report(
                    issue = ISSUE_PLACEHOLDER_COMMENT,
                    scope = node,
                    location = context.getLocation(node),
                    message = """
                        The comment is missing or doesn't include references to
                        the following placeholders: ${missingPlaceholders.joinToString(", ")}
                    """.trimIndent(),
                )
            }
        }
    }

    /**
     * Traverses previous siblings of the element to find a comment.
     * If it encounters another <string> element before a comment,
     * it assumes no comment and returns an empty string.
     */
    private fun extractCommentText(element: Element): String {
        var prev = element.previousSibling
        while (prev != null) {
            when (prev.nodeType) {
                COMMENT_NODE -> {
                    // Found a comment node: return its text.
                    return prev.nodeValue ?: ""
                }
                ELEMENT_NODE -> {
                    // If this element is a <string>, assumes that there is no
                    // comment associated with the string.
                    if ((prev as? Element)?.tagName.equals("string", ignoreCase = true)) {
                        return ""
                    }
                }
            }
            prev = prev.previousSibling
        }
        return ""
    }

    companion object {
        private val BRANDS = listOf(
            "Firefox",
            "Focus",
            "Klar",
            "Pocket",
        )

        @JvmField
        val ISSUE_BLANK_STRING = createStringLintXmlDetectorIssue(
            id = "BlankString",
            briefDescription = "String resource should not be blank",
            explanation = """
                A <string> resource was created, but no string value was added.
            """.trimIndent(),
        )

        @JvmField
        val ISSUE_INCORRECT_ELLIPSIS = createStringLintXmlDetectorIssue(
            id = "IncorrectEllipsisCharacter",
            briefDescription = "Incorrect ellipsis character was used",
            explanation = """
                Incorrect ellipsis character \`...\`. Use \`…\` instead.
            """.trimIndent(),
        )

        @JvmField
        val ISSUE_STRAIGHT_QUOTE_USAGE = createStringLintXmlDetectorIssue(
            id = "IncorrectStraightQuote",
            briefDescription = "Incorrect straight quote character was used",
            explanation = """
                Incorrect straight quote character \`'\`. use \`’\` instead.
            """.trimIndent(),
        )

        @JvmField
        val ISSUE_STRAIGHT_DOUBLE_QUOTE_USAGE = createStringLintXmlDetectorIssue(
            id = "IncorrectStraightDoubleQuote",
            briefDescription = "Incorrect straight double quote character was used",
            explanation = """
                Incorrect straight double quote character \`"\`. use \`“”\` instead.
            """.trimIndent(),
        )

        @JvmField
        val ISSUE_BRAND_USAGE = createStringLintXmlDetectorIssue(
            id = "BrandUsage",
            briefDescription = "Hard-coded brand name in string",
            explanation = """
                Hard-coded brand name in string. Use a variable instead.
            """.trimIndent(),
        )

        @JvmField
        val ISSUE_PLACEHOLDER_COMMENT = createStringLintXmlDetectorIssue(
            id = "PlaceholderComment",
            briefDescription = "Missing Placeholder references in string comment",
            explanation = """
                String comments need to document all placeholders in a string.
            """.trimIndent(),
        )

        private fun createStringLintXmlDetectorIssue(
            id: String,
            briefDescription: String,
            explanation: String,
        ) = Issue.create(
            id = id,
            briefDescription = briefDescription,
            explanation = explanation,
            category = Category.CORRECTNESS,
            priority = 6,
            severity = Severity.ERROR,
            implementation = Implementation(
                StringLintXmlDetector::class.java,
                Scope.RESOURCE_FILE_SCOPE,
            ),
        )
    }
}

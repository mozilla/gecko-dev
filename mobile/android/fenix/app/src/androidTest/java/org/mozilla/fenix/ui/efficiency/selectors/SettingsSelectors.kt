package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSelectors {
    val GO_BACK_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_CONTENT_DESC,
        value = "Navigate up",
        description = "the Back Arrow button",
        groups = listOf("requiredForPage"),
    )

    val GENERAL_HEADING = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "General",
        description = "the General heading",
        groups = listOf("requiredForPage"),
    )

    val SEARCH_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Search",
        description = "the Search button",
        groups = listOf("requiredForPage"),
    )

    val TABS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Tabs",
        description = "the Tabs button",
        groups = listOf("requiredForPage"),
    )

    val ACCESSIBILITY_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Accessibility",
        description = "the Accessibility button",
        groups = listOf("requiredForPage"),
    )

    val AUTOFILL_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Autofill",
        description = "the Autofill button",
        groups = listOf("requiredForPage"),
    )

    val CUSTOMIZE_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Customize",
        description = "the Customize button",
        groups = listOf("requiredForPage"),
    )

    val HOMEPAGE_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Homepage",
        description = "the Homepage button",
        groups = listOf("requiredForPage"),
    )

    val PASSWORDS_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Passwords",
        description = "the Passwords button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        GO_BACK_BUTTON,
        GENERAL_HEADING,
        SEARCH_BUTTON,
        TABS_BUTTON,
        ACCESSIBILITY_BUTTON,
        AUTOFILL_BUTTON,
        CUSTOMIZE_BUTTON,
        HOMEPAGE_BUTTON,
        PASSWORDS_BUTTON,
    )
}

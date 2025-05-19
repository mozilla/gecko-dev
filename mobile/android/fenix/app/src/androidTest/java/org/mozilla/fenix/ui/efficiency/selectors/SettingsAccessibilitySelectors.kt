package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsAccessibilitySelectors {
    val SETTINGS_ACCESSIBILITY_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.preferences_accessibility),
        description = "The Accessibility Settings header",
        groups = listOf("requiredForPage"),
    )

    val USE_SYSTEM_FONT_SIZE_TOGGLE = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "use_system_font_size_toggle",
        description = "Use System Font Size Toggle",
        groups = listOf("accessibilitySettings"),
    )

    val all = listOf(
        SETTINGS_ACCESSIBILITY_TITLE,
        USE_SYSTEM_FONT_SIZE_TOGGLE,
    )
}

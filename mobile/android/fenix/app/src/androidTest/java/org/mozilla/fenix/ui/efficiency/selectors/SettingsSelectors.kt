package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSelectors {
    val GO_BACK_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_CONTENT_DESC,
        value = "Navigate up",
        description = "the Back Arrow button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val GENERAL_HEADING = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "General",
        description = "the General heading",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val SEARCH_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Search",
        description = "the Search button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val TABS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Tabs",
        description = "the Tabs button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val ACCESSIBILITY_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Accessibility",
        description = "the Accessibility button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val AUTOFILL_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Autofill",
        description = "the Autofill button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val CUSTOMIZE_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Customize",
        description = "the Customize button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val HOMEPAGE_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Homepage",
        description = "the Homepage button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val PASSWORDS_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Passwords",
        description = "the Passwords button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val ABOUT_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT,
        value = "About $appName",
        description = "the About button",
        groups = listOf("aboutSettingsSection"),
    )

    val DATA_COLLECTION_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Data collection",
        description = "the Data Collection button",
        groups = listOf("privacyAndSecuritySettingsSection"),
    )

    val DELETE_BROWSING_DATA_ON_QUIT_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Delete browsing data on quit",
        description = "the Delete browsing data on quit button",
        groups = listOf("privacyAndSecuritySettingsSection"),
    )

    val DELETE_BROWSING_DATA_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Delete browsing data",
        description = "the Delete browsing data button",
        groups = listOf("privacyAndSecuritySettingsSection"),
    )

    val ENHANCED_TRACKING_PROTECTION_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Enhanced Tracking Protection",
        description = "the Enhanced tracking protection button",
        groups = listOf("privacyAndSecuritySettingsSection"),
    )

    val HTTPS_ONLY_MODE_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = getStringResource(R.string.preferences_https_only_title),
        description = "the HTTPS only mode button",
        groups = listOf("privacyAndSecuritySettingsSection"),
    )

    val LANGUAGE_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = getStringResource(R.string.preferences_language),
        description = "the Language button",
        groups = listOf("requiredForPage", "generalSettingsSection"),
    )

    val OPEN_LINKS_IN_APPS_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Open links in apps",
        description = "the Open links in apps button",
        groups = listOf("advancedSettingsSection"),
    )

    val PRIVATE_BROWSING_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Private browsing",
        description = "the Private browsing button",
        groups = listOf("privacyAndSecuritySettingsSection"),
    )

    val TRANSLATIONS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Translations",
        description = "the Private browsing button",
        groups = listOf("generalSettingsSection"),
    )

    val SYNC_AND_SAVE_YOUR_DATA_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = "Sync and save your data",
        description = "the Sync and save your data button",
        groups = listOf("requiredForPage"),
    )

    val NOTIFICATIONS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Notifications",
        description = "the Notifications button",
        groups = listOf("privacyAndSecuritySettingsSection"),
    )

    val EXPERIMENTS_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_TEXT,
        value = getStringResource(R.string.preferences_nimbus_experiments),
        description = "the Experiments button",
        groups = listOf("experiments"),
    )

    val SITE_SETTINGS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = "Site settings",
        description = "the Site settings button",
        groups = listOf("privacyAndSecuritySettingsSection"),
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
        ABOUT_BUTTON,
        DATA_COLLECTION_BUTTON,
        DELETE_BROWSING_DATA_ON_QUIT_BUTTON,
        DELETE_BROWSING_DATA_BUTTON,
        ENHANCED_TRACKING_PROTECTION_BUTTON,
        HTTPS_ONLY_MODE_BUTTON,
        LANGUAGE_BUTTON,
        OPEN_LINKS_IN_APPS_BUTTON,
        PRIVATE_BROWSING_BUTTON,
        TRANSLATIONS_BUTTON,
        SYNC_AND_SAVE_YOUR_DATA_BUTTON,
        NOTIFICATIONS_BUTTON,
        EXPERIMENTS_BUTTON,
        SITE_SETTINGS_BUTTON,
    )
}

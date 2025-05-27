package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsSiteSettingsPermissionsSelectors {

    val ASK_TO_ALLOW_RADIO_BUTTON = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "ask_to_allow_radio",
        description = "Ask to allow radio button",
        groups = listOf("askToAllow"),
    )

    val all = listOf(
        ASK_TO_ALLOW_RADIO_BUTTON,
    )
}

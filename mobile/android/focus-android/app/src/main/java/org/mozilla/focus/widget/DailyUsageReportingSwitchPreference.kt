/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget

import android.content.Context
import android.util.AttributeSet
import org.mozilla.focus.ext.components
import org.mozilla.focus.settings.LearnMoreSwitchPreference
import org.mozilla.focus.utils.SupportUtils

/**
 * Switch preference for enabling/disabling telemetry
 */
internal class DailyUsageReportingSwitchPreference(context: Context, attrs: AttributeSet?) :
    LearnMoreSwitchPreference(context, attrs) {

    init {
        isChecked = context.components.settings.isDailyUsagePingEnabled
    }

    override fun onClick() {
        super.onClick()
        if (isChecked) {
            context.components.usageReportingMetricsService.start()
        } else {
            context.components.usageReportingMetricsService.stop()
        }
    }

    override fun getLearnMoreUrl(): String {
        return SupportUtils.getSumoURLForTopic(
            SupportUtils.getAppVersion(context),
            SupportUtils.SumoTopic.USAGE_PING_SETTINGS,
        )
    }
}

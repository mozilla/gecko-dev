/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.content

import android.app.UiModeManager
import android.content.Context
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_NO
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_YES
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.ParameterizedRobolectricTestRunner
import org.robolectric.Robolectric
import org.robolectric.annotation.Config
import org.robolectric.shadow.api.Shadow
import org.robolectric.shadows.ShadowUIModeManager

@RunWith(ParameterizedRobolectricTestRunner::class)
@Config(sdk = [31])
class NightModeTest(@AppCompatDelegate.NightMode private val permission: Int) {
    private val activity = Robolectric.buildActivity(AppCompatActivity::class.java)
        .create()
        .get()
    private val uiModeManager = Shadow.extract<ShadowUIModeManager>(activity.getSystemService(Context.UI_MODE_SERVICE))

    @Test
    fun `GIVEN a night mode WHEN asked to apply it THEN apply and persist it`() {
        activity.setApplicationNightMode(permission)

        assertEquals(permission, AppCompatDelegate.getDefaultNightMode())
        assertEquals(permission.toUIModeManagerNightMode(), uiModeManager.applicationNightMode)
    }

    private fun Int.toUIModeManagerNightMode() = when (this) {
        MODE_NIGHT_NO -> UiModeManager.MODE_NIGHT_NO
        MODE_NIGHT_YES -> UiModeManager.MODE_NIGHT_YES
        else -> UiModeManager.MODE_NIGHT_AUTO
    }

    companion object {
        @JvmStatic
        @ParameterizedRobolectricTestRunner.Parameters
        fun nightModes() = listOf(
            MODE_NIGHT_UNSPECIFIED,
            MODE_NIGHT_YES,
            MODE_NIGHT_NO,
            MODE_NIGHT_FOLLOW_SYSTEM,
            MODE_NIGHT_AUTO_BATTERY,
        )
    }
}

package org.mozilla.focus.helpers

import android.Manifest
import android.os.Build
import androidx.test.rule.GrantPermissionRule
import org.junit.Before
import org.junit.Rule
import org.mozilla.focus.helpers.TestHelper.allowOrPreventSystemUIFromReadingTheClipboard
import org.mozilla.focus.helpers.TestHelper.mDevice

open class TestSetup {

    @get:Rule
    val generalPermissionRule: GrantPermissionRule =
        if (Build.VERSION.SDK_INT >= 33) {
            GrantPermissionRule.grant(
                Manifest.permission.POST_NOTIFICATIONS,
            )
        } else {
            GrantPermissionRule.grant()
        }

    @Before
    open fun setUp() {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            allowOrPreventSystemUIFromReadingTheClipboard(allowToReadClipboard = false)
        }
        // Closes the notification tray if it's open, otherwise it's a no-op.
        mDevice.executeShellCommand("cmd statusbar collapse")
    }
}

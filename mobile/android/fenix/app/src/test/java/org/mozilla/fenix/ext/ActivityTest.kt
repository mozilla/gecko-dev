/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.app.Activity
import android.content.Intent
import android.view.View
import android.view.WindowManager
import io.mockk.mockk
import mozilla.components.support.utils.EXTRA_ACTIVITY_REFERRER_PACKAGE
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf

@RunWith(FenixRobolectricTestRunner::class)
class ActivityTest {

    // This will be addressed on https://github.com/mozilla-mobile/fenix/issues/17804
    @Suppress("DEPRECATION")
    @Test
    fun testEnterImmersiveMode() {
        val activity = Robolectric.buildActivity(Activity::class.java).create().get()
        val window = activity.window

        // Turn off Keep Screen on Flag if it is on
        if (shadowOf(window).getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)) window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Make sure that System UI flags are not set before the test
        val flags = arrayOf(View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION, View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN, View.SYSTEM_UI_FLAG_HIDE_NAVIGATION, View.SYSTEM_UI_FLAG_FULLSCREEN, View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
        if (flags.any { f -> (window.decorView.systemUiVisibility and f) == f }) {
            window.decorView.systemUiVisibility = 0
        }

        // Run
        activity.enterToImmersiveMode()

        // Test
        for (f in flags) assertEquals(f, window.decorView.systemUiVisibility and f)
        assertTrue(shadowOf(window).getFlag(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON))
    }

    @Test
    fun `WHEN Activity is not supported THEN getNavDirections throws IllegalArgument exception`() {
        val activity = mockk<Activity>()

        assertThrows(IllegalArgumentException::class.java) {
            activity.getNavDirections(
                BrowserDirection.FromGlobal,
            )
        }
    }

    @Test
    fun `WHEN Activity is not supported THEN getIntentSource throws IllegalArgument exception`() {
        val activity = mockk<Activity>()

        assertThrows(IllegalArgumentException::class.java) { activity.getIntentSource(mockk()) }
    }

    @Test
    fun `WHEN Activity is not supported THEN getIntentSessionId throws IllegalArgument exception`() {
        val activity = mockk<Activity>()

        assertThrows(IllegalArgumentException::class.java) { activity.getIntentSessionId(mockk()) }
    }

    @Test
    fun `WHEN Activity is not supported THEN getBreadcrumbMessage throws IllegalArgument exception`() {
        val activity = mockk<Activity>()

        assertThrows(IllegalArgumentException::class.java) { activity.getBreadcrumbMessage(mockk()) }
    }

    @Test
    fun `WHEN an intent is from within the app THEN return true`() {
        // We need to create a dummy activity so that we can get the right packageName in the
        // test environment so that we can put it into a new activity to verify the extra is set.
        // We don't hardcode a packageName in here because the value changes based on the variant
        // that we run these from (e.g. debug, beta)
        val dummyActivity = Robolectric.buildActivity(Activity::class.java).create().get()
        val intent = Intent().apply {
            putExtra(EXTRA_ACTIVITY_REFERRER_PACKAGE, dummyActivity.packageName)
        }
        val activity = Robolectric.buildActivity(Activity::class.java, intent)
            .create()
            .get()

        assertTrue(activity.isIntentInternal())
    }
}

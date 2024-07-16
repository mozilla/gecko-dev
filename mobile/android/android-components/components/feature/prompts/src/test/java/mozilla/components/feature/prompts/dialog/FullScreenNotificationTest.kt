/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.dialog

import android.app.Activity
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.whenever
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import org.robolectric.Robolectric
import org.robolectric.shadows.ShadowToast

@RunWith(AndroidJUnit4::class)
class FullScreenNotificationTest {

    @Mock
    lateinit var mockGestureNavUtils: GestureNavUtils

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        val activity = Robolectric.buildActivity(EmptyActivity::class.java)
        activity.create()
    }

    @Test
    fun `when in 3 button navigation mode, full screen notification shows exit instructions for back button`() {
        whenever(mockGestureNavUtils.isInGestureNavigationMode(any())).thenReturn(false)
        ActivityScenario.launch(EmptyActivity::class.java).use { scenario ->
            scenario.onActivity { activity: EmptyActivity ->
                FullScreenNotificationToast(
                    activity,
                    "gesture",
                    "button",
                    mockGestureNavUtils,
                ).show()
                assertTrue(ShadowToast.showedToast("button"))
            }
        }
    }

    @Test
    fun `when in gesture navigation mode, full screen notification shows exit instructions for gesture nav`() {
        whenever(mockGestureNavUtils.isInGestureNavigationMode(any())).thenReturn(true)
        ActivityScenario.launch(EmptyActivity::class.java).use { scenario ->
            scenario.onActivity { activity: EmptyActivity ->
                FullScreenNotificationToast(
                    activity,
                    "gesture",
                    "button",
                    mockGestureNavUtils,
                ).show()
                assertTrue(ShadowToast.showedToast("gesture"))
            }
        }
    }
}

internal class EmptyActivity : Activity()

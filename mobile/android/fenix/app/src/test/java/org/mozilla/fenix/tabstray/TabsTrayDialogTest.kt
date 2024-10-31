/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.app.Activity
import android.content.Context
import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric

@RunWith(FenixRobolectricTestRunner::class)
class TabsTrayDialogTest {
    @Test
    fun `WHEN onBackPressed THEN invoke interactor`() {
        val activity = Robolectric.buildActivity(Activity::class.java).setup().get()
        val context: Context = activity
        val interactor = mockk<TabsTrayInteractor>(relaxed = true)
        val dialog = TabsTrayDialog(context, 0) { interactor }

        dialog.onBackPressedCallback.handleOnBackPressed()

        verify { interactor.onBackPressed() }
    }
}

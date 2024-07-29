/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.LayoutInflater
import androidx.lifecycle.LifecycleCoroutineScope
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.FragmentHomeBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragment.Companion.TOAST_ELEVATION
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.allowUndo

@RunWith(FenixRobolectricTestRunner::class)
class InstrumentedHomeFragmentTest {
    val homeFragment = spyk(HomeFragment())
    val settings = mockk<Settings>()

    @Test
    fun `WHEN a pinned top is removed THEN show the undo snackbar`() {
        try {
            val topSite = TopSite.Default(
                id = 1L,
                title = "Mozilla",
                url = "https://mozilla.org",
                null,
            )
            mockkStatic("org.mozilla.fenix.utils.UndoKt")
            mockkStatic("androidx.lifecycle.LifecycleOwnerKt")
            val lifecycleScope: LifecycleCoroutineScope = mockk(relaxed = true)
            every { any<LifecycleOwner>().lifecycleScope } returns lifecycleScope
            every { homeFragment.getString(R.string.snackbar_top_site_removed) } returns "Mocked Removed Top Site"
            every { homeFragment.getString(R.string.snackbar_deleted_undo) } returns "Mocked Undo Removal"
            every { settings.toolbarPosition } returns ToolbarPosition.TOP
            every {
                any<CoroutineScope>().allowUndo(
                    view = any(),
                    message = any(),
                    undoActionTitle = any(),
                    onCancel = any(),
                    operation = any(),
                    anchorView = any(),
                    elevation = any(),
                    paddedForBottomToolbar = any(),
                )
            } just Runs
            val homeBinding = FragmentHomeBinding.inflate(LayoutInflater.from(testContext))
            every { homeFragment.binding } returns homeBinding

            homeFragment.showUndoSnackbarForTopSite(topSite)

            verify {
                lifecycleScope.allowUndo(
                    view = homeBinding.dynamicSnackbarContainer,
                    message = "Mocked Removed Top Site",
                    undoActionTitle = "Mocked Undo Removal",
                    onCancel = any(),
                    operation = any(),
                    anchorView = any(),
                    elevation = TOAST_ELEVATION,
                    paddedForBottomToolbar = false,
                )
            }
        } finally {
            unmockkStatic("org.mozilla.fenix.utils.UndoKt")
            unmockkStatic("androidx.lifecycle.LifecycleOwnerKt")
        }
    }
}

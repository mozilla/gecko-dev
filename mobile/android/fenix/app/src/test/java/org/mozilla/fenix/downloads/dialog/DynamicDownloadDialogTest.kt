/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.dialog

import android.content.Context
import android.view.LayoutInflater
import android.webkit.MimeTypeMap
import androidx.coordinatorlayout.widget.CoordinatorLayout
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.DownloadDialogLayoutBinding
import org.mozilla.fenix.downloads.dialog.DynamicDownloadDialog.Companion.getCannotOpenFileErrorMessage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Shadows.shadowOf

@RunWith(FenixRobolectricTestRunner::class)
class DynamicDownloadDialogTest {

    @Before
    fun setUp() {
        every { testContext.settings() } returns mockk(relaxed = true)
        every { testContext.components.appStore } returns mockk(relaxed = true)
    }

    @Test
    fun `WHEN calling getCannotOpenFileErrorMessage THEN should return the error message for the download file type`() {
        val download = DownloadState(url = "", fileName = "image.gif")

        shadowOf(MimeTypeMap.getSingleton()).apply {
            addExtensionMimeTypeMapping(".gif", "image/gif")
        }

        val expected = testContext.getString(
            R.string.mozac_feature_downloads_open_not_supported1,
            "gif",
        )

        val result = getCannotOpenFileErrorMessage(testContext, download)
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN a custom behavior not set WHEN showing a new download complete dialog THEN set a new behavior`() {
        val binding = createDynamicDownloadBinding()
        val dialog = createDynamicDownloadDialog(binding = binding)

        assertNull(binding.behavior)

        dialog.show()

        assertTrue(binding.behavior is DynamicDownloadDialogBehavior)
    }

    @Test
    fun `GIVEN a custom behavior already set WHEN showing a new download complete dialog THEN don't set a new behavior`() {
        val binding = createDynamicDownloadBinding()
        val dialog = createDynamicDownloadDialog(binding = binding)

        dialog.show()
        val initialBehavior = binding.behavior

        dialog.show()

        assertSame(initialBehavior, binding.behavior)
    }

    @Test
    fun `GIVEN a custom behavior already set WHEN dismissing the download complete dialog THEN remove any set behaviors`() {
        val binding = createDynamicDownloadBinding()
        val dialog = createDynamicDownloadDialog(binding = binding)
        dialog.show()

        dialog.dismiss()

        assertNull(binding.behavior)
    }

    private val DownloadDialogLayoutBinding.behavior
        get() = (root.layoutParams as? CoordinatorLayout.LayoutParams)?.behavior

    private fun createDynamicDownloadBinding(): DownloadDialogLayoutBinding {
        val downloadDialog = LayoutInflater.from(testContext).inflate(
            R.layout.download_dialog_layout,
            CoordinatorLayout(testContext),
            false,
        )
        CoordinatorLayout(testContext).addView(downloadDialog)

        return DownloadDialogLayoutBinding.bind(downloadDialog)
    }

    private fun createDynamicDownloadDialog(
        context: Context = testContext,
        downloadState: DownloadState? = mockk(relaxed = true),
        didFail: Boolean = false,
        tryAgain: (String) -> Unit = { },
        onCannotOpenFile: (DownloadState) -> Unit = { },
        onDismiss: () -> Unit = { },
        binding: DownloadDialogLayoutBinding,
    ) = DynamicDownloadDialog(
        context = context,
        downloadState = downloadState,
        didFail = didFail,
        tryAgain = tryAgain,
        onCannotOpenFile = onCannotOpenFile,
        binding = binding,
        onDismiss = onDismiss,
    )
}

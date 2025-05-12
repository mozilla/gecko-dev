/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.res.Resources
import androidx.core.content.getSystemService
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn

@RunWith(AndroidJUnit4::class)
class ClipboardHandlerTest {

    private val clipboardUrl = "https://www.mozilla.org"
    private val clipboardText = "Mozilla"
    private lateinit var clipboard: ClipboardManager
    private lateinit var clipboardHandler: ClipboardHandler

    @Before
    fun setup() {
        clipboard = testContext.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        clipboardHandler = ClipboardHandler(testContext)
    }

    @Test
    fun `GIVEN clipboard contains text WHEN requesting it THEN get the available text`() {
        assertEquals(null, clipboardHandler.text)

        clipboard.setPrimaryClip(ClipData.newPlainText("Text", clipboardText))

        assertEquals(clipboardText, clipboardHandler.text)
    }

    @Test
    fun `GIVEN text is added to the clipboard WHEN requesting it back THEN get the same text`() {
        assertEquals(null, clipboardHandler.text)

        clipboardHandler.text = clipboardText

        assertEquals(clipboardText, clipboardHandler.text)
    }

    @Test
    fun `GIVEN clipboard contains an URL WHEN requesting it as an URL THEN get the text`() {
        assertEquals(null, clipboardHandler.extractURL())

        clipboard.setPrimaryClip(ClipData.newPlainText("Text", clipboardUrl))

        assertEquals(clipboardUrl, clipboardHandler.extractURL())
    }

    @Test
    fun `GIVEN clipboard contains text WHEN requesting it as an URL THEN don't return anything`() {
        assertEquals(null, clipboardHandler.extractURL())

        clipboard.setPrimaryClip(ClipData.newPlainText("Text", clipboardText))

        assertNull(clipboardHandler.extractURL())
    }

    @Test
    fun `GIVEN clipboard contains HTML code WHEN requesting it as an URL THEN return the first one found`() {
        assertEquals(null, clipboardHandler.extractURL())

        clipboard.setPrimaryClip(ClipData.newHtmlText("Html", clipboardUrl, clipboardUrl))

        assertEquals(clipboardUrl, clipboardHandler.extractURL())
    }

    @Test
    fun `GIVEN clipboard contains an text marked as an URL WHEN requesting it as an URL THEN return the text`() {
        assertEquals(null, clipboardHandler.extractURL())

        clipboard.setPrimaryClip(
            ClipData(clipboardUrl, arrayOf("text/x-moz-url"), ClipData.Item(clipboardUrl)),
        )
        assertEquals(clipboardUrl, clipboardHandler.extractURL())
    }

    @Test
    fun `GIVEN clipboard contains an URL with an unsafe scheme WHEN requesting it THEN return the text without the unwanted scheme`() {
        val resources = mock<Resources>()
        val context = mock<Context>()
        val aboutPage = "about:page"
        doReturn(resources).`when`(context).resources
        doReturn(arrayOf("about:")).`when`(resources).getStringArray(R.array.mozac_url_schemes_blocklist)
        doReturn(clipboard).`when`(context).getSystemService<ClipboardManager>()
        clipboardHandler = ClipboardHandler(context)
        clipboard.setPrimaryClip(ClipData.newPlainText("about", aboutPage))

        val result = clipboardHandler.firstSafePrimaryClipItemText

        assertEquals("page", result)
    }
}

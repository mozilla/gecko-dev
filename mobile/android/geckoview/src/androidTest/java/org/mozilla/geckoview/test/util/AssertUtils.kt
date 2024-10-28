/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test.util

import android.graphics.Bitmap
import android.util.Base64
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import java.io.ByteArrayOutputStream

object AssertUtils {
    @JvmStatic
    fun assertScreenshotResult(result: Bitmap, comparisonImage: Bitmap) {
        assertNotNull(
            "Screenshot is not null",
            result,
        )
        assertEquals("Widths are the same", comparisonImage.width, result.width)
        assertEquals("Heights are the same", comparisonImage.height, result.height)
        assertEquals("Byte counts are the same", comparisonImage.byteCount, result.byteCount)
        assertEquals("Configs are the same", comparisonImage.config, result.config)

        if (!comparisonImage.sameAs(result)) {
            val outputForComparison = ByteArrayOutputStream()
            comparisonImage.compress(Bitmap.CompressFormat.PNG, 100, outputForComparison)

            val outputForActual = ByteArrayOutputStream()
            result.compress(Bitmap.CompressFormat.PNG, 100, outputForActual)
            val actualString: String = Base64.encodeToString(outputForActual.toByteArray(), Base64.DEFAULT)
            val comparisonString: String = Base64.encodeToString(outputForComparison.toByteArray(), Base64.DEFAULT)

            assertEquals("Encoded strings are the same", comparisonString, actualString)
        }
    }
}

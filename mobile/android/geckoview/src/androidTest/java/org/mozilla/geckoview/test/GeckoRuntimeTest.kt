/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@MediumTest
class GeckoRuntimeTest : BaseSessionTest() {
    @Test
    fun isInteractiveWidgetDefaultResizesVisualFalse() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.interactive_widget_default_resizes_visual" to false,
            ),
        )

        assertFalse(sessionRule.runtime.isInteractiveWidgetDefaultResizesVisual())
    }

    @Test
    fun isInteractiveWidgetDefaultResizesVisualTrue() {
        sessionRule.setPrefsUntilTestEnd(
            mapOf(
                "dom.interactive_widget_default_resizes_visual" to true,
            ),
        )

        assertTrue(sessionRule.runtime.isInteractiveWidgetDefaultResizesVisual())
    }
}

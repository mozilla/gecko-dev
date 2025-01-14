/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.tabcounter

import android.content.res.ColorStateList
import android.view.LayoutInflater
import android.view.View
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.core.view.isVisible
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.databinding.MozacUiTabcounterLayoutBinding
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TabCounterTest {

    private lateinit var tabCounter: TabCounter
    private lateinit var binding: MozacUiTabcounterLayoutBinding

    @Before
    fun setUp() {
        tabCounter = TabCounter(testContext)
        binding =
            MozacUiTabcounterLayoutBinding.inflate(LayoutInflater.from(testContext), tabCounter)
    }

    @Test
    fun `Default tab count is set to zero`() {
        val expectedIcon = ContextCompat.getDrawable(testContext, R.drawable.mozac_ui_tabcounter_box)?.toBitmap()
        val actualIcon = binding.counterBox.background.toBitmap()

        assertTrue(actualIcon.sameAs(expectedIcon))
        assertTrue(binding.counterText.isVisible)
        assertEquals("0", binding.counterText.text)
    }

    @Test
    fun `Set tab count as single digit value shows count`() {
        tabCounter.setCount(1)
        val expectedIcon = ContextCompat.getDrawable(testContext, R.drawable.mozac_ui_tabcounter_box)?.toBitmap()
        val actualIcon = binding.counterBox.background.toBitmap()

        assertTrue(actualIcon.sameAs(expectedIcon))
        assertTrue(binding.counterText.isVisible)
        assertEquals("1", binding.counterText.text)
    }

    @Test
    fun `Set tab count as two digit number shows count`() {
        tabCounter.setCount(99)
        val expectedIcon = ContextCompat.getDrawable(testContext, R.drawable.mozac_ui_tabcounter_box)?.toBitmap()
        val actualIcon = binding.counterBox.background.toBitmap()

        assertTrue(actualIcon.sameAs(expectedIcon))
        assertTrue(binding.counterText.isVisible)
        assertEquals("99", binding.counterText.text)
    }

    @Test
    fun `Setting tab count as three digit value shows correct icon`() {
        tabCounter.setCount(100)
        val expectedIcon = ContextCompat.getDrawable(testContext, R.drawable.mozac_ui_infinite_tabcounter_box)?.toBitmap()
        val actualIcon = binding.counterBox.background.toBitmap()

        assertTrue(actualIcon.sameAs(expectedIcon))
        assertFalse(binding.counterText.isVisible)
    }

    @Test
    fun `Setting tab color shows correct icon`() {
        val colorStateList: ColorStateList = mock()

        tabCounter.setColor(colorStateList)
        assertEquals(binding.counterText.textColors, colorStateList)
    }

    @Test
    fun `Toggling the counterMask will set the mask to visible`() {
        assertEquals(binding.counterMask.visibility, View.GONE)
        tabCounter.toggleCounterMask(true)
        assertEquals(binding.counterMask.visibility, View.VISIBLE)
    }
}

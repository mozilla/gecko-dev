/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import androidx.core.graphics.Insets
import androidx.core.view.WindowInsetsAnimationCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsCompat.Type.ime
import androidx.core.view.WindowInsetsCompat.Type.systemBars
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class ImeInsetsSynchronizerTest {
    private val targetView: View = mock()
    private val targetViewLayoutParams: MarginLayoutParams = mock()
    private val synchronizer = ImeInsetsSynchronizer.setup(targetView)!!

    @Before
    fun setup() {
        doReturn(targetViewLayoutParams).`when`(targetView).layoutParams
    }

    @Test
    fun `WHEN the ime synchronizer is created THEN start listening for window insets updates and animations`() {
        verify(targetView).setOnApplyWindowInsetsListener(any())
        verify(targetView).setWindowInsetsAnimationCallback(any())
    }

    @Test
    fun `GIVEN ime animation in progress WHEN window insets change THEN don't update any margins`() {
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(1, 2, 3, 4)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        val imeAnimation: WindowInsetsAnimationCompat = mock()
        doReturn(ime()).`when`(imeAnimation).typeMask

        synchronizer.onPrepare(imeAnimation)
        val result = synchronizer.onApplyWindowInsets(targetView, windowInsets)

        assertEquals(windowInsets, result)
        verifyNoMoreInteractions(targetViewLayoutParams)
    }

    @Test
    fun `GIVEN ime animation is not in progress WHEN window insets change THEN update target view margins to be shown on top of the keyboard`() {
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(1, 2, 3, 4)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())

        val result = synchronizer.onApplyWindowInsets(targetView, windowInsets)

        assertEquals(windowInsets, result)
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 4)
        verify(targetView).requestLayout()
    }

    @Test
    fun `GIVEN synchronizing targetView with keyboard height is disabled WHEN window insets change THEN don't update any margings`() {
        val synchronizer = ImeInsetsSynchronizer.setup(targetView = targetView, synchronizeViewWithIME = false)!!
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(1, 2, 3, 4)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())

        val result = synchronizer.onApplyWindowInsets(targetView, windowInsets)

        assertEquals(windowInsets, result)
        verifyNoMoreInteractions(targetViewLayoutParams)
    }

    @Test
    fun `GIVEN ime animation is not in progress and it is hidden WHEN window insets change THEN inform about the current keyboard status`() {
        var isKeyboardShowing = "unknown"
        var keyboardAnimationFinishedHeight = -1
        val synchronizer = ImeInsetsSynchronizer.setup(
            targetView = targetView,
            onIMEAnimationStarted = { _, height ->
                isKeyboardShowing = "error"
                keyboardAnimationFinishedHeight = height + 22
            },
            onIMEAnimationFinished = { keyboardShowing, height ->
                isKeyboardShowing = keyboardShowing.toString()
                keyboardAnimationFinishedHeight = height
            },
        )!!
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 0)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        // Set that the keyboard is hidden
        doReturn(false).`when`(windowInsets).isVisible(ime())

        val result = synchronizer.onApplyWindowInsets(targetView, windowInsets)

        assertEquals(windowInsets, result)
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 0)
        verify(targetView).requestLayout()
        assertEquals("false", isKeyboardShowing)
        assertEquals(0, keyboardAnimationFinishedHeight)
    }

    @Test
    fun `GIVEN ime animation is not in progress and it is shown WHEN window insets change THEN inform about the current keyboard status`() {
        var isKeyboardShowing = "unknown"
        var keyboardAnimationFinishedHeight = -1
        val synchronizer = ImeInsetsSynchronizer.setup(
            targetView = targetView,
            onIMEAnimationStarted = { _, height ->
                isKeyboardShowing = "error"
                keyboardAnimationFinishedHeight = height + 22
            },
            onIMEAnimationFinished = { keyboardShowing, height ->
                isKeyboardShowing = keyboardShowing.toString()
                keyboardAnimationFinishedHeight = height
            },
        )!!
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 1000)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        // Set that the keyboard is shown
        doReturn(true).`when`(windowInsets).isVisible(ime())

        val result = synchronizer.onApplyWindowInsets(targetView, windowInsets)

        assertEquals(windowInsets, result)
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 960)
        verify(targetView).requestLayout()
        assertEquals("true", isKeyboardShowing)
        assertEquals(960, keyboardAnimationFinishedHeight)
    }

    @Test
    fun `WHEN the keyboard starts to hide THEN inform about the current keyboard status`() {
        var isKeyboardShowing = "unknown"
        var keyboardAnimationFinishedHeight = -1
        val synchronizer = ImeInsetsSynchronizer.setup(
            targetView = targetView,
            onIMEAnimationStarted = { keyboardShowing, height ->
                isKeyboardShowing = keyboardShowing.toString()
                keyboardAnimationFinishedHeight = height
            },
            onIMEAnimationFinished = { _, height ->
                isKeyboardShowing = "error"
                keyboardAnimationFinishedHeight = height + 22
            },
        )!!
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 1000)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        // Set that the keyboard is now considered hidden, just need to animate to that state
        doReturn(false).`when`(windowInsets).isVisible(ime())
        val imeAnimation: WindowInsetsAnimationCompat = mock()
        doReturn(ime()).`when`(imeAnimation).typeMask
        val animationBounds: WindowInsetsAnimationCompat.BoundsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 200)).`when`(animationBounds).lowerBound
        doReturn(Insets.of(0, 0, 0, 1200)).`when`(animationBounds).upperBound
        // Ensure the current window insets are known
        synchronizer.onApplyWindowInsets(targetView, windowInsets)

        synchronizer.onStart(imeAnimation, animationBounds)

        assertEquals("false", isKeyboardShowing)
        assertEquals(1000, keyboardAnimationFinishedHeight)
    }

    @Test
    fun `WHEN the keyboard starts to show THEN inform about the current keyboard status`() {
        var isKeyboardShowing = "unknown"
        var keyboardAnimationFinishedHeight = -1
        val synchronizer = ImeInsetsSynchronizer.setup(
            targetView = targetView,
            onIMEAnimationStarted = { keyboardShowing, height ->
                isKeyboardShowing = keyboardShowing.toString()
                keyboardAnimationFinishedHeight = height
            },
            onIMEAnimationFinished = { _, height ->
                isKeyboardShowing = "error"
                keyboardAnimationFinishedHeight = height + 22
            },
        )!!
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 1000)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        // Set that the keyboard starts to show
        doReturn(true).`when`(windowInsets).isVisible(ime())
        val imeAnimation: WindowInsetsAnimationCompat = mock()
        doReturn(ime()).`when`(imeAnimation).typeMask
        val animationBounds: WindowInsetsAnimationCompat.BoundsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 200)).`when`(animationBounds).lowerBound
        doReturn(Insets.of(0, 0, 0, 1200)).`when`(animationBounds).upperBound
        // Ensure the current window insets are known
        synchronizer.onApplyWindowInsets(targetView, windowInsets)

        synchronizer.onStart(imeAnimation, animationBounds)

        assertEquals("true", isKeyboardShowing)
        assertEquals(960, keyboardAnimationFinishedHeight)
    }

    @Test
    fun `GIVEN a show keyboard animation WHEN the progress is updated THEN update the targetView to be shown above the keyboard`() {
        // Set the initial system insets
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(1, 2, 3, 4)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        // Set that the keyboard is showing
        doReturn(true).`when`(windowInsets).isVisible(ime())
        val imeAnimation: WindowInsetsAnimationCompat = mock()
        doReturn(ime()).`when`(imeAnimation).typeMask
        synchronizer.onApplyWindowInsets(targetView, windowInsets)
        // Setup the the keyboard to have a final height of 1000px.
        val animationBounds: WindowInsetsAnimationCompat.BoundsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 200)).`when`(animationBounds).lowerBound
        doReturn(Insets.of(0, 0, 0, 1200)).`when`(animationBounds).upperBound

        synchronizer.onStart(imeAnimation, animationBounds)

        // Test the keyboard started to be showing but still below the navigation bar (10px vs 40px)
        doReturn(0.01f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        // Margins should have been set to 0 two times: once for onApplyWindowInsets and once now.
        verify(targetViewLayoutParams, times(2)).setMargins(0, 0, 0, 0)

        // Test the keyboard started to be showing just above the navigation bar (41px vs 40px)
        doReturn(0.041f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 1)

        // Test the keyboard being shown in half, above the navigation bar
        doReturn(0.5f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 460)

        // Test the keyboard being shown almost fully, above the navigation bar
        doReturn(0.95f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 910)

        // Test the keyboard being fully shown, above the navigation bar
        doReturn(1f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 960)
    }

    @Test
    fun `GIVEN synchronizing targetView with keyboard height is disabled WHEN the keyboard is animated into view THEN don't update any margins for the target view`() {
        // Set the initial system insets
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(1, 2, 3, 4)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        // Set that the keyboard is showing
        doReturn(true).`when`(windowInsets).isVisible(ime())
        val imeAnimation: WindowInsetsAnimationCompat = mock()
        doReturn(ime()).`when`(imeAnimation).typeMask
        val synchronizer = ImeInsetsSynchronizer.setup(targetView = targetView, synchronizeViewWithIME = false)!!
        synchronizer.onApplyWindowInsets(targetView, windowInsets)
        // Setup the the keyboard to have a final height of 1000px.
        val animationBounds: WindowInsetsAnimationCompat.BoundsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 200)).`when`(animationBounds).lowerBound
        doReturn(Insets.of(0, 0, 0, 1200)).`when`(animationBounds).upperBound

        synchronizer.onStart(imeAnimation, animationBounds)

        doReturn(0.01f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))

        doReturn(0.041f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))

        doReturn(0.5f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))

        doReturn(0.95f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))

        doReturn(1f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))

        verifyNoMoreInteractions(targetViewLayoutParams)
    }

    @Test
    fun `GIVEN a hide keyboard animation WHEN the progress is updated THEN update the targetView to be shown above the keyboard`() {
        // Set the initial system insets
        val windowInsets: WindowInsetsCompat = mock()
        doReturn(Insets.of(1, 2, 3, 4)).`when`(windowInsets).getInsets(ime())
        doReturn(Insets.of(10, 20, 30, 40)).`when`(windowInsets).getInsets(systemBars())
        // Set that the keyboard is hiding
        doReturn(false).`when`(windowInsets).isVisible(ime())
        val imeAnimation: WindowInsetsAnimationCompat = mock()
        doReturn(ime()).`when`(imeAnimation).typeMask
        synchronizer.onApplyWindowInsets(targetView, windowInsets)
        // Setup the the keyboard to have a final height of 1000px.
        val animationBounds: WindowInsetsAnimationCompat.BoundsCompat = mock()
        doReturn(Insets.of(0, 0, 0, 200)).`when`(animationBounds).lowerBound
        doReturn(Insets.of(0, 0, 0, 1200)).`when`(animationBounds).upperBound

        synchronizer.onStart(imeAnimation, animationBounds)

        // Test the keyboard being fully shown when the animation starts with the targetView on top
        doReturn(0f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        // Margins should have been set to 0 two times: once for onApplyWindowInsets and once now.
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 1000)

        // Test the keyboard being shown in half
        doReturn(0.5f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 500)

        // Test the keyboard almost fully hidden just above the navigation bar
        doReturn(0.99f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 9)

        // Test the keyboard fully hidden
        doReturn(1f).`when`(imeAnimation).interpolatedFraction
        synchronizer.onProgress(mock(), listOf(imeAnimation))
        // Margins should have been set to 0 two times: once for onApplyWindowInsets and once now.
        verify(targetViewLayoutParams).setMargins(0, 0, 0, 0)
    }
}

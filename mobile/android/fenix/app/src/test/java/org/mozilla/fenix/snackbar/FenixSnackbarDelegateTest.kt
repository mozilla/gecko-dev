/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.snackbar

import android.view.View
import com.google.android.material.snackbar.Snackbar.LENGTH_INDEFINITE
import com.google.android.material.snackbar.Snackbar.LENGTH_LONG
import com.google.android.material.snackbar.Snackbar.LENGTH_SHORT
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockkObject
import io.mockk.unmockkObject
import io.mockk.verify
import org.junit.After
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.snackbar.Snackbar
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.helpers.MockkRetryTestRule

private const val appName = "Firefox"
private const val editPassword = "Edit password"

class FenixSnackbarDelegateTest {

    @MockK
    private lateinit var view: View

    @MockK(relaxed = true)
    private lateinit var snackbar: Snackbar
    private lateinit var delegate: FenixSnackbarDelegate

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkObject(Snackbar.Companion)

        delegate = FenixSnackbarDelegate(view)
        every {
            Snackbar.make(view, any())
        } returns snackbar
        every { view.context.getString(R.string.app_name) } returns appName
        every { view.context.getString(R.string.edit_2) } returns editPassword
    }

    @After
    fun teardown() {
        unmockkObject(Snackbar.Companion)
    }

    @Test
    fun `GIVEN an action listener is not provided WHEN the snackbar is made THEN the snackbar's action listener is null`() {
        val snackbarState = delegate.makeSnackbarState(
            snackBarParentView = view,
            text = appName,
            duration = LENGTH_LONG,
            isError = false,
            actionText = editPassword,
            listener = null,
        )

        assertNull(snackbarState.action)
    }

    @Test
    fun `GIVEN an action string is not provided WHEN the snackbar is made THEN the snackbar's action listener is null`() {
        val snackbarState = delegate.makeSnackbarState(
            snackBarParentView = view,
            text = appName,
            duration = LENGTH_LONG,
            isError = false,
            actionText = null,
            listener = {},
        )

        assertNull(snackbarState.action)
    }

    @Test
    fun `GIVEN an action string and an action listener are not provided WHEN the snackbar state is made THEN the snackbar state's listener is null`() {
        val snackbarState = delegate.makeSnackbarState(
            snackBarParentView = view,
            text = appName,
            duration = LENGTH_LONG,
            isError = false,
            actionText = null,
            listener = null,
        )

        assertNull(snackbarState.action)
    }

    @Test
    fun `GIVEN the snackbar is an error WHEN the snackbar state is made THEN the snackbar should be the warning type`() {
        val snackbarState = delegate.makeSnackbarState(
            snackBarParentView = view,
            text = appName,
            duration = LENGTH_LONG,
            isError = true,
            actionText = null,
            listener = null,
        )

        assertTrue(snackbarState.type == SnackbarState.Type.Warning)
    }

    @Test
    fun `GIVEN the snackbar is not an error WHEN the snackbar state is made THEN the snackbar should be the default type`() {
        val snackbarState = delegate.makeSnackbarState(
            snackBarParentView = view,
            text = appName,
            duration = LENGTH_LONG,
            isError = false,
            actionText = null,
            listener = null,
        )

        assertTrue(snackbarState.type == SnackbarState.Type.Default)
    }

    @Test
    fun `WHEN the snackbar is requested THEN show snackbar is shown`() {
        delegate.show(
            text = appName,
            duration = LENGTH_LONG,
            action = null,
            listener = null,
        )

        verify { snackbar.show() }
    }

    @Test
    fun `GIVEN a snackbar is shown for indefinite duration WHEN dismiss is called THEN dismiss the snackbar`() {
        delegate.show(
            text = R.string.app_name,
            duration = LENGTH_INDEFINITE,
            action = R.string.edit_2,
            listener = null,
        )

        delegate.dismiss()

        verify { snackbar.dismiss() }
    }

    @Test
    fun `GIVEN a snackbar is shown with a short duration WHEN dismiss is called THEN dismiss the snackbar`() {
        delegate.show(
            text = R.string.app_name,
            duration = LENGTH_SHORT,
            action = R.string.edit_2,
            listener = null,
        )

        delegate.dismiss()

        verify(exactly = 0) { snackbar.dismiss() }
    }

    @Test
    fun `GIVEN an indefinite snackbar is already displayed WHEN a new indefinite snackbar is requested THEN dismiss the original snackbar`() {
        delegate.show(
            text = R.string.app_name,
            duration = LENGTH_INDEFINITE,
            action = R.string.edit_2,
            listener = null,
        )

        delegate.show(
            text = R.string.app_name,
            duration = LENGTH_INDEFINITE,
            action = R.string.edit_2,
            listener = null,
        )

        verify { snackbar.dismiss() }
    }
}

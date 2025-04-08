/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.utils.Settings

class ToolbarMenuBuilderTest {
    @Before
    fun setup() {
        mockkStatic(Context::components)
        mockkStatic(Context::bookmarkStorage)
        mockkStatic(Context::settings)
    }

    @After
    fun teardown() {
        unmockkStatic(Context::components)
        unmockkStatic(Context::bookmarkStorage)
        unmockkStatic(Context::settings)
    }

    @Test
    fun `GIVEN a valid custom tab id WHEN asked to build a toolbar menu THEN build a menu suited for custom tabs`() {
        val builder = buildToolbarMenuBuilder(
            customTabId = "test",
        )

        val menu = builder.build()

        assertTrue(menu is CustomTabToolbarMenu)
    }

    @Test
    fun `GIVEN no custom tab id WHEN asked to build a toolbar menu THEN build a menu suited for regular tabs`() {
        val builder = buildToolbarMenuBuilder(
            customTabId = null,
        )

        val menu = builder.build()

        assertTrue(menu is DefaultToolbarMenu)
    }

    private fun buildToolbarMenuBuilder(
        context: Context = mockk(relaxed = true) {
            every { this@mockk.components } returns mockk(relaxed = true)
            every { bookmarkStorage } returns mockk()
            every { settings() } returns mockk(relaxed = true)
        },
        components: Components = mockk(relaxed = true),
        settings: Settings = mockk(relaxed = true),
        interactor: BrowserToolbarInteractor = mockk(),
        lifecycleOwner: LifecycleOwner = mockk(),
        customTabId: String? = null,
    ) = ToolbarMenuBuilder(context, components, settings, interactor, lifecycleOwner, customTabId)
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import android.view.LayoutInflater
import androidx.lifecycle.LifecycleOwner
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.databinding.TopSiteItemBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.TopSiteInteractor

@RunWith(FenixRobolectricTestRunner::class)
class TopSiteItemViewHolderTest {

    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    private lateinit var binding: TopSiteItemBinding
    private lateinit var interactor: TopSiteInteractor
    private lateinit var lifecycleOwner: LifecycleOwner
    private lateinit var appStore: AppStore

    private val pocket = TopSite.Default(
        id = 1L,
        title = "Pocket",
        url = "https://getpocket.com",
        createdAt = 0,
    )

    @Before
    fun setup() {
        binding = TopSiteItemBinding.inflate(LayoutInflater.from(testContext))
        interactor = mockk(relaxed = true)
        lifecycleOwner = mockk(relaxed = true)
        appStore = mockk(relaxed = true)

        every { testContext.components.core.icons } returns BrowserIcons(testContext, mockk(relaxed = true))
    }

    @Test
    fun `calls interactor on click`() {
        TopSiteItemViewHolder(binding.root, appStore, lifecycleOwner, interactor).bind(pocket, position = 0)

        binding.root.performClick()
        verify { interactor.onSelectTopSite(pocket, position = 0) }
    }

    @Test
    fun `GIVEN a default top site WHEN bind is called THEN the title has a pin indicator`() {
        val defaultTopSite = TopSite.Default(
            id = 1L,
            title = "Pocket",
            url = "https://getpocket.com",
            createdAt = 0,
        )

        TopSiteItemViewHolder(binding.root, appStore, lifecycleOwner, interactor).bind(defaultTopSite, position = 0)
        val pinIndicator = binding.topSiteTitle.compoundDrawables[0]

        assertNotNull(pinIndicator)
    }

    @Test
    fun `GIVEN a pinned top site WHEN bind is called THEN the title has a pin indicator`() {
        val pinnedTopSite = TopSite.Pinned(
            id = 1L,
            title = "Mozilla",
            url = "https://www.mozilla.org",
            createdAt = 0,
        )

        TopSiteItemViewHolder(binding.root, appStore, lifecycleOwner, interactor).bind(pinnedTopSite, position = 0)
        val pinIndicator = binding.topSiteTitle.compoundDrawables[0]

        assertNotNull(pinIndicator)
    }

    @Test
    fun `GIVEN a frecent top site WHEN bind is called THEN the title does not have a pin indicator`() {
        val frecentTopSite = TopSite.Frecent(
            id = 1L,
            title = "Mozilla",
            url = "https://www.mozilla.org",
            createdAt = 0,
        )

        TopSiteItemViewHolder(binding.root, appStore, lifecycleOwner, interactor).bind(frecentTopSite, position = 0)
        val pinIndicator = binding.topSiteTitle.compoundDrawables[0]

        assertNull(pinIndicator)
    }
}

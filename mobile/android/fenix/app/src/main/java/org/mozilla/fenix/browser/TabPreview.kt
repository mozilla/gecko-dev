/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.util.AttributeSet
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.doOnNextLayout
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.base.images.ImageLoadRequest
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.databinding.TabPreviewBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import kotlin.math.min

/**
 * A 'dummy' view of a tab used by [ToolbarGestureHandler] to support switching tabs by swiping the address bar.
 *
 * The view is responsible for showing the preview and a dummy toolbar of the inactive tab during swiping.
 */
class TabPreview @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
) : CoordinatorLayout(context, attrs, defStyle) {

    private val binding = TabPreviewBinding.inflate(LayoutInflater.from(context), this)
    private val thumbnailLoader = ThumbnailLoader(context.components.core.thumbnailStorage)

    private var mockToolbarView: View = binding.fakeToolbar

    init {
        initializeView()
    }

    @Suppress("LongMethod")
    private fun initializeView() {
        val isToolbarAtTop = context.settings().toolbarPosition == ToolbarPosition.TOP

        binding.fakeToolbar.isVisible = true
        mockToolbarView = binding.fakeToolbar

        if (isToolbarAtTop) {
            mockToolbarView.updateLayoutParams<LayoutParams> {
                gravity = Gravity.TOP
            }
            mockToolbarView.background = AppCompatResources.getDrawable(
                context,
                ThemeManager.resolveAttribute(R.attr.bottomBarBackgroundTop, context),
            )
        }

        // Change view properties to avoid confusing the UI tests
        binding.tabButton.findViewById<View>(R.id.counter_box)?.id = View.NO_ID
        binding.tabButton.findViewById<View>(R.id.counter_text)?.id = View.NO_ID
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)

        currentOpenedTabsCount?.let {
            binding.tabButton.setCount(it)
        }

        binding.previewThumbnail.translationY = if (context.settings().toolbarPosition == ToolbarPosition.TOP) {
            mockToolbarView.height.toFloat()
        } else {
            0f
        }
    }

    /**
     * Load a preview for a thumbnail.
     */
    fun loadPreviewThumbnail(thumbnailId: String, isPrivate: Boolean) {
        doOnNextLayout {
            val previewThumbnail = binding.previewThumbnail
            val thumbnailSize = min(previewThumbnail.height, previewThumbnail.width)
            thumbnailLoader.loadIntoView(
                previewThumbnail,
                ImageLoadRequest(thumbnailId, thumbnailSize, isPrivate),
            )
        }
    }

    private val currentOpenedTabsCount: Int?
        get() {
            val store = context.components.core.store
            return store.state.selectedTab?.let {
                store.state.getNormalOrPrivateTabs(it.content.private).size
            }
        }
}

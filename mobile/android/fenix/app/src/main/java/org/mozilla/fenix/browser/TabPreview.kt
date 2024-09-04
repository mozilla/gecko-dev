/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.content.Context
import android.content.res.Configuration
import android.util.AttributeSet
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.compose.foundation.layout.Column
import androidx.compose.ui.viewinterop.AndroidView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.doOnNextLayout
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import mozilla.components.browser.menu.view.MenuButton
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.BottomToolbarContainerView
import org.mozilla.fenix.components.toolbar.NewTabMenu
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.components.toolbar.navbar.BrowserNavBar
import org.mozilla.fenix.components.toolbar.navbar.shouldAddNavigationBar
import org.mozilla.fenix.components.toolbar.navbar.updateNavBarForConfigurationChange
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.databinding.TabPreviewBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.isTablet
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme
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

    private var bottomToolbarContainerView: BottomToolbarContainerView? = null

    init {
        initializeView()
    }

    private fun initializeView() {
        val isNavBarVisible = context.shouldAddNavigationBar()
        val isToolbarAtTop = context.settings().toolbarPosition == ToolbarPosition.TOP

        binding.fakeToolbar.isVisible = !isNavBarVisible
        binding.toolbarWrapperTwo.isVisible = isNavBarVisible

        if (isToolbarAtTop) {
            val toolbarBinding = if (isNavBarVisible) binding.toolbarWrapperTwo else binding.fakeToolbar

            toolbarBinding.updateLayoutParams<LayoutParams> {
                gravity = Gravity.TOP
            }
            toolbarBinding.background = AppCompatResources.getDrawable(
                context,
                ThemeManager.resolveAttribute(R.attr.bottomBarBackgroundTop, context),
            )
        }

        if (isNavBarVisible) {
            val browserStore = context.components.core.store
            bottomToolbarContainerView = BottomToolbarContainerView(
                context = context,
                parent = this,
                content = {
                    FirefoxTheme {
                        Column {
                            if (!isToolbarAtTop) {
                                // before adding fake navigation bar in the preview, remove fake toolbar
                                removeView(binding.fakeToolbar)
                                AndroidView(factory = { _ -> binding.fakeToolbar })
                            } else {
                                Divider()
                            }

                            BrowserNavBar(
                                isPrivateMode = browserStore.state.selectedTab?.content?.private ?: false,
                                browserStore = browserStore,
                                menuButton = MenuButton(context).apply {
                                    setColorFilter(
                                        ContextCompat.getColor(
                                            context,
                                            ThemeManager.resolveAttribute(R.attr.textPrimary, context),
                                        ),
                                    )
                                },
                                newTabMenu = NewTabMenu(context, onItemTapped = {}),
                                tabsCounterMenu = lazy { TabCounterMenu(context, onItemTapped = {}) },
                                onBackButtonClick = {
                                    // no-op
                                },
                                onBackButtonLongPress = {
                                    // no-op
                                },
                                onForwardButtonClick = {
                                    // no-op
                                },
                                onForwardButtonLongPress = {
                                    // no-op
                                },
                                onNewTabButtonClick = {
                                    // no-op
                                },
                                onNewTabButtonLongPress = {
                                    // no-op
                                },
                                onTabsButtonClick = {
                                    // no-op
                                },
                                onTabsButtonLongPress = {
                                    // no-op
                                },
                                onMenuButtonClick = {
                                    // no-op
                                },
                                onVisibilityUpdated = {
                                    // no-op
                                },
                            )
                        }
                    }
                },
            )
        }

        // Change view properties to avoid confusing the UI tests
        binding.tabButton.findViewById<View>(R.id.counter_box)?.id = View.NO_ID
        binding.tabButton.findViewById<View>(R.id.counter_text)?.id = View.NO_ID
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)

        val store = context.components.core.store
        store.state.selectedTab?.let {
            val count = store.state.getNormalOrPrivateTabs(it.content.private).size
            binding.tabButton.setCount(count)
        }

        binding.previewThumbnail.translationY = if (context.settings().toolbarPosition == ToolbarPosition.TOP) {
            binding.fakeToolbar.height.toFloat()
        } else {
            0f
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        if (context.settings().navigationToolbarEnabled && !context.isTablet()) {
            updateNavBarForConfigurationChange(
                context = context,
                parent = this,
                toolbarView = binding.fakeToolbar,
                bottomToolbarContainerView = bottomToolbarContainerView?.toolbarContainerView,
                reinitializeNavBar = ::initializeView,
                reinitializeMicrosurveyPrompt = {},
            )
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
}

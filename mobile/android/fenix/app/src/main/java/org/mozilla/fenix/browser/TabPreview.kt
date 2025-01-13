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
import mozilla.components.compose.base.theme.AcornWindowSize
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.BottomToolbarContainerView
import org.mozilla.fenix.components.toolbar.NewTabMenu
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.components.toolbar.navbar.BrowserNavBar
import org.mozilla.fenix.components.toolbar.navbar.shouldAddNavigationBar
import org.mozilla.fenix.components.toolbar.navbar.updateNavBarForConfigurationChange
import org.mozilla.fenix.databinding.TabPreviewBinding
import org.mozilla.fenix.ext.components
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
    private val browserStore = context.components.core.store

    private var bottomToolbarContainerView: BottomToolbarContainerView? = null
    private var mockToolbarView: View = binding.fakeToolbar

    init {
        initializeView()
    }

    @Suppress("LongMethod")
    private fun initializeView() {
        val isNavBarVisible = context.shouldAddNavigationBar()
        val isNavBarEnabled = context.settings().navigationToolbarEnabled
        val isLargeWindow = (AcornWindowSize.getWindowSize(context).isNotSmall())
        val isToolbarAtTop = context.settings().toolbarPosition == ToolbarPosition.TOP

        binding.fakeToolbar.isVisible = !isNavBarEnabled
        binding.fakeToolbarTwo.isVisible = isNavBarEnabled
        mockToolbarView = if (isNavBarEnabled) binding.fakeToolbarTwo else binding.fakeToolbar
        initNavBarLandscapeChanges(isNavBarEnabled && isLargeWindow)

        if (isToolbarAtTop) {
            mockToolbarView.updateLayoutParams<LayoutParams> {
                gravity = Gravity.TOP
            }
            mockToolbarView.background = AppCompatResources.getDrawable(
                context,
                ThemeManager.resolveAttribute(R.attr.bottomBarBackgroundTop, context),
            )
        }

        if (isNavBarVisible) {
            bottomToolbarContainerView = BottomToolbarContainerView(
                context = context,
                parent = this,
                content = {
                    FirefoxTheme {
                        Column {
                            if (!isToolbarAtTop) {
                                // before adding fake navigation bar in the preview, remove fake toolbar
                                removeView(mockToolbarView)
                                AndroidView(factory = { _ -> mockToolbarView })
                            }

                            BrowserNavBar(
                                isPrivateMode = browserStore.state.selectedTab?.content?.private ?: false,
                                showDivider = isToolbarAtTop,
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

        currentOpenedTabsCount?.let {
            binding.tabButton.setCount(it)
        }

        binding.previewThumbnail.translationY = if (context.settings().toolbarPosition == ToolbarPosition.TOP) {
            mockToolbarView.height.toFloat()
        } else {
            0f
        }
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        if (context.settings().navigationToolbarEnabled) {
            val isLargeWindow = (AcornWindowSize.getWindowSize(context).isNotSmall())

            initNavBarLandscapeChanges(isLargeWindow)

            updateNavBarForConfigurationChange(
                context = context,
                parent = this,
                toolbarView = mockToolbarView,
                bottomToolbarContainerView = bottomToolbarContainerView?.toolbarContainerView,
                reinitializeNavBar = ::initializeView,
                reinitializeMicrosurveyPrompt = {},
            )
        }
    }

    /**
     * Changes the visibility of the landscape changes to the Toolbar if Navigation Toolbar
     * is active based on layout.
     */
    private fun initNavBarLandscapeChanges(isLargeWindow: Boolean) {
        val isFeltPrivacyEnabled = context.settings().feltPrivateBrowsingEnabled
        val isInPrivateMode = browserStore.state.selectedTab?.content?.private ?: false
        binding.fakeClearDataButton.isVisible = isFeltPrivacyEnabled && isLargeWindow && isInPrivateMode

        binding.fakeBackButton.isVisible = isLargeWindow
        binding.fakeForwardButton.isVisible = isLargeWindow
        binding.fakeNewTabButton.isVisible = isLargeWindow
        binding.fakeTabCounter.isVisible = isLargeWindow
        binding.fakeMenuButton.isVisible = isLargeWindow

        if (isLargeWindow) {
            currentOpenedTabsCount?.let {
                binding.fakeTabCounter.setCount(it)
            }
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

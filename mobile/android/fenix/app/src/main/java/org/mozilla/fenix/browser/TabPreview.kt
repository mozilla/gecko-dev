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
import androidx.compose.foundation.layout.Box
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.doOnNextLayout
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import mozilla.components.browser.state.selector.getNormalOrPrivateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.thumbnails.loader.ThumbnailLoader
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.BrowserToolbar
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.TabCounterAction
import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.PageOriginUpdated
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarStore
import mozilla.components.compose.browser.toolbar.store.DisplayState
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.support.ktx.kotlin.isContentUrl
import mozilla.components.support.ktx.util.URLStringUtils
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

    private lateinit var mockToolbarView: View
    private val browserToolbarStore: BrowserToolbarStore by lazy(LazyThreadSafetyMode.NONE) {
        buildComposableToolbarStore()
    }

    init {
        initializeView()
    }

    @Suppress("LongMethod")
    private fun initializeView() {
        bindToolbar()

        val isToolbarAtTop = context.settings().toolbarPosition == ToolbarPosition.TOP
        if (isToolbarAtTop) {
            mockToolbarView.updateLayoutParams<LayoutParams> {
                gravity = Gravity.TOP
            }
        }
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)

        updateToolbar(
            new = {},
            old = { binding.tabButton.setCount(currentOpenedTabsCount) },
        )

        binding.previewThumbnail.translationY = if (context.settings().toolbarPosition == ToolbarPosition.TOP) {
            mockToolbarView.height.toFloat()
        } else {
            0f
        }
    }

    /**
     * Load a preview for a thumbnail.
     */
    fun loadDestinationPreview(destination: TabSessionState) {
        doOnNextLayout {
            val previewThumbnail = binding.previewThumbnail
            val thumbnailSize = min(previewThumbnail.height, previewThumbnail.width)
            thumbnailLoader.loadIntoView(
                previewThumbnail,
                ImageLoadRequest(destination.id, thumbnailSize, destination.content.private),
            )

            updateToolbar(
                new = {
                    browserToolbarStore.dispatch(
                        PageOriginUpdated(
                            buildComposableToolbarPageOrigin(destination),
                        ),
                    )
                    browserToolbarStore.dispatch(
                        BrowserDisplayToolbarAction.PageActionsStartUpdated(
                            buildComposableToolbarPageStartActions(destination),
                        ),
                    )
                },
                old = {},
            )
        }
    }

    private val currentOpenedTabsCount: Int
        get() {
            val store = context.components.core.store
            return store.state.selectedTab?.let {
                store.state.getNormalOrPrivateTabs(it.content.private).size
            } ?: store.state.tabs.size
        }

    private fun bindToolbar() {
        mockToolbarView = updateToolbar(
            new = { buildComposableToolbar() },
            old = { buildToolbarView() },
        )
    }

    private fun buildToolbarView(): View {
        // Change view properties to avoid confusing the UI tests
        binding.tabButton.findViewById<View>(R.id.counter_box)?.id = NO_ID
        binding.tabButton.findViewById<View>(R.id.counter_text)?.id = NO_ID

        binding.fakeToolbar.isVisible = true
        binding.fakeToolbar.background = AppCompatResources.getDrawable(
            context,
            ThemeManager.resolveAttribute(R.attr.bottomBarBackgroundTop, context),
        )

        return binding.fakeToolbar
    }

    private fun buildComposableToolbar(): ComposeView {
        return binding.composableToolbar.apply {
            setContent {
                AcornTheme {
                    // Ensure the divider is shown together with the toolbar
                    Box {
                        BrowserToolbar(
                            store = browserToolbarStore,
                        )

                        Divider(
                            modifier = Modifier.align(
                                when (context.settings().shouldUseBottomToolbar) {
                                    true -> Alignment.TopCenter
                                    false -> Alignment.BottomCenter
                                },
                            ),
                        )
                    }
                }
            }.apply {
                isVisible = true
            }
        }
    }

    private fun buildComposableToolbarStore(): BrowserToolbarStore {
        val tabsCount = currentOpenedTabsCount

        return BrowserToolbarStore(
            BrowserToolbarState(
                displayState = DisplayState(
                    browserActionsStart = listOf(
                        ActionButton(
                            icon = R.drawable.mozac_ic_home_24,
                            contentDescription = R.string.browser_toolbar_home,
                            onClick = object : BrowserToolbarEvent {},
                        ),
                    ),
                    browserActionsEnd = listOf(
                        TabCounterAction(
                            count = tabsCount,
                            contentDescription = context.getString(
                                R.string.mozac_tab_counter_open_tab_tray, tabsCount.toString(),
                            ),
                            showPrivacyMask = context.components.core.store.state.selectedTab?.content?.private == true,
                            onClick = object : BrowserToolbarEvent {},
                        ),
                        ActionButton(
                            icon = R.drawable.mozac_ic_ellipsis_vertical_24,
                            contentDescription = R.string.content_description_menu,
                            onClick = object : BrowserToolbarEvent {},
                        ),
                    ),
                ),
            ),
        )
    }

    private fun buildComposableToolbarPageStartActions(tab: TabSessionState) = buildList {
        if (tab.content.url.isContentUrl() == true) {
            add(
                ActionButton(
                    icon = R.drawable.mozac_ic_page_portrait_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = object : BrowserToolbarEvent {},
                ),
            )
        } else if (tab.content.securityInfo.secure == true) {
            add(
                ActionButton(
                    icon = R.drawable.mozac_ic_lock_24,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = object : BrowserToolbarEvent {},
                ),
            )
        } else {
            add(
                ActionButton(
                    icon = R.drawable.mozac_ic_broken_lock,
                    contentDescription = R.string.mozac_browser_toolbar_content_description_site_info,
                    onClick = object : BrowserToolbarEvent {},
                ),
            )
        }
    }

    private fun buildComposableToolbarPageOrigin(tab: TabSessionState): PageOrigin {
        val urlString = URLStringUtils.toDisplayUrl(tab.content.url).toString()

        return PageOrigin(
            hint = R.string.search_hint,
            title = null,
            url = urlString,
            onClick = object : BrowserToolbarEvent {},
        )
    }

    /**
     * Pass in the desired configuration for both the `new` composable toolbar and the `old` toolbar View
     * with this method then deciding what to use depending on the actual toolbar currently is use.
     */
    private inline fun <T> updateToolbar(
        new: () -> T,
        old: () -> T,
    ): T = when (context.settings().shouldUseComposableToolbar) {
        true -> new()
        false -> old()
    }
}

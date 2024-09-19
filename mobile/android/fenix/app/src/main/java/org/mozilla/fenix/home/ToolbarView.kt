/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.Gravity
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.constraintlayout.widget.ConstraintSet
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.isVisible
import androidx.core.view.updateLayoutParams
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.utils.ext.isLandscape
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.toolbar.ToolbarPosition
import org.mozilla.fenix.components.toolbar.navbar.shouldAddNavigationBar
import org.mozilla.fenix.databinding.FragmentHomeBinding
import org.mozilla.fenix.ext.increaseTapAreaVertically
import org.mozilla.fenix.ext.isTablet
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.toolbar.ToolbarInteractor
import org.mozilla.fenix.utils.ToolbarPopupWindow
import java.lang.ref.WeakReference

/**
 * View class for setting up the home screen toolbar.
 */
class ToolbarView(
    private val binding: FragmentHomeBinding,
    private val interactor: ToolbarInteractor,
    private val homeFragment: HomeFragment,
    private val homeActivity: HomeActivity,
) {

    private var context = homeFragment.requireContext()

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var tabCounterView: TabCounterView? = null

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal var homeMenuView: HomeMenuView? = null

    init {
        initLayoutParameters()
        updateMargins()
    }

    /**
     * Setups the home screen toolbar.
     *
     * @param browserState [BrowserState] is used to update button visibility.
     */
    fun build(browserState: BrowserState) {
        binding.toolbar.compoundDrawablePadding =
            context.resources.getDimensionPixelSize(R.dimen.search_bar_search_engine_icon_padding)

        binding.toolbarWrapper.setOnClickListener {
            interactor.onNavigateSearch()
        }

        binding.toolbarWrapper.setOnLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(it),
                WeakReference(binding.dynamicSnackbarContainer),
                handlePasteAndGo = interactor::onPasteAndGo,
                handlePaste = interactor::onPaste,
                copyVisible = false,
            )
            true
        }

        binding.toolbarWrapper.increaseTapAreaVertically(TOOLBAR_WRAPPER_INCREASE_HEIGHT_DPS)

        updateButtonVisibility(browserState)
    }

    /**
     * Updates the visibility of the tab counter and menu buttons.
     *
     * @param browserState [BrowserState] is used to update tab counter's state.
     */
    fun updateButtonVisibility(browserState: BrowserState) {
        val showTabCounterAndMenu = !context.shouldAddNavigationBar()
        binding.menuButton.isVisible = showTabCounterAndMenu
        binding.tabButton.isVisible = showTabCounterAndMenu

        if (showTabCounterAndMenu) {
            homeMenuView = buildHomeMenu()
            tabCounterView = buildTabCounter()
            tabCounterView?.update(browserState)
        } else {
            homeMenuView = null
            tabCounterView = null
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun buildHomeMenu() = HomeMenuView(
        context = context,
        lifecycleOwner = homeFragment.viewLifecycleOwner,
        homeActivity = homeActivity,
        navController = homeFragment.findNavController(),
        homeFragment = homeFragment,
        menuButton = WeakReference(binding.menuButton),
    ).also { it.build() }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun buildTabCounter() = TabCounterView(
        context = context,
        browsingModeManager = homeActivity.browsingModeManager,
        navController = homeFragment.findNavController(),
        tabCounter = binding.tabButton,
        showLongPressMenu = !(context.settings().navigationToolbarEnabled && context.isTablet()),
    )

    /**
     * Dismisses the home menu.
     */
    fun dismissMenu() {
        homeMenuView?.dismissMenu()
    }

    /**
     * Updates the tab counter view based on the current browser state.
     *
     * @param browserState [BrowserState] is passed down to tab counter view to calculate the view state.
     */
    fun updateTabCounter(browserState: BrowserState) {
        tabCounterView?.update(browserState)
    }

    private fun initLayoutParameters() {
        when (context.settings().toolbarPosition) {
            ToolbarPosition.TOP -> {
                binding.toolbarLayout.layoutParams = CoordinatorLayout.LayoutParams(
                    ConstraintLayout.LayoutParams.MATCH_PARENT,
                    ConstraintLayout.LayoutParams.WRAP_CONTENT,
                ).apply {
                    gravity = Gravity.TOP
                }

                val isTabletAndTabStripEnabled = context.isTabStripEnabled()
                ConstraintSet().apply {
                    clone(binding.toolbarLayout)
                    clear(binding.bottomBar.id, ConstraintSet.BOTTOM)
                    clear(binding.bottomBarShadow.id, ConstraintSet.BOTTOM)

                    if (isTabletAndTabStripEnabled) {
                        connect(
                            binding.bottomBar.id,
                            ConstraintSet.TOP,
                            binding.tabStripView.id,
                            ConstraintSet.BOTTOM,
                        )
                    } else {
                        connect(
                            binding.bottomBar.id,
                            ConstraintSet.TOP,
                            ConstraintSet.PARENT_ID,
                            ConstraintSet.TOP,
                        )
                    }
                    connect(
                        binding.bottomBarShadow.id,
                        ConstraintSet.TOP,
                        binding.bottomBar.id,
                        ConstraintSet.BOTTOM,
                    )
                    connect(
                        binding.bottomBarShadow.id,
                        ConstraintSet.BOTTOM,
                        ConstraintSet.PARENT_ID,
                        ConstraintSet.BOTTOM,
                    )
                    applyTo(binding.toolbarLayout)
                }

                binding.bottomBar.background = AppCompatResources.getDrawable(
                    context,
                    context.theme.resolveAttribute(R.attr.bottomBarBackgroundTop),
                )

                binding.homeAppBar.updateLayoutParams<ViewGroup.MarginLayoutParams> {
                    topMargin =
                        context.resources.getDimensionPixelSize(R.dimen.home_fragment_top_toolbar_header_margin) +
                        if (isTabletAndTabStripEnabled) {
                            context.resources.getDimensionPixelSize(R.dimen.tab_strip_height)
                        } else {
                            0
                        }
                }
            }

            ToolbarPosition.BOTTOM -> {}
        }
    }

    private fun updateMargins() {
        if (context.settings().navigationToolbarEnabled) {
            val marginStart = context.resources.getDimensionPixelSize(R.dimen.toolbar_horizontal_margin)
            val marginEnd = if (context.isLandscape() || context.isTablet()) {
                context.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_short_margin)
            } else {
                context.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
            }

            (binding.toolbarWrapper.layoutParams as ConstraintLayout.LayoutParams).apply {
                this.marginStart = marginStart
                this.marginEnd = marginEnd
            }
        }
    }

    companion object {
        const val TOOLBAR_WRAPPER_INCREASE_HEIGHT_DPS = 4
    }
}

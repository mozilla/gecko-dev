/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.tabs.toolbar.TabCounterToolbarButton
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.feature.toolbar.ToolbarPresenter
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.ui.tabcounter.TabCounterMenu
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.AddressToolbar
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.tabstrip.isTabStripEnabled
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.components.toolbar.navbar.shouldAddNavigationBar
import org.mozilla.fenix.components.toolbar.ui.createShareBrowserAction
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.isLargeWindow
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager

/**
 * Feature configuring the toolbar when in display mode.
 */
@SuppressWarnings("LongParameterList")
abstract class ToolbarIntegration(
    private val context: Context,
    private val toolbar: BrowserToolbar,
    scrollableToolbar: ScrollableToolbar,
    toolbarMenu: ToolbarMenu,
    private val interactor: BrowserToolbarInteractor,
    private val customTabId: String?,
    isPrivate: Boolean,
    renderStyle: ToolbarFeature.RenderStyle,
) : LifecycleAwareFeature {

    val store = context.components.core.store
    private val toolbarPresenter: ToolbarPresenter = ToolbarPresenter(
        toolbar = toolbar,
        store = store,
        customTabId = customTabId,
        shouldDisplaySearchTerms = true,
        urlRenderConfiguration = ToolbarFeature.UrlRenderConfiguration(
            context.components.publicSuffixList,
            ThemeManager.resolveAttribute(R.attr.textPrimary, context),
            renderStyle = renderStyle,
        ),
    )

    private val menuPresenter =
        MenuPresenter(toolbar, context.components.core.store, customTabId)

    private val toolbarController = ToolbarBehaviorController(scrollableToolbar, store, customTabId)

    init {
        if (!context.settings().enableMenuRedesign) {
            toolbar.display.menuBuilder = toolbarMenu.menuBuilder
        }

        toolbar.private = isPrivate

        if (context.settings().enableMenuRedesign && customTabId == null) {
            addMenuBrowserAction()
        }
    }

    override fun start() {
        menuPresenter.start()
        toolbarPresenter.start()
        toolbarController.start()
    }

    override fun stop() {
        menuPresenter.stop()
        toolbarPresenter.stop()
        toolbarController.stop()
    }

    fun invalidateMenu() {
        menuPresenter.invalidateActions()
    }

    private fun addMenuBrowserAction() {
        val menuAction = Toolbar.ActionButton(
            imageDrawable = AppCompatResources.getDrawable(
                context,
                R.drawable.mozac_ic_ellipsis_vertical_24,
            )!!,
            contentDescription = context.getString(R.string.content_description_menu),
            visible = {
                context.settings().enableMenuRedesign && !context.shouldAddNavigationBar()
            },
            weight = { Int.MAX_VALUE },
            iconTintColorResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
            listener = {
                val accessPoint = if (customTabId.isNullOrBlank()) {
                    MenuAccessPoint.Browser
                } else {
                    MenuAccessPoint.External
                }

                interactor.onMenuButtonClicked(accessPoint = accessPoint)
            },
        )

        toolbar.addBrowserAction(menuAction)
    }
}

@SuppressWarnings("LongParameterList")
class DefaultToolbarIntegration(
    private val context: Context,
    private val toolbar: BrowserToolbar,
    scrollableToolbar: ScrollableToolbar,
    toolbarMenu: ToolbarMenu,
    private val lifecycleOwner: LifecycleOwner,
    customTabId: String? = null,
    private val isPrivate: Boolean,
    private val interactor: BrowserToolbarInteractor,
) : ToolbarIntegration(
    context = context,
    toolbar = toolbar,
    scrollableToolbar = scrollableToolbar,
    toolbarMenu = toolbarMenu,
    interactor = interactor,
    customTabId = customTabId,
    isPrivate = isPrivate,
    renderStyle = ToolbarFeature.RenderStyle.UncoloredUrl,
) {

    @VisibleForTesting
    internal var cfrPresenter = BrowserToolbarCFRPresenter(
        context = context,
        browserStore = context.components.core.store,
        settings = context.settings(),
        toolbar = toolbar,
        isPrivate = isPrivate,
        customTabId = customTabId,
    )

    init {
        toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.EMPTY,
            DisplayToolbar.Indicators.HIGHLIGHT,
        )

        if (context.isTabStripEnabled()) {
            addShareBrowserAction()
        } else {
            addNewTabBrowserAction()
            addTabCounterBrowserAction()
        }
    }

    private fun addNewTabBrowserAction() {
        val newTabAction = BrowserToolbar.Button(
            imageDrawable = AppCompatResources.getDrawable(context, R.drawable.mozac_ic_plus_24)!!,
            contentDescription = context.getString(R.string.library_new_tab),
            visible = {
                context.settings().navigationToolbarEnabled && !context.shouldAddNavigationBar()
            },
            weight = { NEW_TAB_ACTION_WEIGHT },
            iconTintColorResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
            listener = interactor::onNewTabButtonClicked,
        )

        toolbar.addBrowserAction(newTabAction)
    }

    private fun addTabCounterBrowserAction() {
        val tabCounterAction = TabCounterToolbarButton(
            lifecycleOwner = lifecycleOwner,
            showTabs = {
                toolbar.hideKeyboard()
                interactor.onTabCounterClicked()
            },
            store = store,
            menu = buildTabCounterMenu(),
            visible = { !context.shouldAddNavigationBar() },
            weight = { TAB_COUNTER_ACTION_WEIGHT },
        )

        val tabCount = if (isPrivate) {
            store.state.privateTabs.size
        } else {
            store.state.normalTabs.size
        }

        tabCounterAction.updateCount(tabCount)

        toolbar.addBrowserAction(tabCounterAction)
    }

    private fun addShareBrowserAction() {
        toolbar.addBrowserAction(
            BrowserToolbar.createShareBrowserAction(
                context = context,
                listener = {
                    AddressToolbar.shareTapped.record((NoExtras()))
                    interactor.onShareActionClicked()
                },
            ),
        )
    }

    override fun start() {
        super.start()
        cfrPresenter.start()
    }

    override fun stop() {
        cfrPresenter.stop()
        super.stop()
    }

    private fun buildTabCounterMenu(): TabCounterMenu? =
        when ((context.settings().navigationToolbarEnabled && context.isLargeWindow())) {
            true -> null
            false -> FenixTabCounterMenu(
                context = context,
                onItemTapped = {
                    interactor.onTabCounterMenuItemTapped(it)
                },
                iconColor = if (isPrivate) {
                    ContextCompat.getColor(context, R.color.fx_mobile_private_icon_color_primary)
                } else {
                    null
                },
            ).also {
                it.updateMenu(context.settings().toolbarPosition)
            }
        }

    companion object {
        private const val NEW_TAB_ACTION_WEIGHT = 1
        private const val TAB_COUNTER_ACTION_WEIGHT = 2
    }
}

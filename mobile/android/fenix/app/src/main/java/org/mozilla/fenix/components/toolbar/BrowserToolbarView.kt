/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.support.ktx.util.URLStringUtils
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.components.usecases.FenixBrowserUseCases.Companion.ABOUT_HOME
import org.mozilla.fenix.customtabs.CustomTabToolbarIntegration
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.ToolbarPopupWindow
import java.lang.ref.WeakReference

/**
 * A wrapper over [BrowserToolbar] to allow extra customisation and behavior.
 *
 * @param context [Context] used for various system interactions.
 * @param container [ViewGroup] which will serve as parent of this View.
 * @param snackbarParent [ViewGroup] in which new snackbars will be shown.
 * @param settings [Settings] object to get the toolbar position and other settings.
 * @param interactor [BrowserToolbarInteractor] to handle toolbar interactions.
 * @param customTabSession [CustomTabSessionState] if the toolbar is shown in a custom tab.
 * @param lifecycleOwner View lifecycle owner used to determine when to cancel UI jobs.
 * @param tabStripContent Composable content for the tab strip.
 */
@SuppressWarnings("LargeClass", "LongParameterList")
class BrowserToolbarView(
    private val context: Context,
    container: ViewGroup,
    private val snackbarParent: ViewGroup,
    private val settings: Settings,
    private val interactor: BrowserToolbarInteractor,
    private val customTabSession: CustomTabSessionState?,
    private val lifecycleOwner: LifecycleOwner,
    private val tabStripContent: @Composable () -> Unit,
) : FenixBrowserToolbarView(
    context = context,
    settings = settings,
    customTabSession = customTabSession,
) {

    @LayoutRes
    private val toolbarLayout = when (settings.toolbarPosition) {
        ToolbarPosition.BOTTOM -> R.layout.component_bottom_browser_toolbar
        ToolbarPosition.TOP -> if (shouldShowTabStrip()) {
            R.layout.component_browser_top_toolbar_with_tab_strip
        } else {
            R.layout.component_browser_top_toolbar
        }
    }

    override val layout = LayoutInflater.from(context)
        .inflate(toolbarLayout, container, false)

    @set:VisibleForTesting
    var toolbar: BrowserToolbar = layout.findViewById(R.id.toolbar)

    val toolbarIntegration: ToolbarIntegration

    val menuToolbar: ToolbarMenu

    init {
        container.addView(layout)
        val isCustomTabSession = customTabSession != null

        if (toolbarLayout == R.layout.component_browser_top_toolbar_with_tab_strip) {
            layout.findViewById<ComposeView>(R.id.tabStripView).apply {
                setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)
                setContent {
                    tabStripContent()
                }
            }
        }

        toolbar.display.setOnUrlLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(toolbar),
                WeakReference(snackbarParent),
                customTabSession?.id,
                interactor::onBrowserToolbarPasteAndGo,
                interactor::onBrowserToolbarPaste,
            )
            true
        }

        with(context) {
            layout.elevation =
                resources.getDimension(R.dimen.browser_fragment_toolbar_elevation)

            toolbar.apply {
                setToolbarBehavior(settings.toolbarPosition)
                setDisplayToolbarColors()

                if (!isCustomTabSession) {
                    display.setUrlBackground(
                        AppCompatResources.getDrawable(
                            this@with,
                            R.drawable.search_url_background,
                        ),
                    )
                }

                display.onUrlClicked = {
                    interactor.onBrowserToolbarClicked()
                    false
                }

                display.progressGravity = when (settings.toolbarPosition) {
                    ToolbarPosition.BOTTOM -> DisplayToolbar.Gravity.TOP
                    ToolbarPosition.TOP -> DisplayToolbar.Gravity.BOTTOM
                }

                display.urlFormatter = { url ->
                    if (url.contentEquals(ABOUT_HOME)) {
                        // Default to showing the toolbar hint when the URL is ABOUT_HOME.
                        ""
                    } else {
                        URLStringUtils.toDisplayUrl(url)
                    }
                }

                display.hint = context.getString(R.string.search_hint)
            }

            menuToolbar = ToolbarMenuBuilder(
                context = this,
                components = components,
                settings = settings,
                interactor = interactor,
                lifecycleOwner = lifecycleOwner,
                customTabSessionId = customTabSession?.id,
            ).build()
            if (!isCustomTabSession) {
                toolbar.display.setMenuDismissAction {
                    toolbar.invalidateActions()
                }
            }

            toolbarIntegration = if (customTabSession != null) {
                CustomTabToolbarIntegration(
                    context = this,
                    toolbar = toolbar,
                    scrollableToolbar = toolbar as ScrollableToolbar,
                    toolbarMenu = menuToolbar,
                    interactor = interactor,
                    customTabId = customTabSession.id,
                    isPrivate = customTabSession.content.private,
                )
            } else {
                DefaultToolbarIntegration(
                    context = this,
                    toolbar = toolbar,
                    scrollableToolbar = this@BrowserToolbarView,
                    toolbarMenu = menuToolbar,
                    lifecycleOwner = lifecycleOwner,
                    isPrivate = components.core.store.state.selectedTab?.content?.private ?: false,
                    interactor = interactor,
                )
            }
        }
    }

    override fun updateDividerVisibility(isVisible: Boolean) = toolbar.setBackgroundResource(
        when (isVisible) {
            true -> {
                when (settings.shouldUseBottomToolbar) {
                    true -> R.drawable.toolbar_background
                    false -> R.drawable.toolbar_background_top
                }
            }

            false -> R.drawable.toolbar_background_no_divider
        },
    )

    /**
     * Hides the menu button of the toolbar.
     */
    fun dismissMenu() {
        toolbar.dismissMenu()
    }

    /**
     * Updates the visibility of the menu in the toolbar.
     */
    fun updateMenuVisibility(isVisible: Boolean) {
        with(toolbar) {
            if (isVisible) {
                showMenuButton()
                setDisplayHorizontalPadding(0)
            } else {
                hideMenuButton()
                setDisplayHorizontalPadding(
                    context.resources.getDimensionPixelSize(R.dimen.browser_fragment_display_toolbar_padding),
                )
            }
        }
    }

    private fun setDisplayToolbarColors() {
        val primaryTextColor = ContextCompat.getColor(
            context,
            ThemeManager.resolveAttribute(R.attr.textPrimary, context),
        )
        val secondaryTextColor = ContextCompat.getColor(
            context,
            ThemeManager.resolveAttribute(R.attr.textSecondary, context),
        )
        val separatorColor = ContextCompat.getColor(
            context,
            ThemeManager.resolveAttribute(R.attr.borderPrimary, context),
        )

        toolbar.display.colors = toolbar.display.colors.copy(
            text = primaryTextColor,
            siteInfoIconSecure = primaryTextColor,
            siteInfoIconInsecure = primaryTextColor,
            siteInfoIconLocalPdf = primaryTextColor,
            menu = primaryTextColor,
            hint = secondaryTextColor,
            separator = separatorColor,
            trackingProtection = primaryTextColor,
            highlight = ContextCompat.getColor(
                context,
                R.color.fx_mobile_icon_color_information,
            ),
        )
    }
}

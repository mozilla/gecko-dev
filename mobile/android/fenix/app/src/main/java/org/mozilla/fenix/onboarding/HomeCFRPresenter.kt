/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import android.content.Context
import android.view.View
import androidx.compose.material.Text
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat.getColor
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.compose.cfr.CFRPopup
import mozilla.components.compose.cfr.CFRPopupProperties
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.home.recentsyncedtabs.view.RecentSyncedTabViewHolder
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Vertical padding needed to improve the visual alignment of the popup and respect the UX design.
 */
private const val CFR_TO_ANCHOR_VERTICAL_PADDING = -16

/**
 * Delegate for handling the Home Onboarding CFR.
 *
 * @param context [Context] used for various Android interactions.
 * @param recyclerView [RecyclerView] will serve as anchor for the CFR.
 */
class HomeCFRPresenter(
    private val context: Context,
    private val recyclerView: RecyclerView,
) {

    /**
     * Determine the CFR to be shown on the Home screen and show a CFR for the resultant view
     * if any.
     */
    fun show(): Boolean {
        return when (val result = getCFRToShow()) {
            is Result.SyncedTab -> {
                showSyncedTabCFR(view = result.view)
                true
            }
            else -> false
        }
    }

    @OptIn(ExperimentalComposeUiApi::class)
    private fun showSyncedTabCFR(view: View) {
        CFRPopup(
            anchor = view,
            properties = CFRPopupProperties(
                popupBodyColors = listOf(
                    getColor(context, R.color.fx_mobile_layer_color_gradient_end),
                    getColor(context, R.color.fx_mobile_layer_color_gradient_start),
                ),
                popupVerticalOffset = CFR_TO_ANCHOR_VERTICAL_PADDING.dp,
                dismissButtonColor = getColor(context, R.color.fx_mobile_icon_color_oncolor),
                indicatorDirection = CFRPopup.IndicatorDirection.DOWN,
            ),
            onDismiss = {
                when (it) {
                    true -> {
                        Onboarding.syncCfrExplicitDismissal.record(NoExtras())
                        // Turn off the synced tab CFR after the CFR is shown.
                        context.settings().showSyncCFR = false
                    }
                    false -> Onboarding.syncCfrImplicitDismissal.record(NoExtras())
                }
            },
            text = {
                FirefoxTheme {
                    Text(
                        text = context.getString(R.string.sync_cfr_message),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.body2,
                        modifier = Modifier
                            .semantics {
                                testTagsAsResourceId = true
                                testTag = "sync_cfr.message"
                            },
                    )
                }
            },
        ).show()

        Onboarding.synCfrShown.record(NoExtras())
    }

    /**
     * Returns a [Result] that indicates the CFR that should be shown on the Home screen if any
     * based on the views available and the preferences.
     */
    private fun getCFRToShow(): Result {
        var result: Result = Result.None
        val count = recyclerView.adapter?.itemCount ?: return result

        if (context.settings().navigationToolbarEnabled && context.settings().shouldShowNavigationBarCFR) {
            return result
        }

        for (index in count downTo 0) {
            val viewHolder = recyclerView.findViewHolderForAdapterPosition(index)

            if (context.settings().showSyncCFR && viewHolder is RecentSyncedTabViewHolder) {
                result = Result.SyncedTab(view = viewHolder.composeView)
                break
            }
        }

        return result
    }

    /**
     * The result of determining which CFR to show on the Home screen.
     */
    sealed class Result {
        /**
         * Indicates no CFR should be shown on the Home screen.
         */
        object None : Result()

        /**
         * Indicates a CFR should be shown for a Synced Tab and the associated [view] to anchor
         * the CFR.
         */
        data class SyncedTab(val view: View) : Result()
    }
}

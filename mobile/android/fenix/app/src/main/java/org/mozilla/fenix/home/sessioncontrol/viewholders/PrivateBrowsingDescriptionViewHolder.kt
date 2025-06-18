/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders

import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.LifecycleOwner
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ComposeViewHolder
import org.mozilla.fenix.home.privatebrowsing.interactor.PrivateBrowsingInteractor

/**
 * View holder for a private browsing description.
 *
 * @param composeView [ComposeView] which will be populated with Jetpack Compose UI content.
 * @param viewLifecycleOwner [LifecycleOwner] life cycle owner for the view.
 * @property interactor [PrivateBrowsingInteractor] which will have delegated to all user interactions.
 */
class PrivateBrowsingDescriptionViewHolder(
    composeView: ComposeView,
    viewLifecycleOwner: LifecycleOwner,
    val interactor: PrivateBrowsingInteractor,
) : ComposeViewHolder(composeView, viewLifecycleOwner) {

    init {
        val horizontalPadding =
            composeView.resources.getDimensionPixelSize(R.dimen.home_item_horizontal_margin)
        composeView.setPadding(horizontalPadding, 0, horizontalPadding, 0)
    }

    @Composable
    override fun Content() {
        FeltPrivacyModeInfoCard(
            onLearnMoreClick = interactor::onLearnMoreClicked,
        )
    }

    companion object {
        val LAYOUT_ID = View.generateViewId()
    }
}

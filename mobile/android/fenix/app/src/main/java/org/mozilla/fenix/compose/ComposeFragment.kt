/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.fragment.app.Fragment

/**
 * Base class for fragments that use Compose UI and have a 1:1 relationship with a Composable, i.e
 * Fragments that do not have [View]s and [ComposeView]s defined in the XML layout.
 *
 * The ViewCompositionStrategy is set to [ViewCompositionStrategy.DisposeOnLifecycleDestroyed],
 * meaning that the [ComposeView] will be disposed when the Fragment's view is destroyed.
 *
 * Read more about [ViewCompositionStrategy] here:
 * https://medium.com/androiddevelopers/viewcompositionstrategy-demystefied-276427152f34
 * https://developer.android.com/develop/ui/compose/migrate/interoperability-apis/compose-in-views#composition-strategy
 */
abstract class ComposeFragment : Fragment() {

    final override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        setViewCompositionStrategy(
            strategy = ViewCompositionStrategy.DisposeOnLifecycleDestroyed(viewLifecycleOwner),
        )
        setContent {
            UI()
        }
    }

    /**
     * The Composable UI for this fragment that will be set as the content of the [ComposeView].
     */
    @Composable
    abstract fun UI()
}

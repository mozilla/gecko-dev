/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.app.Activity
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import com.google.android.material.snackbar.Snackbar
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.ext.components

/**
 * A binding that shows standard snackbar errors.
 *
 * @param activity [Activity] used for system interactions and accessing resources.
 * @param snackbarParent [ViewGroup] in which to find a suitable parent for displaying the snackbar.
 * @param appStore The [AppStore] containing information about when to show a snackbar styled for errors.
 */
class StandardSnackbarErrorBinding(
    private val activity: Activity,
    private val snackbarParent: ViewGroup,
    appStore: AppStore,
) : AbstractBinding<AppState>(appStore) {

    override suspend fun onState(flow: Flow<AppState>) {
        flow.map { state -> state.standardSnackbarError }
            .distinctUntilChanged()
            .collect {
                it?.let { standardSnackbarError ->
                    snackbarParent.let { view ->
                        val snackBar = FenixSnackbar.make(
                            view = view,
                            duration = Snackbar.LENGTH_INDEFINITE,
                        )
                        snackBar.setText(
                            standardSnackbarError.message,
                        )
                        snackBar.setButtonTextColor(
                            ContextCompat.getColor(
                                activity,
                                R.color.fx_mobile_text_color_primary,
                            ),
                        )
                        snackBar.setBackground(
                            AppCompatResources.getDrawable(
                                activity,
                                R.drawable.standard_snackbar_error_background,
                            ),
                        )
                        snackBar.setSnackBarTextColor(
                            ContextCompat.getColor(
                                activity,
                                R.color.fx_mobile_text_color_critical,
                            ),
                        )
                        snackBar.setAction(
                            text = activity.getString(R.string.standard_snackbar_error_dismiss),
                            action = {
                                view.context.components.appStore.dispatch(
                                    AppAction.UpdateStandardSnackbarErrorAction(
                                        null,
                                    ),
                                )
                            },
                        )
                        snackBar.show()
                    }
                }
            }
    }
}

/**
 * Standard Snackbar Error data class.
 *
 * @property message that will appear on the snackbar.
 */
data class StandardSnackbarError(
    val message: String,
)

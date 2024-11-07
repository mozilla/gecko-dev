/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.snackbar

import androidx.compose.material.Snackbar
import androidx.compose.material.SnackbarData
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import org.mozilla.fenix.compose.core.Action
import org.mozilla.fenix.compose.snackbar.SnackbarState.Type
import androidx.compose.material.SnackbarHost as MaterialSnackbarHost

/**
 * Host for [Snackbar]s to properly show, hide, and dismiss items via [snackbarHostState] in Compose.
 * For displaying Snackbars anchored to a View, consider using [Snackbar.make] instead.
 *
 * @param snackbarHostState State of this component to read and show [Snackbar]s accordingly
 * @param modifier The [Modifier] used to style the SnackbarHost.
 */
@Composable
fun SnackbarHost(
    snackbarHostState: AcornSnackbarHostState,
    modifier: Modifier = Modifier,
) {
    // We need separate hosts for the different use cases/styles until we migrate to material 3
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1925333
    MaterialSnackbarHost(
        hostState = snackbarHostState.defaultSnackbarHostState,
        modifier = modifier,
    ) { snackbarData ->
        Snackbar(
            snackbarState = SnackbarState(
                message = snackbarData.message,
                type = Type.Default,
                action = snackbarData.action,
            ),
        )
    }

    MaterialSnackbarHost(
        hostState = snackbarHostState.warningSnackbarHostState,
        modifier = modifier,
    ) { snackbarData ->
        Snackbar(
            snackbarState = SnackbarState(
                message = snackbarData.message,
                type = Type.Warning,
                action = snackbarData.action,
            ),
        )
    }
}

private val SnackbarData.action: Action?
    get() = actionLabel?.let {
        Action(
            label = it,
            onClick = this::performAction,
        )
    }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.gleandebugtools

import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.navigation.fragment.findNavController
import mozilla.telemetry.glean.Glean
import org.mozilla.fenix.R
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.compose.ComposeFragment
import org.mozilla.fenix.debugsettings.gleandebugtools.ui.GleanDebugToolsScreen
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * [ComposeFragment] for displaying the Glean Debug Tools in the about:glean page.
 */
class GleanDebugToolsFragment : ComposeFragment() {

    private val store by lazyStore {
        GleanDebugToolsStore(
            initialState = GleanDebugToolsState(
                logPingsToConsoleEnabled = Glean.getLogPings(),
                debugViewTag = Glean.getDebugViewTag() ?: "",
            ),
            middlewares = listOf(
                GleanDebugToolsMiddleware(
                    gleanDebugToolsStorage = DefaultGleanDebugToolsStorage(),
                    clipboardHandler = requireComponents.clipboardHandler,
                    openDebugView = { debugViewLink ->
                        val intent = Intent(Intent.ACTION_VIEW)
                        intent.data = Uri.parse(debugViewLink)
                        requireContext().startActivity(intent)
                    },
                    showToast = { pingType ->
                        val toast = Toast.makeText(
                            requireContext(),
                            requireContext().getString(
                                R.string.glean_debug_tools_send_ping_toast_message,
                                pingType,
                            ),
                            Toast.LENGTH_LONG,
                        )
                        toast.show()
                    },
                ),
            ),
        )
    }

    @Composable
    @SuppressLint("UnusedMaterialScaffoldPaddingParameter")
    override fun UI() {
        FirefoxTheme {
            Scaffold(
                topBar = {
                    TopAppBar(
                        title = {
                            Text(
                                text = stringResource(R.string.glean_debug_tools_title),
                                color = FirefoxTheme.colors.textPrimary,
                                style = FirefoxTheme.typography.headline6,
                            )
                        },
                        navigationIcon = {
                            val directions = GleanDebugToolsFragmentDirections.actionGlobalBrowser()
                            IconButton(onClick = { findNavController().navigate(directions) }) {
                                Icon(
                                    painter = painterResource(R.drawable.mozac_ic_back_24),
                                    contentDescription = stringResource(
                                        R.string.bookmark_navigate_back_button_content_description,
                                    ),
                                    tint = FirefoxTheme.colors.iconPrimary,
                                )
                            }
                        },
                        backgroundColor = FirefoxTheme.colors.layer1,
                    )
                },
                backgroundColor = FirefoxTheme.colors.layer1,
            ) {
                GleanDebugToolsScreen(
                    gleanDebugToolsStore = store,
                )
            }
        }
    }
}

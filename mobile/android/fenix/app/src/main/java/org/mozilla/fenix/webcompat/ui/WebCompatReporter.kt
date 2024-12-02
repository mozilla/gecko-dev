/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.ui

import android.annotation.SuppressLint
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Scaffold
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Dropdown
import org.mozilla.fenix.compose.TextField
import org.mozilla.fenix.compose.TextFieldColors
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.button.TextButton
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.webcompat.store.WebCompatReporterAction
import org.mozilla.fenix.webcompat.store.WebCompatReporterState
import org.mozilla.fenix.webcompat.store.WebCompatReporterStore

private const val PROBLEM_DESCRIPTION_MAX_LINES = 6

/**
 * Top-level UI for the Web Compat Reporter feature.
 *
 * @param store [WebCompatReporterStore] used to manage the state of the Web Compat Reporter feature.
 */
@Suppress("LongMethod")
@SuppressLint("UnusedMaterialScaffoldPaddingParameter")
@Composable
fun WebCompatReporter(
    store: WebCompatReporterStore,
) {
    val state by store.observeAsState(store.state) { it }

    BackHandler {
        store.dispatch(WebCompatReporterAction.BackPressed)
    }

    Scaffold(
        topBar = {
            TempAppBar(
                onBackClick = {
                    store.dispatch(WebCompatReporterAction.BackPressed)
                },
            )
        },
        backgroundColor = FirefoxTheme.colors.layer2,
    ) {
        Column(
            modifier = Modifier.verticalScroll(rememberScrollState())
                .padding(horizontal = 16.dp, vertical = 12.dp),
        ) {
            Text(
                text = stringResource(
                    id = R.string.webcompat_reporter_description,
                    stringResource(R.string.app_name),
                ),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.body2,
            )

            Spacer(modifier = Modifier.height(32.dp))

            TextField(
                value = state.enteredUrl,
                onValueChange = {
                    store.dispatch(WebCompatReporterAction.BrokenSiteChanged(newUrl = it))
                },
                placeholder = "",
                errorText = stringResource(id = R.string.webcompat_reporter_url_error_invalid),
                label = stringResource(id = R.string.webcompat_reporter_label_url),
                isError = state.hasUrlTextError,
                singleLine = true,
            )

            Spacer(modifier = Modifier.height(16.dp))

            Dropdown(
                label = stringResource(id = R.string.webcompat_reporter_label_whats_broken),
                placeholder = stringResource(id = R.string.webcompat_reporter_choose_reason),
                dropdownItems = state.toDropdownItems(
                    onDropdownItemClick = {
                        store.dispatch(WebCompatReporterAction.ReasonChanged(newReason = it))
                    },
                ),
            )

            Spacer(modifier = Modifier.height(16.dp))

            TextField(
                value = state.problemDescription,
                onValueChange = {
                    store.dispatch(WebCompatReporterAction.ProblemDescriptionChanged(newProblemDescription = it))
                },
                placeholder = "",
                errorText = "",
                label = stringResource(id = R.string.webcompat_reporter_label_description),
                singleLine = false,
                maxLines = PROBLEM_DESCRIPTION_MAX_LINES,
                colors = TextFieldColors.default(
                    inputColor = FirefoxTheme.colors.textSecondary,
                ),
            )

            Text(
                text = stringResource(id = R.string.webcompat_reporter_send_more_info),
                modifier = Modifier
                    .clickable {
                        store.dispatch(WebCompatReporterAction.SendMoreInfoClicked)
                    }.padding(vertical = 16.dp),
                style = FirefoxTheme.typography.body2,
                color = FirefoxTheme.colors.textAccent,
                textDecoration = TextDecoration.Underline,
            )

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.End,
            ) {
                TextButton(
                    text = stringResource(id = R.string.webcompat_reporter_cancel),
                    onClick = {
                        store.dispatch(WebCompatReporterAction.CancelClicked)
                    },
                    upperCaseText = false,
                )

                Spacer(modifier = Modifier.width(10.dp))

                PrimaryButton(
                    text = stringResource(id = R.string.webcompat_reporter_send),
                    modifier = Modifier.wrapContentSize(),
                    enabled = !state.hasUrlTextError,
                ) {
                    store.dispatch(WebCompatReporterAction.SendReportClicked)
                }
            }
        }
    }
}

@Composable
private fun TempAppBar(
    onBackClick: () -> Unit,
) {
    TopAppBar(
        backgroundColor = FirefoxTheme.colors.layer1,
        title = {
            Text(
                text = stringResource(id = R.string.webcompat_reporter_screen_title),
                color = FirefoxTheme.colors.textPrimary,
                style = FirefoxTheme.typography.headline6,
            )
        },
        navigationIcon = {
            IconButton(onClick = onBackClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.bookmark_navigate_back_button_content_description),
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }
        },
    )
}

private class WebCompatPreviewParameterProvider : PreviewParameterProvider<WebCompatReporterState> {
    override val values: Sequence<WebCompatReporterState>
        get() = sequenceOf(
            // Initial feature opening
            WebCompatReporterState(
                enteredUrl = "www.example.com/url_parameters_that_break_the_page",
            ),
            // Error in URL field
            WebCompatReporterState(
                enteredUrl = "",
            ),
            // Multi-line description
            WebCompatReporterState(
                enteredUrl = "www.example.com/url_parameters_that_break_the_page",
                reason = WebCompatReporterState.BrokenSiteReason.Slow,
                problemDescription = "The site wouldn’t load and after I tried xyz it still wouldn’t " +
                    "load and then again site wouldn’t load and after I tried xyz it still wouldn’t " +
                    "load and then again site wouldn’t load and after I tried xyz it still wouldn’t " +
                    "load and then again site wouldn’t load and after I tried xyz it still wouldn’t " +
                    "load and then again site wouldn’t load and after I tried xyz it still wouldn’t " +
                    "load and then again ",
            ),
        )
}

@PreviewLightDark
@Composable
private fun WebCompatReporterPreview(
    @PreviewParameter(WebCompatPreviewParameterProvider::class) initialState: WebCompatReporterState,
) {
    FirefoxTheme {
        WebCompatReporter(
            store = WebCompatReporterStore(
                initialState = initialState,
            ),
        )
    }
}

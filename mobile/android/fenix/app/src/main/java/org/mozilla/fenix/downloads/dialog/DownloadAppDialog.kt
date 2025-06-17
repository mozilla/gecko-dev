/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.dialog

import android.content.Context
import androidx.appcompat.app.AlertDialog
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.colorspace.ColorSpaces
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.graphics.drawable.toBitmap
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.feature.downloads.ui.DownloaderApp
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Creates and configures an [AlertDialog] for allowing the user to choose a third-party
 * application to handle a download.
 *
 * @param context The Context in which the dialog should be shown.
 * @param downloaderApps A list of [DownloaderApp] objects representing the available
 *                       applications to choose from.
 * @param onAppSelected A lambda function that will be invoked when the user selects an
 *                      application.
 * @param onDismiss A lambda function that will be invoked when the dialog is dismissed
 *                  for any reason.
 * @return The created [AlertDialog] instance, ready to be shown via `dialog.show()`.
 */
internal fun createDownloadAppDialog(
    context: Context,
    downloaderApps: List<DownloaderApp>,
    onAppSelected: (DownloaderApp) -> Unit,
    onDismiss: () -> Unit,
): AlertDialog {
    lateinit var dialog: AlertDialog

    val composeView = ComposeView(context).apply {
        setContent {
            FirefoxTheme {
                DownloaderAppList(
                    apps = downloaderApps,
                    onAppSelected = { selectedApp ->
                        onAppSelected(selectedApp)
                        dialog.dismiss()
                    },
                    modifier = Modifier.padding(top = 16.dp, bottom = 8.dp),
                )
            }
        }
    }

    val builder = AlertDialog.Builder(context)
        .setTitle(context.getString(R.string.download_app_dialog_title))
        .setView(composeView)
        .setOnDismissListener {
            onDismiss.invoke()
        }

    dialog = builder.create()
    return dialog
}

@Composable
internal fun DownloaderAppList(
    apps: List<DownloaderApp>,
    onAppSelected: (DownloaderApp) -> Unit,
    modifier: Modifier = Modifier,
) {
    LazyColumn(modifier = modifier) {
        items(apps) { app ->
            DownloaderAppItem(
                iconBitmap = app.resolver.loadIcon(LocalContext.current.packageManager).toBitmap()
                    .asImageBitmap(),
                appName = app.name,
                onAppSelected = { onAppSelected(app) },
            )
        }
    }
}

@Composable
internal fun DownloaderAppItem(
    iconBitmap: ImageBitmap,
    appName: String,
    onAppSelected: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 6.dp)
            .clickable { onAppSelected() },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Image(
            bitmap = iconBitmap,
            contentDescription = null,
            modifier = Modifier.size(40.dp),
        )
        Spacer(Modifier.width(16.dp))
        Text(
            text = appName,
            style = FirefoxTheme.typography.subtitle1,
            color = FirefoxTheme.colors.textPrimary,
            modifier = Modifier.weight(1f),
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
internal fun DownloaderAppItemPreview() {
    val placeholderBitmap = ImageBitmap(width = 40, height = 40, colorSpace = ColorSpaces.Srgb)

    FirefoxTheme {
        DownloaderAppItem(
            iconBitmap = placeholderBitmap,
            appName = "Firefox Nightly Downloader",
            onAppSelected = { },
            modifier = Modifier.padding(vertical = 4.dp),
        )
    }
}

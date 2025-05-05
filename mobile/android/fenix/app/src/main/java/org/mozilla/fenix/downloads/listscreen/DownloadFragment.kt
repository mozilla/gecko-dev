/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import androidx.appcompat.widget.Toolbar
import androidx.compose.runtime.Composable
import androidx.navigation.fragment.findNavController
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.compose.ComposeFragment
import org.mozilla.fenix.compose.snackbar.Snackbar
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.downloads.dialog.DynamicDownloadDialog
import org.mozilla.fenix.downloads.listscreen.di.DownloadUIMiddlewareProvider
import org.mozilla.fenix.downloads.listscreen.di.DownloadUIMiddlewareProvider.provideUndoDelayProvider
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIStore
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.ext.setToolbarColors
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Fragment for displaying and managing the downloads list.
 */
class DownloadFragment : ComposeFragment(), UserInteractionHandler {

    private val downloadStore by lazyStore { viewModelScope ->
        DownloadUIStore(
            initialState = DownloadUIState.INITIAL,
            middleware = DownloadUIMiddlewareProvider.provideMiddleware(
                coroutineScope = viewModelScope,
                applicationContext = requireContext().applicationContext,
            ),
        )
    }

    @Composable
    override fun UI() {
        FirefoxTheme {
            DownloadsScreen(
                downloadsStore = downloadStore,
                undoDelayProvider = provideUndoDelayProvider(requireContext().settings()),
                onItemClick = { openItem(it) },
                onNavigationIconClick = {
                    if (downloadStore.state.mode is DownloadUIState.Mode.Editing) {
                        downloadStore.dispatch(DownloadUIAction.ExitEditMode)
                    } else {
                        this@DownloadFragment.findNavController().popBackStack()
                    }
                },
            )
        }
    }

    override fun onBackPressed(): Boolean {
        return if (downloadStore.state.mode is DownloadUIState.Mode.Editing) {
            downloadStore.dispatch(DownloadUIAction.ExitEditMode)
            true
        } else {
            false
        }
    }

    private fun openItem(item: FileItem, mode: BrowsingMode? = null) {
        mode?.let { (activity as HomeActivity).browsingModeManager.mode = it }
        context?.let {
            val downloadState = DownloadState(
                id = item.id,
                url = item.url,
                fileName = item.fileName,
                contentType = item.contentType,
                status = item.status,
            )

            val canOpenFile = AbstractFetchDownloadService.openFile(
                applicationContext = it.applicationContext,
                downloadFileName = downloadState.fileName,
                downloadFilePath = downloadState.filePath,
                downloadContentType = downloadState.contentType,
            )

            val rootView = view
            if (!canOpenFile && rootView != null) {
                Snackbar.make(
                    snackBarParentView = rootView,
                    snackbarState = SnackbarState(
                        message = DynamicDownloadDialog.getCannotOpenFileErrorMessage(
                            context = it,
                            download = downloadState,
                        ),
                    ),
                ).show()
            }
        }
    }

    override fun onDetach() {
        super.onDetach()
        context?.let {
            activity?.title = getString(R.string.app_name)
            activity?.findViewById<Toolbar>(R.id.navigationToolbar)?.setToolbarColors(
                it.getColorFromAttr(R.attr.textPrimary),
                it.getColorFromAttr(R.attr.layer1),
            )
        }
    }
}

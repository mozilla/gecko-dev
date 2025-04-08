/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import android.os.Bundle
import android.text.SpannableString
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.appcompat.widget.Toolbar
import androidx.compose.runtime.Composable
import androidx.core.content.ContextCompat
import androidx.core.view.MenuProvider
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.feature.downloads.AbstractFetchDownloadService
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.compose.ComposeFragment
import org.mozilla.fenix.compose.core.Action
import org.mozilla.fenix.compose.snackbar.Snackbar
import org.mozilla.fenix.compose.snackbar.SnackbarState
import org.mozilla.fenix.downloads.dialog.DynamicDownloadDialog
import org.mozilla.fenix.downloads.listscreen.middleware.DefaultUndoDelayProvider
import org.mozilla.fenix.downloads.listscreen.middleware.DownloadDeleteMiddleware
import org.mozilla.fenix.downloads.listscreen.middleware.DownloadTelemetryMiddleware
import org.mozilla.fenix.downloads.listscreen.middleware.DownloadUIMapperMiddleware
import org.mozilla.fenix.downloads.listscreen.middleware.DownloadUIShareMiddleware
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIAction
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIStore
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.setTextColor
import org.mozilla.fenix.ext.setToolbarColors
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Fragment for displaying and managing the downloads list.
 */
@SuppressWarnings("TooManyFunctions", "LargeClass")
class DownloadFragment : ComposeFragment(), UserInteractionHandler, MenuProvider {

    private val undoDelayProvider by lazy { DefaultUndoDelayProvider(requireComponents.settings) }
    private val downloadStore by lazyStore { viewModelScope ->
        DownloadUIStore(
            initialState = DownloadUIState.INITIAL,
            middleware = listOf(
                DownloadUIMapperMiddleware(
                    browserStore = requireComponents.core.store,
                    fileSizeFormatter = requireComponents.core.fileSizeFormatter,
                    scope = viewModelScope,
                ),
                DownloadUIShareMiddleware(applicationContext = requireContext().applicationContext),
                DownloadTelemetryMiddleware(),
                DownloadDeleteMiddleware(
                    undoDelayProvider = undoDelayProvider,
                    removeDownloadUseCase = requireComponents.useCases.downloadUseCases.removeDownload,
                ),
            ),
        )
    }

    @Composable
    override fun UI() {
        FirefoxTheme {
            DownloadsScreen(
                downloadsStore = downloadStore,
                onItemClick = { openItem(it) },
                onItemDeleteClick = { deleteFileItems(setOf(it)) },
            )
        }
    }

    private fun invalidateOptionsMenu() {
        activity?.invalidateOptionsMenu()
    }

    /**
     * Schedules [items] for deletion.
     * Note: When tapping on a download item's "trash" button
     * (itemView.overflow_menu) this [items].size() will be 1.
     */
    private fun deleteFileItems(items: Set<FileItem>) {
        val itemIds = items.map { it.id }.toSet()
        downloadStore.dispatch(DownloadUIAction.AddPendingDeletionSet(itemIds))
        showSnackbar(items)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        requireActivity().addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        observeModeChanges()
    }

    private fun showSnackbar(items: Set<FileItem>) {
        val rootView: View = requireActivity().getRootView() ?: return
        Snackbar.make(
            rootView,
            snackbarState = SnackbarState(
                message = getMultiSelectSnackBarMessage(items),
                duration = SnackbarState.Duration.Custom(undoDelayProvider.undoDelay.toInt()),
                action = Action(
                    label = getString(R.string.download_undo_delete_snackbar_action),
                    onClick = {
                        val itemIds = items.mapTo(mutableSetOf()) { it.id }
                        downloadStore.dispatch(DownloadUIAction.UndoPendingDeletionSet(itemIds))
                    },
                ),
            ),
        ).show()
    }

    private fun observeModeChanges() {
        viewLifecycleOwner.lifecycleScope.launch {
            downloadStore.flow()
                .distinctUntilChangedBy { it.mode }
                .map { it.mode }
                .collect { mode ->
                    invalidateOptionsMenu()
                    when (mode) {
                        is DownloadUIState.Mode.Editing -> {
                            updateToolbarForSelectingMode(
                                title = getString(
                                    R.string.download_multi_select_title,
                                    mode.selectedItems.size,
                                ),
                            )
                        }

                        DownloadUIState.Mode.Normal -> {
                            updateToolbarForNormalMode(title = getString(R.string.library_downloads))
                        }
                    }
                }
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.library_downloads))
    }

    override fun onCreateMenu(menu: Menu, inflater: MenuInflater) {
        val menuRes = when (downloadStore.state.mode) {
            is DownloadUIState.Mode.Normal -> return
            is DownloadUIState.Mode.Editing -> R.menu.download_select_multi
        }
        inflater.inflate(menuRes, menu)

        menu.findItem(R.id.delete_downloads_multi_select)?.title =
            SpannableString(getString(R.string.download_delete_item)).apply {
                setTextColor(requireContext(), R.attr.textCritical)
            }
    }

    override fun onMenuItemSelected(item: MenuItem): Boolean = when (item.itemId) {
        R.id.delete_downloads_multi_select -> {
            deleteFileItems(downloadStore.state.mode.selectedItems)
            downloadStore.dispatch(DownloadUIAction.ExitEditMode)
            true
        }

        R.id.select_all_downloads_multi_select -> {
            downloadStore.dispatch(DownloadUIAction.AddAllItemsForRemoval)
            true
        }
        // other options are not handled by this menu provider
        else -> false
    }

    /**
     * Provides a message to the Undo snackbar.
     */
    private fun getMultiSelectSnackBarMessage(fileItems: Set<FileItem>): String {
        return if (fileItems.size > 1) {
            getString(R.string.download_delete_multiple_items_snackbar_2, fileItems.size)
        } else {
            String.format(
                requireContext().getString(R.string.download_delete_single_item_snackbar_2),
                fileItems.first().fileName,
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

    private fun updateToolbarForNormalMode(title: String?) {
        context?.let {
            updateToolbar(
                title = title,
                foregroundColor = it.getColorFromAttr(R.attr.textPrimary),
                backgroundColor = it.getColorFromAttr(R.attr.layer1),
            )
        }
    }

    private fun updateToolbarForSelectingMode(title: String?) {
        context?.let {
            updateToolbar(
                title = title,
                foregroundColor = ContextCompat.getColor(
                    it,
                    R.color.fx_mobile_text_color_oncolor_primary,
                ),
                backgroundColor = it.getColorFromAttr(R.attr.accent),
            )
        }
    }

    private fun updateToolbar(title: String?, foregroundColor: Int, backgroundColor: Int) {
        activity?.title = title
        val toolbar = activity?.findViewById<Toolbar>(R.id.navigationToolbar)
        toolbar?.setToolbarColors(foregroundColor, backgroundColor)
        toolbar?.setNavigationIcon(R.drawable.ic_back_button)
        toolbar?.navigationIcon?.setTint(foregroundColor)
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

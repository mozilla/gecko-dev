package org.mozilla.fenix.share

import io.mockk.mockk
import mozilla.components.concept.sync.TabData
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.snackbar.SnackbarState

class ShareActionReducerTest {
    @Test
    fun `WHEN ShareToAppFailed action is dispatched THEN snackbar state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(
            AppAction.ShareAction.ShareToAppFailed,
        ).joinBlocking()

        assertEquals(
            SnackbarState.ShareToAppFailed,
            appStore.state.snackbarState,
        )
    }

    @Test
    fun `WHEN SharedTabsSuccessfully action is dispatched THEN snackbar state is updated`() {
        val destination = listOf("a")
        val tabs = listOf(mockk<TabData>(), mockk<TabData>())
        val appStore = AppStore()

        appStore.dispatch(
            AppAction.ShareAction.SharedTabsSuccessfully(destination, tabs),
        ).joinBlocking()

        assertEquals(
            SnackbarState.SharedTabsSuccessfully(destination, tabs),
            appStore.state.snackbarState,
        )
    }

    @Test
    fun `WHEN ShareTabsFailed action is dispatched THEN snackbar state is updated`() {
        val destination = listOf("a")
        val tabs = listOf(mockk<TabData>(), mockk<TabData>())
        val appStore = AppStore()

        appStore.dispatch(
            AppAction.ShareAction.ShareTabsFailed(destination, tabs),
        ).joinBlocking()

        assertEquals(
            SnackbarState.ShareTabsFailed(destination, tabs),
            appStore.state.snackbarState,
        )
    }

    @Test
    fun `WHEN CopyLinkToClipboard action is dispatched THEN snackbar state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(
            AppAction.ShareAction.CopyLinkToClipboard,
        ).joinBlocking()

        assertEquals(
            SnackbarState.CopyLinkToClipboard,
            appStore.state.snackbarState,
        )
    }
}

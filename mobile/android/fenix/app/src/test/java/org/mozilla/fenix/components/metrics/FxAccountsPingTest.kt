package org.mozilla.fenix.components.metrics

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.service.fxa.store.Account
import mozilla.components.service.fxa.store.SyncAction
import mozilla.components.service.fxa.store.SyncStatus
import mozilla.components.service.fxa.store.SyncStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Pings.fxAccounts
import org.mozilla.fenix.components.TelemetryMiddleware

@RunWith(AndroidJUnit4::class)
internal class FxAccountsPingTest {

    lateinit var context: Context
    private lateinit var mockAccount: Account
    private lateinit var syncStore: SyncStore

    @Before
    fun setup() {
        context = ApplicationProvider.getApplicationContext()
        mockAccount =
            Account(
                uid = "123",
                email = "email@email.com",
                avatar = null,
                displayName = "TempName",
                currentDeviceId = null,
                sessionToken = null,
            )
        val telemetryMiddleware = TelemetryMiddleware()
        syncStore = SyncStore(middleware = listOf(telemetryMiddleware))
    }

    @Test
    fun `the state changes and the UID does change and the ping is submitted`() {
        assertEquals(null, syncStore.state.account?.uid)
        var validatorRun = false
        syncStore.dispatch(SyncAction.UpdateSyncStatus(SyncStatus.Idle))
        fxAccounts.testBeforeNextSubmit {
            validatorRun = true
        }
        syncStore.dispatch(
            SyncAction.UpdateAccount(
                account = mockAccount,
            ),
        ).joinBlocking()
        assertEquals("123", syncStore.state.account?.uid)
        if (!validatorRun) fail("The ping was not sent")
    }

    @Test
    fun `the state changes and the UID becomes null and the ping is NOT submitted`() {
        assertEquals(null, syncStore.state.account?.uid)
        var validatorRun = false
        syncStore.dispatch(SyncAction.UpdateSyncStatus(SyncStatus.Idle))
        syncStore.dispatch(
            SyncAction.UpdateAccount(
                account = mockAccount,
            ),
        ).joinBlocking()
        assertEquals(syncStore.state.account?.uid, "123")
        fxAccounts.testBeforeNextSubmit {
            validatorRun = true
        }
        syncStore.dispatch(
            SyncAction.UpdateAccount(
                account = mockAccount.copy(uid = null),
            ),
        ).joinBlocking()
        assertEquals(null, syncStore.state.account?.uid)
        if (validatorRun) fail("The ping was sent")
    }

    @Test
    fun `the state changes and the UID remains the same and the ping is NOT submitted`() {
        assertEquals(null, syncStore.state.account?.uid)
        var validatorRun = false
        syncStore.dispatch(SyncAction.UpdateSyncStatus(SyncStatus.Idle))
        syncStore.dispatch(
            SyncAction.UpdateAccount(
                account = mockAccount,
            ),
        ).joinBlocking()
        assertEquals("123", syncStore.state.account?.uid)
        fxAccounts.testBeforeNextSubmit {
            validatorRun = true
        }
        syncStore.dispatch(
            SyncAction.UpdateAccount(
                account = mockAccount.copy(email = "newEmail@email.com"),
            ),
        ).joinBlocking()
        assertEquals("123", syncStore.state.account?.uid)
        assertEquals("newEmail@email.com", syncStore.state.account?.email)
        if (validatorRun) fail("The ping was sent")
    }
}

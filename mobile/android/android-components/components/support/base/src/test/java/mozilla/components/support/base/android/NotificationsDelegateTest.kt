/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import android.Manifest.permission.POST_NOTIFICATIONS
import android.app.Notification
import androidx.activity.result.ActivityResultCallback
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.NotificationManagerCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.eq
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class NotificationsDelegateTest {

    private lateinit var notificationManagerCompat: NotificationManagerCompat
    private lateinit var crashReporter: CrashReporting
    private lateinit var notificationsDelegate: NotificationsDelegate
    private lateinit var activity: AppCompatActivity
    private lateinit var notification: Notification
    private lateinit var lifecycle: LifecycleRegistry
    private lateinit var activityResultLauncher: ActivityResultLauncher<String>

    @Before
    fun setUp() {
        notificationManagerCompat = mock()
        crashReporter = mock()
        notificationsDelegate = spy(
            NotificationsDelegate(notificationManagerCompat),
        )
        activity = mock()
        lifecycle = mock()

        `when`(lifecycle.currentState).thenReturn(Lifecycle.State.STARTED)
        `when`(activity.lifecycle).thenReturn(lifecycle)

        activityResultLauncher = mock()

        notification = mock()
    }

    @Test
    fun `hasPostNotificationsPermission returns true when notifications are enabled`() {
        `when`(notificationManagerCompat.areNotificationsEnabled()).thenReturn(
            true,
        )
        assertTrue(notificationsDelegate.hasPostNotificationsPermission())
    }

    @Test
    fun `hasPostNotificationsPermission returns false when notifications are disabled`() {
        `when`(notificationManagerCompat.areNotificationsEnabled()).thenReturn(
            false,
        )
        assertFalse(notificationsDelegate.hasPostNotificationsPermission())
    }

    @Test
    fun `bindToActivity adds activity and result launcher to notificationPermissionHandler`() {
        `when`(
            activity.registerForActivityResult(
                any<ActivityResultContracts.RequestPermission>(),
                any<ActivityResultCallback<Boolean>>(),
            ),
        ).thenReturn(activityResultLauncher)

        notificationsDelegate.bindToActivity(activity)

        verify(activity).registerForActivityResult(
            any<ActivityResultContracts.RequestPermission>(),
            any<ActivityResultCallback<Boolean>>(),
        )

        assertEquals(1, notificationsDelegate.notificationPermissionHandler.size)
        assertEquals(activity, notificationsDelegate.notificationPermissionHandler.entries.first().key)
        assertEquals(activityResultLauncher, notificationsDelegate.notificationPermissionHandler.entries.first().value)
        assertEquals(
            Lifecycle.State.STARTED,
            notificationsDelegate.notificationPermissionHandler.entries.first().key.lifecycle.currentState,
        )
    }

    @Test
    fun `unBindActivity removes activity from notificationPermissionHandler`() {
        `when`(
            activity.registerForActivityResult(
                any<ActivityResultContracts.RequestPermission>(),
                any<ActivityResultCallback<Boolean>>(),
            ),
        ).thenReturn(activityResultLauncher)

        notificationsDelegate.bindToActivity(activity)
        assertEquals(1, notificationsDelegate.notificationPermissionHandler.size)

        notificationsDelegate.unBindActivity(activity)
        assertEquals(0, notificationsDelegate.notificationPermissionHandler.size)
    }

    @Test
    fun `requestNotificationPermission with bound activity does not throw exception and calls launch`() {
        `when`(
            activity.registerForActivityResult(
                any<ActivityResultContracts.RequestPermission>(),
                any<ActivityResultCallback<Boolean>>(),
            ),
        ).thenReturn(activityResultLauncher)

        notificationsDelegate.bindToActivity(activity)

        notificationsDelegate.requestNotificationPermission()

        verify(activityResultLauncher).launch(POST_NOTIFICATIONS)
    }

    @Test
    fun `requestNotificationPermission without bound activity does not throw exception and does not call launch`() {
        `when`(
            activity.registerForActivityResult(
                any<ActivityResultContracts.RequestPermission>(),
                any<ActivityResultCallback<Boolean>>(),
            ),
        ).thenReturn(activityResultLauncher)

        notificationsDelegate.bindToActivity(activity)
        notificationsDelegate.unBindActivity(activity)

        notificationsDelegate.requestNotificationPermission()

        verify(activityResultLauncher, never()).launch(POST_NOTIFICATIONS)
    }

    @Test
    fun `notify posts notification when permission is granted`() {
        `when`(notificationManagerCompat.areNotificationsEnabled()).thenReturn(
            true,
        )

        notificationsDelegate.notify(
            "testTag",
            notificationId = 1,
            notification = notification,
        )

        verify(notificationManagerCompat).notify("testTag", 1, notification)
    }

    @Test
    fun `notify requests permission when permission is not granted`() {
        `when`(notificationManagerCompat.areNotificationsEnabled()).thenReturn(
            false,
        )
        doNothing().`when`(notificationsDelegate).requestNotificationPermission(
            any(),
            any(),
            eq(false),
        )

        notificationsDelegate.bindToActivity(activity)
        notificationsDelegate.notify(
            notificationId = 1,
            notification = notification,
        )

        verify(notificationsDelegate).requestNotificationPermission(
            any<OnPermissionGranted>(),
            any<OnPermissionRejected>(),
            eq(false),
        )
    }

    @Test
    fun `requestNotificationPermission requests notification permission when permission is not previously granted`() {
        `when`(notificationManagerCompat.areNotificationsEnabled()).thenReturn(
            false,
        )

        `when`(
            activity.registerForActivityResult(
                any<ActivityResultContracts.RequestPermission>(),
                any<ActivityResultCallback<Boolean>>(),
            ),
        ).thenReturn(activityResultLauncher)

        notificationsDelegate.bindToActivity(activity)

        assertEquals(0, notificationsDelegate.permissionRequestsCount)

        notificationsDelegate.notify(
            notificationId = 1,
            notification = notification,
        )

        verify(activityResultLauncher).launch(POST_NOTIFICATIONS)
    }

    @Test
    fun `requestNotificationPermission calls onPermissionGranted when permission request is granted`() {
        `when`(notificationManagerCompat.areNotificationsEnabled()).thenReturn(false)
        `when`(
            activity.registerForActivityResult(
                any<ActivityResultContracts.RequestPermission>(),
                any<ActivityResultCallback<Boolean>>(),
            ),
        ).thenReturn(activityResultLauncher)

        notificationsDelegate.bindToActivity(activity)

        val onPermissionGranted = mock<OnPermissionGranted>()
        val onPermissionRejected = mock<OnPermissionRejected>()

        notificationsDelegate.requestNotificationPermission(
            onPermissionGranted = onPermissionGranted,
            onPermissionRejected = onPermissionRejected,
        )

        val callbackCaptor = argumentCaptor<ActivityResultCallback<Boolean>>()
        verify(activity).registerForActivityResult(
            any<ActivityResultContracts.RequestPermission>(),
            callbackCaptor.capture(),
        )

        // Simulate permission granted
        callbackCaptor.value.onActivityResult(true)

        verify(onPermissionGranted).invoke()
        verify(onPermissionRejected, never()).invoke()
    }

    @Test
    fun `requestNotificationPermission calls onPermissionRejected when permission request is not granted`() {
        `when`(notificationManagerCompat.areNotificationsEnabled()).thenReturn(false)
        `when`(
            activity.registerForActivityResult(
                any<ActivityResultContracts.RequestPermission>(),
                any<ActivityResultCallback<Boolean>>(),
            ),
        ).thenReturn(activityResultLauncher)

        notificationsDelegate.bindToActivity(activity)

        val onPermissionGranted = mock<OnPermissionGranted>()
        val onPermissionRejected = mock<OnPermissionRejected>()

        notificationsDelegate.requestNotificationPermission(
            onPermissionGranted = onPermissionGranted,
            onPermissionRejected = onPermissionRejected,
        )

        val callbackCaptor = argumentCaptor<ActivityResultCallback<Boolean>>()
        verify(activity).registerForActivityResult(
            any<ActivityResultContracts.RequestPermission>(),
            callbackCaptor.capture(),
        )

        // Simulate permission rejected
        callbackCaptor.value.onActivityResult(false)

        verify(onPermissionRejected).invoke()
        verify(onPermissionGranted, never()).invoke()
    }
}

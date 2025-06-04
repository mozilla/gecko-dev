/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import io.mockk.Called
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import org.junit.Assert.assertEquals
import org.junit.Test

class BreadcrumbRecorderTest {

    @Test
    fun `sets listener on create and destroy`() = runTest {
        val navController: NavController = mockk(relaxUnitFun = true)
        val lifecycleOwner: LifecycleOwner = mockk()
        val breadCrumbRecorder = BreadcrumbsRecorder(mockk(), navController) { "test" }

        verify { navController wasNot Called }

        // Simulate ON_CREATE
        breadCrumbRecorder.onCreate(lifecycleOwner)
        verify { navController.addOnDestinationChangedListener(breadCrumbRecorder) }

        // Simulate ON_DESTROY
        breadCrumbRecorder.onDestroy(lifecycleOwner)
        verify { navController.removeOnDestinationChangedListener(breadCrumbRecorder) }
    }

    @Test
    fun `ensure crash reporter recordCrashBreadcrumb is called`() {
        val service = object : CrashReporterService {
            override val id: String = "test"
            override val name: String = "Test"
            override fun createCrashReportUrl(identifier: String): String? = null
            override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? = ""
            override fun report(crash: Crash.NativeCodeCrash): String? = ""
            override fun report(crash: Crash.UncaughtExceptionCrash): String? = ""
        }

        val reporter = spyk(
            CrashReporter(
                context = mockk(),
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ),
        )

        val navController: NavController = mockk()
        val navDestination: NavDestination = mockk()

        val breadCrumbRecorder = BreadcrumbsRecorder(reporter, navController) { "test" }
        breadCrumbRecorder.onDestinationChanged(navController, navDestination, null)

        verify {
            reporter.recordCrashBreadcrumb(
                withArg {
                    assertEquals("test", it.message)
                    assertEquals("DestinationChanged", it.category)
                    assertEquals(Breadcrumb.Level.INFO, it.level)
                },
            )
        }
    }
}

/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import org.junit.Assert.assertArrayEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.gecko.EventDispatcher
import org.mozilla.gecko.util.GeckoBundle
import org.mozilla.geckoview.CrashPullController
import org.mozilla.geckoview.test.rule.GeckoSessionTestRule.AssertCalled

@RunWith(AndroidJUnit4::class)
@MediumTest
class CrashPullTest : BaseSessionTest() {

    @Before
    fun setup() {
        mainSession.loadTestPath(ADDRESS_FORM_HTML_PATH)
        mainSession.waitForPageStop()
    }

    @Test
    fun testWithEmptyArray() {
        sessionRule.addExternalDelegateDuringNextWait(
            CrashPullController.Delegate::class,
            { delegate -> sessionRule.runtime.setCrashPullDelegate(delegate) },
            { sessionRule.runtime.setCrashPullDelegate(null) },
            object : CrashPullController.Delegate {
                @AssertCalled(count = 1)
                override fun onCrashPull(crashIDs: Array<String>) {
                    assertArrayEquals(arrayOf<String>(), crashIDs)
                    super.onCrashPull(crashIDs)
                }
            },
        )

        val bundle = GeckoBundle()
        bundle.putStringArray("crashIDs", arrayOf<String>())
        EventDispatcher.getInstance().dispatch("GeckoView:RemoteSettingsCrashPull", bundle)
        mainSession.waitUntilCalled(CrashPullController.Delegate::class, "onCrashPull")
    }

    @Test
    fun testWithArrayOfOne() {
        sessionRule.addExternalDelegateDuringNextWait(
            CrashPullController.Delegate::class,
            { delegate -> sessionRule.runtime.setCrashPullDelegate(delegate) },
            { sessionRule.runtime.setCrashPullDelegate(null) },
            object : CrashPullController.Delegate {
                @AssertCalled(count = 1)
                override fun onCrashPull(crashIDs: Array<String>) {
                    assertArrayEquals(arrayOf("$CRASH_PATH/989df240-a40c-405a-9a22-f2fc4a31db6c.dmp"), crashIDs)
                    super.onCrashPull(crashIDs)
                }
            },
        )

        val bundle = GeckoBundle()
        bundle.putStringArray("crashIDs", arrayOf("$CRASH_PATH/989df240-a40c-405a-9a22-f2fc4a31db6c.dmp"))
        EventDispatcher.getInstance().dispatch("GeckoView:RemoteSettingsCrashPull", bundle)
        mainSession.waitUntilCalled(CrashPullController.Delegate::class, "onCrashPull")
    }

    @Test
    fun testWithArrayOfTwo() {
        sessionRule.addExternalDelegateDuringNextWait(
            CrashPullController.Delegate::class,
            { delegate -> sessionRule.runtime.setCrashPullDelegate(delegate) },
            { sessionRule.runtime.setCrashPullDelegate(null) },
            object : CrashPullController.Delegate {
                @AssertCalled(count = 1)
                override fun onCrashPull(crashIDs: Array<String>) {
                    assertArrayEquals(
                        arrayOf("$CRASH_PATH/989df240-a40c-405a-9a22-f2fc4a31db6c.dmp", "$CRASH_PATH/a3adbf3d-9a9a-48a1-a9ed-c603ef9c5c87.dmp"),
                        crashIDs,
                    )
                    super.onCrashPull(crashIDs)
                }
            },
        )

        val bundle = GeckoBundle()
        bundle.putStringArray("crashIDs", arrayOf("$CRASH_PATH/989df240-a40c-405a-9a22-f2fc4a31db6c.dmp", "$CRASH_PATH/a3adbf3d-9a9a-48a1-a9ed-c603ef9c5c87.dmp"))
        EventDispatcher.getInstance().dispatch("GeckoView:RemoteSettingsCrashPull", bundle)
        mainSession.waitUntilCalled(CrashPullController.Delegate::class, "onCrashPull")
    }

    companion object {
        const val CRASH_PATH = "/data/data/org.mozilla.geckoview.test/files/mozilla/Crash Reports/pending"
    }
}

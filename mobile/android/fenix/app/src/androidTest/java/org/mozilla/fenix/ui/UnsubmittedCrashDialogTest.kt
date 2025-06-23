/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ui

import android.app.Activity
import android.content.Context
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.hasTextExactly
import androidx.compose.ui.test.junit4.AndroidComposeTestRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import mozilla.components.lib.crash.store.CrashAction
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.junit.After
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.crashes.UnsubmittedCrashDialog
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MatcherHelper
import org.mozilla.fenix.helpers.MatcherHelper.itemContainingText
import org.mozilla.fenix.helpers.TestHelper
import org.mozilla.fenix.helpers.TestSetup

class UnsubmittedCrashDialogTest : TestSetup() {
    private lateinit var fakeContext: Context

    @Before
    fun setup() {
        mockkStatic("mozilla.components.support.ktx.android.content.ContextKt")
        fakeContext = mockk<Context>()
        every { fakeContext.getTheme() } returns mockk()
        every { fakeContext.packageName } returns "org.mozilla.fenix.debug"
        every { any<Activity>().getColorFromAttr(any()) } returns 0
        every { fakeContext.startActivity(any()) } returns mockk()
    }

    @After
    fun teardown() {
        unmockkStatic("org.mozilla.fenix.ext.ContextKt")
    }

    @get:Rule(order = 0)
    val composeTestRule =
        AndroidComposeTestRule(
            HomeActivityTestRule.withDefaultSettingsOverrides(useNewCrashReporterDialog = true),
        ) { it.activity }

    private fun addCrashToStore(action: CrashAction) {
        composeTestRule.activityRule.activity.applicationContext.components.appStore.dispatch(AppAction.CrashActionWrapper(action))
    }

    private fun verifyDialogText(text: String) =
        MatcherHelper.assertUIObjectExists(itemContainingText(text))
    private fun verifyDialogTextGone(text: String) =
        MatcherHelper.assertUIObjectIsGone(itemContainingText(text))

    private fun getUnsubmittedCrashNormal(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crash_dialog_title, getStringResource(R.string.app_name))
    private fun submitUnsubmittedCrashNormal(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crash_dialog_positive_button)
    private fun cancelUnsubmittedCrashNormal(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crash_dialog_negative_button)

    private fun getUnsubmittedCrashPullOne(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crash_requested_by_devs_dialog_title, getStringResource(R.string.app_name))
    private fun getUnsubmittedCrashPullTwo(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crashes_requested_by_devs_dialog_title, 2, getStringResource(R.string.app_name))
    private fun getUnsubmittedCrashPullThree(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crashes_requested_by_devs_dialog_title, 3, getStringResource(R.string.app_name))
    private fun cancelForEverUnsubmittedCrashPull(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crash_requested_by_devs_dialog_never_button)
    private fun learnMoreUnsubmittedCrashPull(): String =
        TestHelper.appContext.resources.getString(R.string.unsubmitted_crash_requested_by_devs_learn_more)

    @OptIn(ExperimentalTestApi::class)
    private fun clickButton(text: String) {
        composeTestRule.waitUntilAtLeastOneExists(hasTextExactly(text))
        val node = composeTestRule.onNodeWithText(text, false, true)
        node.assertTextEquals(text)
        node.performClick()
    }

    @Test
    fun displayClassicDialogOn_CrashActionShowPrompt() {
        addCrashToStore(CrashAction.ShowPrompt)
        verifyDialogText(getUnsubmittedCrashNormal())
        verifyDialogText(cancelUnsubmittedCrashNormal())
        verifyDialogText(submitUnsubmittedCrashNormal())
    }

    @Test
    fun unsubmittedCrashDialog_ClickOnCancelDispatches_CrashActionCancelTapped_andDismissesDialog() {
        addCrashToStore(CrashAction.ShowPrompt)
        verifyDialogText(getUnsubmittedCrashNormal())
        clickButton(cancelUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashNormal())
    }

    @Test
    fun unsubmittedCrashDialog_ClickOnCancelDispatches_CrashActionCancelTapped() {
        var dispatchedAction: CrashAction? = null
        UnsubmittedCrashDialog(
            dispatcher = { action -> dispatchedAction = action },
            crashIDs = null,
            TestHelper.appContext,
        ).show(composeTestRule.activityRule.activity.supportFragmentManager, UnsubmittedCrashDialog.TAG)
        verifyDialogText(getUnsubmittedCrashNormal())
        clickButton(cancelUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashNormal())
        assertTrue(dispatchedAction is CrashAction.CancelTapped)
    }

    @Test
    fun unsubmittedCrashDialog_ClickOnSubmitDispatches_andDismissesDialog() {
        addCrashToStore(CrashAction.ShowPrompt)
        verifyDialogText(getUnsubmittedCrashNormal())
        verifyDialogText(submitUnsubmittedCrashNormal())
        clickButton(submitUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashNormal())
    }

    @Test
    fun unsubmittedCrashDialog_ClickOnSubmitDispatches_CrashActionReportTapped() {
        var dispatchedAction: CrashAction? = null
        UnsubmittedCrashDialog(
            dispatcher = { action -> dispatchedAction = action },
            crashIDs = null,
            TestHelper.appContext,
        ).show(composeTestRule.activityRule.activity.supportFragmentManager, UnsubmittedCrashDialog.TAG)
        verifyDialogText(getUnsubmittedCrashNormal())
        clickButton(submitUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashNormal())
        assertTrue(dispatchedAction is CrashAction.ReportTapped)
        val report = (dispatchedAction as? CrashAction.ReportTapped)
        assertFalse(report?.automaticallySendChecked == true)
        assertNull(report?.crashIDs)
    }

    @Test
    fun displayCrashPullDialogOnForOneCrash_CrashActionPullCrashes_withOneCrashIDs() {
        addCrashToStore(CrashAction.PullCrashes(arrayOf("1")))
        verifyDialogText(getUnsubmittedCrashPullOne())
    }

    @Test
    fun displayCrashPullDialogOnForSeveralCrashes_CrashActionPullCrashes_withSeveralCrashIDs() {
        addCrashToStore(CrashAction.PullCrashes(arrayOf("1", "2")))
        verifyDialogText(getUnsubmittedCrashPullTwo())

        addCrashToStore(CrashAction.PullCrashes(arrayOf("1", "2", "3")))
        verifyDialogText(getUnsubmittedCrashPullThree())
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_ClickOnCancelDispatches_andDismissesDialog() {
        addCrashToStore(CrashAction.PullCrashes(arrayOf("1")))
        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(cancelUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashPullOne())
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_ClickOnCancelDispatches_CrashActionCancelTapped() {
        var dispatchedAction: CrashAction? = null
        UnsubmittedCrashDialog(
            dispatcher = { action -> dispatchedAction = action },
            crashIDs = arrayOf("1"),
            TestHelper.appContext,
        ).show(composeTestRule.activityRule.activity.supportFragmentManager, UnsubmittedCrashDialog.TAG)
        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(cancelUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashPullOne())
        assertTrue(dispatchedAction is CrashAction.CancelTapped)
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_ClickOnCancelForEverDispatches_andDismissesDialog() {
        addCrashToStore(CrashAction.PullCrashes(arrayOf("1")))
        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(cancelForEverUnsubmittedCrashPull().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashPullOne())
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_ClickOnCancelForEverDispatches_CrashActionCancelForEverTapped() {
        var dispatchedAction: CrashAction? = null
        UnsubmittedCrashDialog(
            dispatcher = { action -> dispatchedAction = action },
            crashIDs = arrayOf("1"),
            TestHelper.appContext,
        ).show(composeTestRule.activityRule.activity.supportFragmentManager, UnsubmittedCrashDialog.TAG)
        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(cancelForEverUnsubmittedCrashPull().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashPullOne())
        assertTrue(dispatchedAction is CrashAction.CancelForEverTapped)
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_ClickOnSubmitDispatches_CrashActionReportTapped() {
        var dispatchedAction: CrashAction? = null
        UnsubmittedCrashDialog(
            dispatcher = { action -> dispatchedAction = action },
            crashIDs = arrayOf("1"),
            TestHelper.appContext,
        ).show(composeTestRule.activityRule.activity.supportFragmentManager, UnsubmittedCrashDialog.TAG)
        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(submitUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashPullOne())
        assertTrue(dispatchedAction is CrashAction.ReportTapped)
        val report = (dispatchedAction as? CrashAction.ReportTapped)
        assertFalse(report?.automaticallySendChecked == true)
        assertArrayEquals(report?.crashIDs, arrayOf("1"))
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_ClickOnSubmit_UpdateDontShowBefore() {
        var dontShowBeforeValue = TestHelper.appContext.settings().crashPullDontShowBefore

        addCrashToStore(CrashAction.PullCrashes(arrayOf("1")))
        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(submitUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashPullOne())

        var newDate = Math.round(System.currentTimeMillis() / 1000.0)
        var dontShowBeforeValueSubmit = TestHelper.appContext.settings().crashPullDontShowBefore
        var expectedDontShowBeforeValueSubmitUp = newDate + 6 * 86400
        var expectedDontShowBeforeValueSubmitDown = newDate + 8 * 86400
        assertTrue("$dontShowBeforeValueSubmit >= $expectedDontShowBeforeValueSubmitUp && $dontShowBeforeValueSubmit <= $expectedDontShowBeforeValueSubmitDown", dontShowBeforeValueSubmit >= expectedDontShowBeforeValueSubmitUp && dontShowBeforeValueSubmit <= expectedDontShowBeforeValueSubmitDown)
    }

    @OptIn(ExperimentalTestApi::class)
    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_DontShowBefore_NextWeek_Blocked() {
        var dontShowBeforeValue = TestHelper.appContext.settings().crashPullDontShowBefore
        var oneWeekAfter = Math.round(System.currentTimeMillis() / 1000.0) + 7 * 86400
        TestHelper.appContext.settings().crashPullDontShowBefore = oneWeekAfter

        addCrashToStore(CrashAction.PullCrashes(arrayOf("1")))
        composeTestRule.waitUntilDoesNotExist(hasTextExactly(getUnsubmittedCrashPullOne()))

        TestHelper.appContext.settings().crashPullDontShowBefore = dontShowBeforeValue
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_DontShowBefore_PreviousWeek_NotBlocked() {
        var dontShowBeforeValue = TestHelper.appContext.settings().crashPullDontShowBefore
        var oneWeekBefore = Math.round(System.currentTimeMillis() / 1000.0) - 7 * 86400
        TestHelper.appContext.settings().crashPullDontShowBefore = oneWeekBefore

        addCrashToStore(CrashAction.PullCrashes(arrayOf("1")))
        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(submitUnsubmittedCrashNormal().uppercase())
        verifyDialogTextGone(getUnsubmittedCrashPullOne())

        TestHelper.appContext.settings().crashPullDontShowBefore = dontShowBeforeValue
    }

    @Test
    fun unsubmittedCrashDialog_PullingOneCrash_ClickOnLearnMoreTriggersIntent_andDontDismissesDialog() {
        val scope: CoroutineScope = CoroutineScope(Dispatchers.Main)

        runBlocking {
            scope.launch {
                UnsubmittedCrashDialog(
                    dispatcher = { },
                    crashIDs = arrayOf("1"),
                    fakeContext,
                ).show(composeTestRule.activityRule.activity.supportFragmentManager, UnsubmittedCrashDialog.TAG)
            }.join()
        }

        verifyDialogText(getUnsubmittedCrashPullOne())
        clickButton(learnMoreUnsubmittedCrashPull().uppercase())

        runBlocking {
            scope.launch {
                verify(exactly = 1) {
                    fakeContext.startActivity(
                        withArg { intent ->
                            val uri = intent.getData().toString()
                            assertEquals("https://support.mozilla.org/en-US/kb/unsent-crash-reports-in-firefox-android", uri)
                        },
                    )
                }
            }.join()
        }

        verifyDialogText(getUnsubmittedCrashPullOne())
    }
}

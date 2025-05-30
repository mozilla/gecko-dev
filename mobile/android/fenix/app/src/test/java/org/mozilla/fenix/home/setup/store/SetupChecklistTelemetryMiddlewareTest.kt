/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertNotNull
import junit.framework.TestCase.assertNull
import junit.framework.TestCase.assertTrue
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SetupChecklistTelemetryMiddlewareTest {
    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var telemetry: SetupChecklistTelemetryRecorder
    private lateinit var middleware: SetupChecklistTelemetryMiddleware

    @Before
    fun setup() {
        telemetry = DefaultSetupChecklistTelemetryRecorder()
        middleware = SetupChecklistTelemetryMiddleware(telemetry)
    }

    @Test
    fun `WHEN ChecklistItem clicked action is invoked THEN SetupChecklistTelemetryRecorder is invoked`() {
        var setupChecklistTelemetryRecorderInvoked = false
        val telemetry = object : SetupChecklistTelemetryRecorder {
            override fun taskClicked(task: ChecklistItem.Task) {
                setupChecklistTelemetryRecorderInvoked = true
            }
        }
        val middleware = SetupChecklistTelemetryMiddleware(telemetry)
        checklistItemClickedAction(middleware, ChecklistItem.Task.Type.EXPLORE_EXTENSION)

        assertTrue(setupChecklistTelemetryRecorderInvoked)
    }

    @Test
    fun `WHEN checklist item SET_AS_DEFAULT is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(middleware, ChecklistItem.Task.Type.SET_AS_DEFAULT)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("default-browser", result)
    }

    @Test
    fun `WHEN checklist item SIGN_IN is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(middleware, ChecklistItem.Task.Type.SIGN_IN)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("sign-in", result)
    }

    @Test
    fun `WHEN checklist item SELECT_THEME is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(middleware, ChecklistItem.Task.Type.SELECT_THEME)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("theme-selection", result)
    }

    @Test
    fun `WHEN checklist item CHANGE_TOOLBAR_PLACEMENT is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(middleware, ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("toolbar-selection", result)
    }

    @Test
    fun `WHEN checklist item INSTALL_SEARCH_WIDGET is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(middleware, ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("search-widget", result)
    }

    @Test
    fun `WHEN checklist item EXPLORE_EXTENSION is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(middleware, ChecklistItem.Task.Type.EXPLORE_EXTENSION)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("extensions", result)
    }

    /**
     * Invokes the [ChecklistItem] clicked action of [type] for the given
     * [SetupChecklistPreferencesMiddleware].
     */
    private fun checklistItemClickedAction(
        middleware: SetupChecklistTelemetryMiddleware,
        type: ChecklistItem.Task.Type,
    ) {
        val task = ChecklistItem.Task(
            type = type,
            title = R.string.setup_checklist_task_default_browser,
            icon = R.drawable.ic_addons_extensions,
            isCompleted = false,
        )

        val context = mock<MiddlewareContext<AppState, AppAction>>()
        middleware.invoke(
            context = context,
            next = {},
            action = AppAction.SetupChecklistAction.ChecklistItemClicked(task),
        )
    }
}

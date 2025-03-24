package org.mozilla.fenix.home.setup.store

import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertNotNull
import junit.framework.TestCase.assertNull
import junit.framework.TestCase.assertTrue
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.checklist.ChecklistItem
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SetupChecklistTelemetryMiddlewareTest {
    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    private lateinit var store: SetupChecklistStore
    private lateinit var telemetry: SetupChecklistTelemetryRecorder
    private lateinit var middleware: SetupChecklistTelemetryMiddleware

    @Before
    fun setup() {
        telemetry = DefaultSetupChecklistTelemetryRecorder()
        middleware = SetupChecklistTelemetryMiddleware(telemetry)
        store = SetupChecklistStore(
            middleware = listOf(middleware),
            initialState = SetupChecklistState(),
        )
    }

    @Test
    fun `WHEN ChecklistItem clicked action is dispatched to store THEN SetupChecklistTelemetryRecorder is invoked`() {
        var setupChecklistTelemetryRecorderInvoked = false
        val telemetry = object : SetupChecklistTelemetryRecorder {
            override fun taskClicked(task: ChecklistItem.Task) {
                setupChecklistTelemetryRecorderInvoked = true
            }
        }
        val middleware = SetupChecklistTelemetryMiddleware(telemetry)
        val store = SetupChecklistStore(
            middleware = listOf(middleware),
            initialState = SetupChecklistState(),
        )

        checklistItemClickedAction(store, ChecklistItem.Task.Type.EXPLORE_EXTENSION)

        assertTrue(setupChecklistTelemetryRecorderInvoked)
    }

    @Test
    fun `WHEN checklist item SET_AS_DEFAULT is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(store, ChecklistItem.Task.Type.SET_AS_DEFAULT)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("default-browser", result)
    }

    @Test
    fun `WHEN checklist item SIGN_IN is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(store, ChecklistItem.Task.Type.SIGN_IN)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("sign-in", result)
    }

    @Test
    fun `WHEN checklist item SELECT_THEME is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(store, ChecklistItem.Task.Type.SELECT_THEME)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("theme-selection", result)
    }

    @Test
    fun `WHEN checklist item CHANGE_TOOLBAR_PLACEMENT is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(store, ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("toolbar-selection", result)
    }

    @Test
    fun `WHEN checklist item INSTALL_SEARCH_WIDGET is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(store, ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("search-widget", result)
    }

    @Test
    fun `WHEN checklist item EXPLORE_EXTENSION is clicked THEN telemetry is sent`() {
        assertNull(Onboarding.setupChecklistTaskClicked.testGetValue())

        checklistItemClickedAction(store, ChecklistItem.Task.Type.EXPLORE_EXTENSION)

        val event = Onboarding.setupChecklistTaskClicked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("task_id").toString()
        assertEquals("extensions", result)
    }

    /**
     * Dispatches the ChecklistItem clicked action of [type] to the SetupChecklistStore.
     *
     * @param store Action is dispatched to this SetupChecklist store.
     * @param type ChecklistItem task type used for the ChecklistItem clicked action.
     */
    private fun checklistItemClickedAction(store: SetupChecklistStore, type: ChecklistItem.Task.Type) {
        val task = ChecklistItem.Task(
            type = type,
            title = "A cool task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = false,
        )

        store.dispatch(SetupChecklistAction.ChecklistItemClicked(task))
        store.waitUntilIdle()
    }
}

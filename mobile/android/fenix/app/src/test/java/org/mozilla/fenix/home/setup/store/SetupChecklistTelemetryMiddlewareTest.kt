package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.MiddlewareContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.junit.MockitoJUnitRunner
import org.mozilla.fenix.R
import org.mozilla.fenix.checklist.ChecklistItem

// todo complete as part of https://bugzilla.mozilla.org/show_bug.cgi?id=1951909

@RunWith(MockitoJUnitRunner::class)
class SetupChecklistTelemetryMiddlewareTest {
    @Mock
    private lateinit var telemetry: SetupChecklistTelemetryRecorder

    @Mock
    private lateinit var context: MiddlewareContext<SetupChecklistState, SetupChecklistAction>

    private lateinit var middleware: SetupChecklistTelemetryMiddleware

    @Before
    fun setup() {
        middleware = SetupChecklistTelemetryMiddleware(telemetry)
    }

    @Test
    fun `GIVEN init action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.Init)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN closed action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.Closed)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN checklist item clicked action WHEN middleware is invoked THEN no telemetry is sent`() {
        val task = ChecklistItem.Task(
            type = ChecklistItem.Task.Type.EXPLORE_EXTENSION,
            title = "A cool task",
            icon = R.drawable.ic_addons_extensions,
            isCompleted = false,
        )
        middleware.invoke(context, {}, SetupChecklistAction.ChecklistItemClicked(task))
        verifyNoInteractions(telemetry)
    }
}

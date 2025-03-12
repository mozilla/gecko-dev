package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.MiddlewareContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.junit.MockitoJUnitRunner

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
    fun `GIVEN default browser clicked action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.DefaultBrowserClicked)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN sync click action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.SyncClicked)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN theme selection click action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.ThemeSelectionClicked)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN toolbar selection click action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.ToolbarSelectionClicked)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN extensions clicked action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.ExtensionsClicked)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN add search widget click action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.AddSearchWidgetClicked)
        verifyNoInteractions(telemetry)
    }

    @Test
    fun `GIVEN view state action WHEN middleware is invoked THEN no telemetry is sent`() {
        middleware.invoke(context, {}, SetupChecklistAction.ViewState(SetupChecklistViewState.FULL))
        verifyNoInteractions(telemetry)
    }
}

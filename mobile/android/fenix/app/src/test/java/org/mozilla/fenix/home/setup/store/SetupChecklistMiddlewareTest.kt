/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.MiddlewareContext
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.junit.MockitoJUnitRunner
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem
import org.mozilla.fenix.components.appstate.setup.checklist.SetupChecklistState

@RunWith(MockitoJUnitRunner::class)
class SetupChecklistMiddlewareTest {

    @Mock
    private lateinit var context: MiddlewareContext<SetupChecklistState, AppAction.SetupChecklistAction>

    @Test
    fun `GIVEN click action contains a sign in task WHEN middleware is invoked THEN navigate to sign in callback is invoked`() {
        var isInvoked = false
        val middleware = buildMiddleware(navigateToSignIn = {
            isInvoked = true
        })

        val task = buildTask(type = ChecklistItem.Task.Type.SIGN_IN)
        middleware.invoke(context, {}, AppAction.SetupChecklistAction.ChecklistItemClicked(task))

        assertTrue(isInvoked)
    }

    @Test
    fun `GIVEN click action contains a set as default task WHEN middleware is invoked THEN trigger default prompt callback is invoked`() {
        var isInvoked = false
        val middleware = buildMiddleware(triggerDefaultPrompt = {
            isInvoked = true
        })

        val task = buildTask(type = ChecklistItem.Task.Type.SET_AS_DEFAULT)
        middleware.invoke(context, {}, AppAction.SetupChecklistAction.ChecklistItemClicked(task))

        assertTrue(isInvoked)
    }

    @Test
    fun `GIVEN click action contains a select theme task WHEN middleware is invoked THEN navigate to customize callback is invoked`() {
        var isInvoked = false
        val middleware = buildMiddleware(navigateToCustomize = {
            isInvoked = true
        })

        val task = buildTask(type = ChecklistItem.Task.Type.SELECT_THEME)
        middleware.invoke(context, {}, AppAction.SetupChecklistAction.ChecklistItemClicked(task))

        assertTrue(isInvoked)
    }

    @Test
    fun `GIVEN click action contains a change toolbar placement task WHEN middleware is invoked THEN navigate to customize callback is invoked`() {
        var isInvoked = false
        val middleware = buildMiddleware(navigateToCustomize = {
            isInvoked = true
        })

        val task = buildTask(type = ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT)
        middleware.invoke(context, {}, AppAction.SetupChecklistAction.ChecklistItemClicked(task))

        assertTrue(isInvoked)
    }

    @Test
    fun `GIVEN click action contains an install search widget task WHEN middleware is invoked THEN install search widget callback is invoked`() {
        var isInvoked = false
        val middleware = buildMiddleware(installSearchWidget = {
            isInvoked = true
        })

        val task = buildTask(type = ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET)
        middleware.invoke(context, {}, AppAction.SetupChecklistAction.ChecklistItemClicked(task))

        assertTrue(isInvoked)
    }

    @Test
    fun `GIVEN click action contains an explore extension task WHEN middleware is invoked THEN navigate to extension callback is invoked`() {
        var isInvoked = false
        val middleware = buildMiddleware(navigateToExtensions = {
            isInvoked = true
        })

        val task = buildTask(type = ChecklistItem.Task.Type.EXPLORE_EXTENSION)
        middleware.invoke(context, {}, AppAction.SetupChecklistAction.ChecklistItemClicked(task))

        assertTrue(isInvoked)
    }

    private fun buildMiddleware(
        triggerDefaultPrompt: () -> Unit = {},
        navigateToSignIn: () -> Unit = {},
        navigateToCustomize: () -> Unit = {},
        navigateToExtensions: () -> Unit = {},
        installSearchWidget: () -> Unit = {},
    ) = SetupChecklistMiddleware(
        triggerDefaultPrompt = triggerDefaultPrompt,
        navigateToSignIn = navigateToSignIn,
        navigateToCustomize = navigateToCustomize,
        navigateToExtensions = navigateToExtensions,
        installSearchWidget = installSearchWidget,
    )

    private fun buildTask(
        type: ChecklistItem.Task.Type,
    ) = ChecklistItem.Task(
        type = type,
        title = "A cool task",
        icon = R.drawable.ic_addons_extensions,
        isCompleted = true,
    )
}

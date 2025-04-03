/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.MiddlewareContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.junit.MockitoJUnitRunner
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem

@RunWith(MockitoJUnitRunner::class)
class SetupChecklistPreferencesMiddlewareTest {

    @Mock
    private lateinit var context: MiddlewareContext<AppState, AppAction>

    @Test
    fun `invoke sets the preference only when a relevant task is clicked`() {
        ChecklistItem.Task.Type.entries.forEach {
            val repository = FakeRepository(false)
            val middleware = buildMiddleware(repository)
            val task = buildTask(type = it)
            middleware.invoke(
                context,
                {},
                AppAction.SetupChecklistAction.ChecklistItemClicked(task),
            )

            when (it) {
                ChecklistItem.Task.Type.SELECT_THEME,
                ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT,
                ChecklistItem.Task.Type.EXPLORE_EXTENSION,
                -> assertTrue(repository.wasInvoked())

                ChecklistItem.Task.Type.SET_AS_DEFAULT,
                ChecklistItem.Task.Type.SIGN_IN,
                ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
                -> assertFalse(repository.wasInvoked())
            }
        }
    }

    @Test
    fun `invoke does not set the preference when the item is a group`() {
        val repository = FakeRepository(false)
        val middleware = buildMiddleware(repository)
        middleware.invoke(
            context,
            {},
            AppAction.SetupChecklistAction.ChecklistItemClicked(buildGroup()),
        )
        assertFalse(repository.wasInvoked())
    }

    private fun buildMiddleware(repository: SetupChecklistRepository) =
        SetupChecklistPreferencesMiddleware(repository)

    private fun buildGroup() = ChecklistItem.Group(
        title = 0,
        tasks = emptyList(),
        isExpanded = false,
    )

    private fun buildTask(type: ChecklistItem.Task.Type) = ChecklistItem.Task(
        type = type,
        title = 0,
        icon = 0,
        isCompleted = false,
    )
}

private class FakeRepository(var invoked: Boolean) : SetupChecklistRepository {
    fun wasInvoked() = invoked

    override fun getPreference(type: PreferenceType) = false

    override fun setPreference(type: PreferenceType, hasCompleted: Boolean) {
        invoked = true
    }
}

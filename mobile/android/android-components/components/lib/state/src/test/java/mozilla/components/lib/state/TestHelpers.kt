/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

data class TestState(
    val counter: Int,
) : State

fun reducer(state: TestState, action: TestAction): TestState = when (action) {
    is TestAction.IncrementAction -> state.copy(counter = state.counter + 1)
    is TestAction.DecrementAction -> state.copy(counter = state.counter - 1)
    is TestAction.SetValueAction -> state.copy(counter = action.value)
    is TestAction.DoubleAction -> state.copy(counter = state.counter * 2)
    is TestAction.DoNothingAction -> state
}

sealed class TestAction : Action {
    object IncrementAction : TestAction()
    object DecrementAction : TestAction()
    object DoNothingAction : TestAction()
    object DoubleAction : TestAction()
    data class SetValueAction(val value: Int) : TestAction()
}

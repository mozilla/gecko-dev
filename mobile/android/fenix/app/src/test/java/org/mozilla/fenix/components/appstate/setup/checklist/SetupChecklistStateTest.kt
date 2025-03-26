/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.setup.checklist

import org.junit.Assert.assertEquals
import org.junit.Test

class SetupChecklistStateTest {
    @Test
    fun `WHEN state is initialized THEN the initial state is correct`() {
        val expected = SetupChecklistState(
            checklistItems = emptyList(),
            progress = Progress(
                totalTasks = 0,
                completedTasks = 0,
            ),
        )
        assertEquals(expected, SetupChecklistState())
    }
}

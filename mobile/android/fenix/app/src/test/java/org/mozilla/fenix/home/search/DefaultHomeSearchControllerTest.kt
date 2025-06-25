/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.search

import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.UpdateSearchBeingActiveState

class DefaultHomeSearchControllerTest {
    @Test
    fun `GIVEN a browser search is in progress WHEN the home content is focused THEN update the application state`() {
        val appStore: AppStore = mockk(relaxed = true)
        val controller = DefaultHomeSearchController(appStore)

        controller.handleHomeContentFocusedWhileSearchIsActive()

        verify { appStore.dispatch(UpdateSearchBeingActiveState(false)) }
    }
}

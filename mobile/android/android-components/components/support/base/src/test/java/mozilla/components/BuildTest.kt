/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components

import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class BuildTest {
    @Test
    fun `Sanity check Build class`() {
        assertNotNull(Build.VERSION)
        assertNotNull(Build.APPLICATION_SERVICES_VERSION)
        assertNotNull(Build.GIT_HASH)

        assertTrue(Build.VERSION.isNotBlank())
        assertTrue(Build.APPLICATION_SERVICES_VERSION.isNotBlank())
        assertTrue(Build.GIT_HASH.isNotBlank())
    }
}

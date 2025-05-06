/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class DefaultHomepageAsANewTabPreferenceRepositoryTest {

    @Test
    fun `WHEN homepage as a new tab enabled getter is called THEN return the value in shared preferences`() {
        val settings = Settings(testContext)
        val repository = DefaultHomepageAsANewTabPreferenceRepository(settings)

        assertFalse(settings.enableHomepageAsNewTab)
        assertFalse(repository.getHomepageAsANewTabEnabled())

        settings.enableHomepageAsNewTab = true
        assertTrue(repository.getHomepageAsANewTabEnabled())

        settings.enableHomepageAsNewTab = false
        assertFalse(repository.getHomepageAsANewTabEnabled())
    }
}

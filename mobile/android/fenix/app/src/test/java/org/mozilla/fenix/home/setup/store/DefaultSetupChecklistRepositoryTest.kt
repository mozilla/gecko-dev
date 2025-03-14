/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class DefaultSetupChecklistRepositoryTest {

    @Test
    fun `WHEN getPreference is called for the toolbar step THEN the proper value is returned`() {
        val settings = Settings(testContext)
        val repository = DefaultSetupChecklistRepository(settings)

        assertFalse(repository.getPreference(PreferenceType.ToolbarComplete))

        settings.hasCompletedSetupStepToolbar = true
        assertTrue(repository.getPreference(PreferenceType.ToolbarComplete))

        settings.hasCompletedSetupStepToolbar = false
        assertFalse(repository.getPreference(PreferenceType.ToolbarComplete))
    }

    @Test
    fun `WHEN getPreference is called for the theme step THEN the proper value is returned`() {
        val settings = Settings(testContext)
        val repository = DefaultSetupChecklistRepository(settings)

        assertFalse(repository.getPreference(PreferenceType.ThemeComplete))

        settings.hasCompletedSetupStepTheme = true
        assertTrue(repository.getPreference(PreferenceType.ThemeComplete))

        settings.hasCompletedSetupStepTheme = false
        assertFalse(repository.getPreference(PreferenceType.ThemeComplete))
    }

    @Test
    fun `WHEN getPreference is called for the extensions step THEN the proper value is returned`() {
        val settings = Settings(testContext)
        val repository = DefaultSetupChecklistRepository(settings)

        assertFalse(repository.getPreference(PreferenceType.ExtensionsComplete))

        settings.hasCompletedSetupStepExtensions = true
        assertTrue(repository.getPreference(PreferenceType.ExtensionsComplete))

        settings.hasCompletedSetupStepExtensions = false
        assertFalse(repository.getPreference(PreferenceType.ExtensionsComplete))
    }
}

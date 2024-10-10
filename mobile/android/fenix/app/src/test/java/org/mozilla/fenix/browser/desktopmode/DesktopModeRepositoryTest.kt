package org.mozilla.fenix.browser.desktopmode

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.preferencesDataStore
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

private val Context.testDataStore: DataStore<Preferences> by preferencesDataStore(name = "DesktopModeRepositoryTest")

@RunWith(AndroidJUnit4::class)
class DesktopModeRepositoryTest {

    @After
    fun teardown() = runTest {
        testContext.testDataStore.edit { it.clear() }
    }

    @Test
    fun `GIVEN desktop mode defaults to true WHEN the repository is read for the first time THEN desktop mode should be enabled`() =
        runTest {
            val repository = createRepository(
                initialDesktopMode = true,
            )

            assertTrue(repository.getDesktopBrowsingEnabled())
        }

    @Test
    fun `GIVEN desktop mode defaults to false WHEN the repository is read for the first time THEN desktop mode should be disabled`() =
        runTest {
            val repository = createRepository(
                initialDesktopMode = false,
            )

            assertFalse(repository.getDesktopBrowsingEnabled())
        }

    @Test
    fun `WHEN the repository is written to THEN the preference is updated`() =
        runTest {
            val repository = createRepository()

            repository.setDesktopBrowsingEnabled(enabled = true)
            assertTrue(repository.getDesktopBrowsingEnabled())

            repository.setDesktopBrowsingEnabled(enabled = false)
            assertFalse(repository.getDesktopBrowsingEnabled())
        }

    private suspend fun createRepository(
        initialDesktopMode: Boolean = false,
    ) = DefaultDesktopModeRepository(
        context = testContext,
        dataStore = testContext.testDataStore,
    ).apply {
        setDesktopBrowsingEnabled(enabled = initialDesktopMode)
    }
}

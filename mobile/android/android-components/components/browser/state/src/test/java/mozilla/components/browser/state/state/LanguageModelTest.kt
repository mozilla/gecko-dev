/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.LanguageModel.Companion.PIVOT_LANGUAGE_CODE
import mozilla.components.concept.engine.translate.LanguageModel.Companion.areModelsProcessing
import mozilla.components.concept.engine.translate.LanguageModel.Companion.isPivotDownloaded
import mozilla.components.concept.engine.translate.LanguageModel.Companion.shouldPivotSync
import mozilla.components.concept.engine.translate.ModelManagementOptions
import mozilla.components.concept.engine.translate.ModelOperation
import mozilla.components.concept.engine.translate.ModelState
import mozilla.components.concept.engine.translate.OperationLevel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LanguageModelTest {

    private val mockLanguageModels = listOf(
        LanguageModel(language = Language(code = "es"), status = ModelState.NOT_DOWNLOADED, size = 111),
        LanguageModel(language = Language(code = "de"), status = ModelState.NOT_DOWNLOADED, size = 122),
        LanguageModel(language = Language(code = "fr"), status = ModelState.DOWNLOADED, size = 133),
        LanguageModel(language = Language(code = "en"), status = ModelState.DOWNLOADED, size = 144),
        LanguageModel(language = Language(code = "nn"), status = ModelState.ERROR_DELETION, size = 155),
        LanguageModel(language = Language(code = "it"), status = ModelState.ERROR_DOWNLOAD, size = 166),
    )

    @Test
    fun `GIVEN a language level model operation THEN update the state of that language only`() {
        val options = ModelManagementOptions(
            languageToManage = "es",
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.LANGUAGE,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            appLanguage = "any-code",
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        val expectedModelState = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 111),
            mockLanguageModels[1],
            mockLanguageModels[2],
            mockLanguageModels[3],
            mockLanguageModels[4],
            mockLanguageModels[5],
        )

        assertEquals(expectedModelState, newModelState)
    }

    @Test
    fun `GIVEN a language level model operation THEN do not update that language if it is a noop`() {
        val options = ModelManagementOptions(
            languageToManage = "en",
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.LANGUAGE,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            appLanguage = "any-code",
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        // Expect no state change, since downloading a downloaded model does not make sense.
        assertEquals(mockLanguageModels, newModelState)
    }

    @Test
    fun `GIVEN a language level model operation that fails THEN set a failure condition`() {
        val options = ModelManagementOptions(
            languageToManage = "es",
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.LANGUAGE,
        )

        // Simulated error state
        val errorState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.ERROR_DOWNLOAD else ModelState.ERROR_DELETION

        val newModelState = LanguageModel.determineNewLanguageModelState(
            appLanguage = "any-code",
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = errorState,
        )

        val expectedModelState = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.ERROR_DOWNLOAD, size = 111),
            mockLanguageModels[1],
            mockLanguageModels[2],
            mockLanguageModels[3],
            mockLanguageModels[4],
            mockLanguageModels[5],
        )

        // Expect the failure state occurs
        assertEquals(expectedModelState, newModelState)
    }

    @Test
    fun `GIVEN an all model operation THEN update models that aren't already in that state`() {
        val options = ModelManagementOptions(
            languageToManage = null,
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.ALL,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            appLanguage = "any-code",
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        val expectedModelState = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 111),
            LanguageModel(language = Language(code = "de"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 122),
            LanguageModel(language = Language(code = "fr"), status = ModelState.DOWNLOADED, size = 133),
            LanguageModel(language = Language(code = "en"), status = ModelState.DOWNLOADED, size = 144),
            LanguageModel(language = Language(code = "nn"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 155),
            LanguageModel(language = Language(code = "it"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 166),
        )
        assertEquals(expectedModelState, newModelState)
    }

    @Test
    fun `GIVEN a cached model operation THEN the state shouldn't change`() {
        val options = ModelManagementOptions(
            languageToManage = null,
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.CACHE,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val newModelState = LanguageModel.determineNewLanguageModelState(
            appLanguage = "any-code",
            currentLanguageModels = mockLanguageModels,
            options = options,
            newStatus = processState,
        )

        assertEquals(mockLanguageModels, newModelState)
    }

    @Test
    fun `GIVEN an operation without a pivot THEN the state of operation and pivot language should change`() {
        val options = ModelManagementOptions(
            languageToManage = "de",
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.LANGUAGE,
        )

        // Simulated process state before syncing with the engine
        val processState = if (options.operation == ModelOperation.DOWNLOAD) ModelState.DOWNLOAD_IN_PROGRESS else ModelState.DELETION_IN_PROGRESS

        val mockNonDownloadedPivot = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.NOT_DOWNLOADED, size = 111),
            LanguageModel(language = Language(code = "de"), status = ModelState.NOT_DOWNLOADED, size = 122),
            LanguageModel(language = Language(code = "fr"), status = ModelState.NOT_DOWNLOADED, size = 133),
            LanguageModel(language = Language(code = PIVOT_LANGUAGE_CODE), status = ModelState.NOT_DOWNLOADED, size = 144),
            LanguageModel(language = Language(code = "nn"), status = ModelState.NOT_DOWNLOADED, size = 155),
            LanguageModel(language = Language(code = "it"), status = ModelState.NOT_DOWNLOADED, size = 166),
        )

        val newModelState = LanguageModel.determineNewLanguageModelState(
            appLanguage = "any-code",
            currentLanguageModels = mockNonDownloadedPivot,
            options = options,
            newStatus = processState,
        )

        val expectedState = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.NOT_DOWNLOADED, size = 111),
            LanguageModel(language = Language(code = "de"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 122),
            LanguageModel(language = Language(code = "fr"), status = ModelState.NOT_DOWNLOADED, size = 133),
            LanguageModel(language = Language(code = PIVOT_LANGUAGE_CODE), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 144),
            LanguageModel(language = Language(code = "nn"), status = ModelState.NOT_DOWNLOADED, size = 155),
            LanguageModel(language = Language(code = "it"), status = ModelState.NOT_DOWNLOADED, size = 166),
        )

        assertEquals(expectedState, newModelState)
    }

    @Test
    fun `GIVEN a specified state THEN check if a pivot sync should occur`() {
        val options = ModelManagementOptions(
            languageToManage = "de",
            operation = ModelOperation.DOWNLOAD,
            operationLevel = OperationLevel.LANGUAGE,
        )

        // Pivot is downloaded
        assertFalse(shouldPivotSync(appLanguage = "any-code", languageModels = mockLanguageModels, options))

        val mockNonDownloadedPivot = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.NOT_DOWNLOADED, size = 111),
            LanguageModel(language = Language(code = "de"), status = ModelState.NOT_DOWNLOADED, size = 122),
            LanguageModel(language = Language(code = "fr"), status = ModelState.NOT_DOWNLOADED, size = 133),
            LanguageModel(language = Language(code = PIVOT_LANGUAGE_CODE), status = ModelState.NOT_DOWNLOADED, size = 144),
            LanguageModel(language = Language(code = "nn"), status = ModelState.NOT_DOWNLOADED, size = 155),
            LanguageModel(language = Language(code = "it"), status = ModelState.NOT_DOWNLOADED, size = 166),
        )

        // Pivot is downloaded and attempting to download de
        assertTrue(isPivotDownloaded(appLanguage = "any-code", languageModels = mockLanguageModels))
        assertFalse(
            shouldPivotSync(
                appLanguage = "any-code",
                languageModels = mockLanguageModels,
                options = options,
            ),
        )

        // Pivot is not downloaded and attempting to download de
        assertFalse(isPivotDownloaded(appLanguage = "any-code", languageModels = mockNonDownloadedPivot))
        assertTrue(
            shouldPivotSync(
                appLanguage = "any-code",
                languageModels = mockNonDownloadedPivot,
                options = options,
            ),
        )

        // English is the app language and pivot language, so no pivot sync
        assertFalse(
            shouldPivotSync(
                appLanguage = "en",
                languageModels = mockNonDownloadedPivot,
                options = options,
            ),
        )

        // When English is the language being operated on, then it shouldn't sync
        assertFalse(
            shouldPivotSync(
                appLanguage = "any-code",
                languageModels = mockNonDownloadedPivot,
                options =
                ModelManagementOptions(
                    languageToManage = "en",
                    operation = ModelOperation.DOWNLOAD,
                    operationLevel = OperationLevel.LANGUAGE,
                ),
            ),
        )

        val mockPivotNotMentioned = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.NOT_DOWNLOADED, size = 111),
            LanguageModel(language = Language(code = "de"), status = ModelState.NOT_DOWNLOADED, size = 122),
            LanguageModel(language = Language(code = "fr"), status = ModelState.NOT_DOWNLOADED, size = 133),
            LanguageModel(language = Language(code = "nn"), status = ModelState.NOT_DOWNLOADED, size = 155),
            LanguageModel(language = Language(code = "it"), status = ModelState.NOT_DOWNLOADED, size = 166),
        )

        // Pivot is not mentioned and attempting to download de
        // (Implicitly do not need pivot.)
        assertTrue(
            isPivotDownloaded(
                appLanguage = "any-code",
                languageModels = mockPivotNotMentioned,
            ),
        )
        assertFalse(
            shouldPivotSync(
                appLanguage = "any-code",
                languageModels = mockPivotNotMentioned,
                options = options,
            ),
        )
    }

    @Test
    fun `GIVEN a specified state THEN check if the language models are processing`() {
        // None are processing because there are no "_IN_PROGRESS" states.
        assertFalse(
            areModelsProcessing(
                languageModels = mockLanguageModels,
            ),
        )

        val mockLanguagesProcessing = listOf(
            LanguageModel(language = Language(code = "es"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 111),
            LanguageModel(language = Language(code = "de"), status = ModelState.DOWNLOAD_IN_PROGRESS, size = 122),
            LanguageModel(language = Language(code = "fr"), status = ModelState.NOT_DOWNLOADED, size = 133),
            LanguageModel(language = Language(code = PIVOT_LANGUAGE_CODE), status = ModelState.NOT_DOWNLOADED, size = 144),
            LanguageModel(language = Language(code = "nn"), status = ModelState.NOT_DOWNLOADED, size = 155),
            LanguageModel(language = Language(code = "it"), status = ModelState.NOT_DOWNLOADED, size = 166),
        )

        // Some are processing with "_IN_PROGRESS" states.
        assertTrue(
            areModelsProcessing(
                languageModels = mockLanguagesProcessing,
            ),
        )
    }

    @Test
    fun `GIVEN various state changes THEN the state should only be operable under certain conditions`() {
        // If a model is already downloaded, then we shouldn't switch to download in progress.
        assertFalse(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOADED, newStatus = ModelState.DOWNLOAD_IN_PROGRESS))

        // If a model is already not installed, then we shouldn't switch to deletion in progress.
        assertFalse(LanguageModel.checkIfOperable(currentStatus = ModelState.NOT_DOWNLOADED, newStatus = ModelState.DELETION_IN_PROGRESS))

        // If a model is already downloaded, then we can switch to deletion in progress.
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOADED, newStatus = ModelState.DELETION_IN_PROGRESS))

        // If a model is already not installed, then we can switch to download in progress.
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.NOT_DOWNLOADED, newStatus = ModelState.DOWNLOAD_IN_PROGRESS))

        // In process states should always be operable.
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOAD_IN_PROGRESS, newStatus = ModelState.DELETION_IN_PROGRESS))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOAD_IN_PROGRESS, newStatus = ModelState.DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOAD_IN_PROGRESS, newStatus = ModelState.NOT_DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DOWNLOAD_IN_PROGRESS, newStatus = ModelState.ERROR_DOWNLOAD))

        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DELETION_IN_PROGRESS, newStatus = ModelState.DOWNLOAD_IN_PROGRESS))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DELETION_IN_PROGRESS, newStatus = ModelState.DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DELETION_IN_PROGRESS, newStatus = ModelState.NOT_DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.DELETION_IN_PROGRESS, newStatus = ModelState.ERROR_DELETION))

        // Error states should always be operable
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.ERROR_DOWNLOAD, newStatus = ModelState.DOWNLOAD_IN_PROGRESS))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.ERROR_DOWNLOAD, newStatus = ModelState.DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.ERROR_DOWNLOAD, newStatus = ModelState.NOT_DOWNLOADED))

        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.ERROR_DELETION, newStatus = ModelState.DELETION_IN_PROGRESS))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.ERROR_DELETION, newStatus = ModelState.DOWNLOADED))
        assertTrue(LanguageModel.checkIfOperable(currentStatus = ModelState.ERROR_DELETION, newStatus = ModelState.NOT_DOWNLOADED))
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.translate

import android.os.Looper
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.engine.gecko.translate.GeckoTranslationUtils.intoTranslationError
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.ModelManagementOptions
import mozilla.components.concept.engine.translate.ModelOperation
import mozilla.components.concept.engine.translate.ModelState
import mozilla.components.concept.engine.translate.OperationLevel
import mozilla.components.concept.engine.translate.TranslationError
import org.junit.Assert
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.TranslationsController.Language
import org.mozilla.geckoview.TranslationsController.TranslationsException
import org.robolectric.RobolectricTestRunner
import org.robolectric.Shadows.shadowOf
import org.mozilla.geckoview.TranslationsController.RuntimeTranslation.LanguageModel as GeckoViewLanguageModel
import org.mozilla.geckoview.TranslationsController.RuntimeTranslation.TranslationSupport as GeckoViewTranslationSupport

/**
 * Creates a successfully completed [GeckoResult] for any type [T].
 * This is a convenience wrapper around GeckoResult.fromValue() for use in tests.
 * @param T The type of the value in the GeckoResult.
 * @param value The value to wrap in the successful GeckoResult. Can be null if T is nullable.
 * @return A successfully completed GeckoResult.
 */
fun <T> successGeckoResult(value: T): GeckoResult<T> {
    return GeckoResult.fromValue(value)
}

/**
 * Creates an exceptionally completed [GeckoResult] for any type [T].
 * This is a convenience wrapper around GeckoResult.fromException() for use in tests.
 * @param T The type the GeckoResult would have held if successful.
 * @param exception The throwable to wrap in the exceptional GeckoResult.
 * @return An exceptionally completed GeckoResult.
 */
fun <T> exceptionalGeckoResult(exception: Throwable): GeckoResult<T> {
    return GeckoResult.fromException(exception)
}

@RunWith(RobolectricTestRunner::class)
class GeckoTranslationUtilsTest {

    /**
     * Awaits the completion of a [GeckoResult] and executes provided callbacks for success or error.
     * This helper is intended for use in Robolectric tests within a coroutine scope (e.g., `runTest`).
     * It ensures that Robolectric's main looper is idled to process GeckoResult's callbacks.
     *
     * @param T The type of the value in the GeckoResult.
     * @param onSuccess A lambda to execute if the GeckoResult completes successfully. Receives the result value.
     * @param onError A lambda to execute if the GeckoResult completes exceptionally. Receives the throwable.
     */
    fun <T> GeckoResult<T>.awaitAndProcess(
        onSuccess: (value: T?) -> Unit,
        onError: (error: Throwable) -> Unit,
    ) {
        this.then(
            { value ->
                onSuccess(value)
                GeckoResult<Void>()
            },
            { error ->
                onError(error)
                GeckoResult<Void>()
            },
        )

        val mainLooper = Looper.getMainLooper()
        if (mainLooper != null) {
            shadowOf(mainLooper).idle()
        } else {
            throw AssertionError("Main Looper not found, cannot idle for GeckoResult completion.")
        }
    }

    @Test
    fun `intoTranslationError maps TranslationsException correctly`() {
        val unknownCode = 999
        val geckoException =
            TranslationsException(TranslationsException.ERROR_MODEL_COULD_NOT_DOWNLOAD)
        val unknownGeckoException =
            TranslationsException(unknownCode) // A code not explicitly mapped
        val genericException = RuntimeException("Some other error")

        val error1 = geckoException.intoTranslationError()
        assertTrue(error1 is TranslationError.ModelCouldNotDownloadError)
        assertEquals(geckoException, (error1 as TranslationError.ModelCouldNotDownloadError).cause)

        val error2 = unknownGeckoException.intoTranslationError()
        assertTrue(error2 is TranslationError.UnknownError)
        assertEquals(unknownGeckoException, (error2 as TranslationError.UnknownError).cause)

        val error3 = genericException.intoTranslationError()
        assertTrue(error3 is TranslationError.UnknownError)
        assertEquals(genericException, (error3 as TranslationError.UnknownError).cause)
    }

    @Test
    fun `mapGeckoViewLanguageModels maps GeckoViewLanguageModel to LanguageModel when all languages are valid`() =
        runTest {
            val english = GeckoViewLanguageModel(Language("en", "English"), true, 12)
            val deutsch = GeckoViewLanguageModel(Language("de", "Deutsch"), false, 13)
            val french = GeckoViewLanguageModel(Language("fr", "French"), true, 15)

            var onSuccessCalled = false
            var onErrorCalled = false

            val geckoResult: GeckoResult<List<GeckoViewLanguageModel>> =
            GeckoResult.allOf(
                GeckoResult.fromValue(english),
                GeckoResult.fromValue(deutsch),
                GeckoResult.fromValue(french),
            )

            val mappingResult = GeckoTranslationUtils.mapGeckoViewLanguageModels(geckoResult)

            mappingResult.awaitAndProcess(
                onSuccess = { languageList ->
                    onSuccessCalled = true
                    assertNotNull(languageList!!)
                    assertEquals(3, languageList.size)
                    assertEquals("en", languageList[0].language?.code)
                    assertEquals("de", languageList[1].language?.code)
                    assertEquals("fr", languageList[2].language?.code)

                    assertEquals(ModelState.DOWNLOADED, languageList[0].status)
                    assertEquals(ModelState.NOT_DOWNLOADED, languageList[1].status)
                    assertEquals(ModelState.DOWNLOADED, languageList[2].status)

                    assertEquals(12L, languageList[0].size)
                    assertEquals(13L, languageList[1].size)
                    assertEquals(15L, languageList[2].size)
                    GeckoResult<Void>()
                },
                onError = { throwable ->
                    onErrorCalled = true
                    fail("Unexpected mapping error")
                    GeckoResult<Void>()
                },
            )

            assertTrue("onSuccessCalled should be true", onSuccessCalled)
            assertFalse("onErrorCalled should be false", onErrorCalled)
        }

    @Test
    fun `mapGeckoViewLanguageModels maps GeckoViewLanguageModel to LanguageModel when one model is invalid`() =
        runTest {
            val english = GeckoViewLanguageModel(Language("en", "English"), true, 12)
            val deutsch = GeckoViewLanguageModel(Language("de", "Deutsch"), false, 13)
            val french = GeckoViewLanguageModel(Language("fr", "French"), true, 15)

            var onSuccessCalled = false
            var onErrorCalled = false

            val geckoResult: GeckoResult<List<GeckoViewLanguageModel>> =
                GeckoResult.allOf(
                    successGeckoResult(english),
                    successGeckoResult(deutsch),
                    exceptionalGeckoResult(
                        RuntimeException("Some error"),
                    ),
                    successGeckoResult(french),
                )

            val mappingResult = GeckoTranslationUtils.mapGeckoViewLanguageModels(geckoResult)

            mappingResult.awaitAndProcess(
                onSuccess = { languageList ->
                    onSuccessCalled = true
                    fail("Unexpected mapping success when one language is invalid")
                },
                onError = { throwable ->
                    onErrorCalled = true
                    assertTrue(throwable is RuntimeException)
                    assertEquals("Some error", throwable.message)
                    GeckoResult<Void>()
                },
            )

            assertFalse("onSuccessCalled should be false", onSuccessCalled)
            assertTrue("onErrorCalled should be true", onErrorCalled)
        }

    @Test
    fun `mapGeckoTranslationSupport maps successfully`() = runTest {
        val gvSupport = GeckoViewTranslationSupport(
            listOf(Language("en", "English"), Language("es", "Español")),
            listOf(Language("de", "Deutsch"), Language("fr", "Français")),
        )
        val sourceResult = successGeckoResult(gvSupport)
        val mappingResult = GeckoTranslationUtils.mapGeckoTranslationSupport(sourceResult)

        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = { translationSupport ->
                onSuccessCalled = true

                assertNotNull(translationSupport!!)

                assertEquals(2, translationSupport.fromLanguages?.size)
                assertEquals("en", translationSupport.fromLanguages?.get(0)?.code)
                assertEquals("es", translationSupport.fromLanguages?.get(1)?.code)

                assertEquals(2, translationSupport.toLanguages?.size)
                assertEquals("de", translationSupport.toLanguages?.get(0)?.code)
                assertEquals("fr", translationSupport.toLanguages?.get(1)?.code)
            },
            onError = {
                onErrorCalled = true
                fail("Unexpected error: $it")
            },
        )

        assertTrue("onSuccessCalled should be true", onSuccessCalled)
        assertFalse("onErrorCalled should be false", onErrorCalled)
    }

    @Test
    fun `mapGeckoTranslationSupport maps successfully with empty language lists`() = runTest {
        val gvSupport = GeckoViewTranslationSupport(emptyList(), emptyList())
        val sourceResult = successGeckoResult(gvSupport)
        val mappingResult = GeckoTranslationUtils.mapGeckoTranslationSupport(sourceResult)

        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = { translationSupport ->
                onSuccessCalled = true
                assertNotNull(translationSupport!!)
                assertTrue(translationSupport.fromLanguages?.isEmpty() == true)
                assertTrue(translationSupport.toLanguages?.isEmpty() == true)
            },
            onError = {
                onErrorCalled = true
                fail("Unexpected error: $it")
            },
        )
        assertTrue("onSuccessCalled should be true", onSuccessCalled)
        assertFalse("onErrorCalled should be false", onErrorCalled)
    }

    @Test
    fun `mapGeckoTranslationSupport propagates error when input GeckoResult is exceptional`() =
        runTest {
            val underlyingException = RuntimeException("Source GeckoResult failed")
            val exceptionalSourceResult: GeckoResult<GeckoViewTranslationSupport> =
                exceptionalGeckoResult(underlyingException)
            val mappingResult =
                GeckoTranslationUtils.mapGeckoTranslationSupport(exceptionalSourceResult)

            var onSuccessCalled = false
            var onErrorCalled = false

            mappingResult.awaitAndProcess(
                onSuccess = {
                    onSuccessCalled = true
                    fail("onSuccess should not be called")
                },
                onError = { error ->
                    onErrorCalled = true
                    assertEquals(underlyingException, error)
                },
            )
            assertFalse("onSuccessCalled should be false", onSuccessCalled)
            assertTrue("onErrorCalled should be true", onErrorCalled)
        }

    @Test
    fun `buildGeckoModelManagementOptions builds correctly`() {
        val options = ModelManagementOptions(
            languageToManage = "en",
            operation = ModelOperation.DELETE,
            operationLevel = OperationLevel.ALL,
        )
        val gvOptions = GeckoTranslationUtils.buildGeckoModelManagementOptions(options)

        assertEquals("delete", gvOptions.operation)
        assertEquals("all", gvOptions.operationLevel)
        assertEquals("en", gvOptions.language)
    }

    @Test
    fun `buildGeckoModelManagementOptions builds correctly without languageToManage`() {
        val options = ModelManagementOptions(
            operation = ModelOperation.DELETE,
            operationLevel = OperationLevel.CACHE,
        )
        val gvOptions = GeckoTranslationUtils.buildGeckoModelManagementOptions(options)

        assertEquals("delete", gvOptions.operation)
        assertEquals("cache", gvOptions.operationLevel)
        Assert.assertNull(gvOptions.language)
    }

    @Test
    fun `mapLanguageSetting maps valid string successfully`() = runTest {
        val sourceResult = successGeckoResult("always")
        val mappingResult = GeckoTranslationUtils.mapLanguageSetting(sourceResult)
        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = { setting ->
                onSuccessCalled = true
                assertEquals(LanguageSetting.ALWAYS, setting)
            },
            onError = {
                onErrorCalled = true
                fail("Unexpected error: $it")
            },
        )
        assertTrue("onSuccessCalled should be true", onSuccessCalled)
        assertFalse("onErrorCalled should be false", onErrorCalled)
    }

    @Test
    fun `mapLanguageSetting maps unexpected case string successfully`() = runTest {
        val sourceResult = successGeckoResult("alWays")
        val mappingResult = GeckoTranslationUtils.mapLanguageSetting(sourceResult)
        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = { setting ->
                onSuccessCalled = true
                assertEquals(LanguageSetting.ALWAYS, setting)
            },
            onError = {
                onErrorCalled = true
                fail("Unexpected error: $it")
            },
        )
        assertTrue("onSuccessCalled should be true", onSuccessCalled)
        assertFalse("onErrorCalled should be false", onErrorCalled)
    }

    @Test
    fun `mapLanguageSetting results in error for invalid string`() = runTest {
        val invalidSettingString = "invalid_setting_string"
        val sourceResult = successGeckoResult(invalidSettingString)
        val mappingResult = GeckoTranslationUtils.mapLanguageSetting(sourceResult)
        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = {
                onSuccessCalled = true
                fail("onSuccess should not be called for invalid string")
            },
            onError = { error ->
                onErrorCalled = true
                assertTrue(error is IllegalArgumentException)
                assertTrue(error.message!!.contains("The language setting $invalidSettingString is not mapped"))
            },
        )
        assertFalse("onSuccessCalled should be false", onSuccessCalled)
        assertTrue("onErrorCalled should be true", onErrorCalled)
    }

    @Test
    fun `mapLanguageSetting propagates error when input GeckoResult is exceptional`() = runTest {
        val underlyingException = RuntimeException("Source failed")
        val exceptionalSource: GeckoResult<String> = exceptionalGeckoResult(underlyingException)
        val mappingResult = GeckoTranslationUtils.mapLanguageSetting(exceptionalSource)
        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = {
                onSuccessCalled = true
                fail("onSuccess should not be called")
            },
            onError = { error ->
                onErrorCalled = true
                assertEquals(underlyingException, error)
            },
        )
        assertFalse("onSuccessCalled should be false", onSuccessCalled)
        assertTrue("onErrorCalled should be true", onErrorCalled)
    }

    // --- Tests for mapToLanguageSettingMap ---
    @Test
    fun `mapToLanguageSettingMap maps valid map successfully`() = runTest {
        val stringMap = mapOf(
            "en" to "always",
            "es" to "never",
        )

        val sourceResult = successGeckoResult(stringMap)
        val mappingResult = GeckoTranslationUtils.mapToLanguageSettingMap(sourceResult)
        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = { settingMap ->
                onSuccessCalled = true
                assertNotNull(settingMap)
                assertEquals(2, settingMap!!.size)
                assertEquals(LanguageSetting.ALWAYS, settingMap["en"])
                assertEquals(LanguageSetting.NEVER, settingMap["es"])
            },
            onError = {
                onErrorCalled = true
                fail("Unexpected error: $it")
            },
        )
        assertTrue("onSuccessCalled should be true", onSuccessCalled)
        assertFalse("onErrorCalled should be false", onErrorCalled)
    }

    @Test
    fun `mapToLanguageSettingMap results in error for map with one invalid setting string`() = runTest {
        val invalidSettingString = "invalid_setting_string"

        val stringMap = mapOf(
            "en" to invalidSettingString,
            "es" to "never",
        )

        val sourceResult = successGeckoResult(stringMap)
        val mappingResult = GeckoTranslationUtils.mapToLanguageSettingMap(sourceResult)
        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = {
                onSuccessCalled = true
                fail("onSuccess should not be called")
            },
            onError = { error ->
                onErrorCalled = true
                assertTrue(error is IllegalArgumentException)
                assertTrue(error.message!!.contains("The language setting $invalidSettingString is not mapped"))
            },
        )
        assertFalse("onSuccessCalled should be false", onSuccessCalled)
        assertTrue("onErrorCalled should be true", onErrorCalled)
    }

    @Test
    fun `mapToLanguageSettingMap propagates error when input GeckoResult is exceptional`() = runTest {
        val underlyingException = RuntimeException("Source failed")
        val exceptionalSource: GeckoResult<Map<String, String>> =
            exceptionalGeckoResult(underlyingException)
        val mappingResult = GeckoTranslationUtils.mapToLanguageSettingMap(exceptionalSource)
        var onSuccessCalled = false
        var onErrorCalled = false

        mappingResult.awaitAndProcess(
            onSuccess = {
                onSuccessCalled = true
                fail("onSuccess should not be called")
            },
            onError = { error ->
                onErrorCalled = true
                assertEquals(underlyingException, error)
            },
        )
        assertFalse("onSuccessCalled should be false", onSuccessCalled)
        assertTrue("onErrorCalled should be true", onErrorCalled)
    }
}

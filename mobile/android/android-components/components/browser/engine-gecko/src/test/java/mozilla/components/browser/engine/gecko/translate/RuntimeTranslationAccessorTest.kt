/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.translate

import mozilla.components.concept.engine.translate.TranslationError
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.MockitoAnnotations
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.TranslationsController.TranslationsException
import org.mozilla.geckoview.TranslationsController.TranslationsException.ERROR_COULD_NOT_TRANSLATE
import org.robolectric.RobolectricTestRunner
import org.robolectric.shadows.ShadowLooper

@RunWith(RobolectricTestRunner::class)
class RuntimeTranslationAccessorTest {

    private lateinit var accessor: DefaultRuntimeTranslationAccessor

    @Before
    fun setUp() {
        MockitoAnnotations.openMocks(this)
        accessor = DefaultRuntimeTranslationAccessor()
    }

    // Helper to create a successfully resolved GeckoResult
    private fun <T> successfulGeckoResult(value: T?): GeckoResult<T> {
        val result = GeckoResult<T>()
        result.complete(value)
        return result
    }

    // Helper to create a failed GeckoResult
    private fun <T> failedGeckoResult(exception: Throwable): GeckoResult<T> {
        val result = GeckoResult<T>()
        result.completeExceptionally(exception)
        return result
    }

    @Test
    fun `handleGeckoResult with successful GeckoResult and non-null value`() {
        var successValue: String? = null
        var errorValue: TranslationError? = null

        val geckoResult = successfulGeckoResult("Test Success")

        accessor.handleGeckoResult(
            geckoResult,
            onSuccess = { successValue = it },
            onError = { errorValue = it },
        )

        ShadowLooper.idleMainLooper()

        assertEquals("Test Success", successValue)
        assertNull(errorValue)
    }

    @Test
    fun `handleGeckoResult with successful GeckoResult (null value)`() {
        var successValue: String? = "Initial"
        var errorValue: TranslationError? = null
        val geckoResult = successfulGeckoResult<String>(null)

        accessor.handleGeckoResult(
            geckoResult,
            onSuccess = { successValue = it },
            onError = { errorValue = it },
        )

        ShadowLooper.idleMainLooper()

        assertEquals("Initial", successValue)
        assertNotNull(errorValue)
        assertTrue(errorValue is TranslationError.UnexpectedNull)
    }

    @Test
    fun `handleGeckoResult with successful GeckoResult (Unit value)`() {
        var successCalled = false
        var errorValue: TranslationError? = null
        val geckoResult = successfulGeckoResult<Unit>(Unit)

        accessor.handleGeckoResult(
            geckoResult,
            onSuccess = { successCalled = true },
            onError = { errorValue = it },
        )

        ShadowLooper.idleMainLooper()

        assertTrue(successCalled)
        assertNull(errorValue)
    }

    @Test
    fun `handleGeckoResult with failed GeckoResult`() {
        var successValue: String? = null
        var errorValue: TranslationError? = null
        val exception = RuntimeException("Failure")
        val geckoResult = failedGeckoResult<String>(exception)

        accessor.handleGeckoResult(
            geckoResult,
            onSuccess = { successValue = it },
            onError = { errorValue = it },
        )

        ShadowLooper.idleMainLooper()

        assertNull(successValue)
        assertNotNull(errorValue)
        assertTrue(errorValue is TranslationError.UnknownError)
        assertEquals(exception, (errorValue as TranslationError.UnknownError).cause)
    }

    @Test
    fun `handleGeckoResult with custom exception mapping`() {
        var successValue: String? = null
        var errorValue: TranslationError? = null
        val specificException = TranslationsException(ERROR_COULD_NOT_TRANSLATE)
        val geckoResult = failedGeckoResult<String>(specificException)

        accessor.handleGeckoResult(
            geckoResult,
            onSuccess = { successValue = it },
            onError = { errorValue = it },
        )

        ShadowLooper.idleMainLooper()

        assertNull(successValue)
        assertNotNull(errorValue)
        assertTrue(errorValue is TranslationError.CouldNotTranslateError)
    }
}

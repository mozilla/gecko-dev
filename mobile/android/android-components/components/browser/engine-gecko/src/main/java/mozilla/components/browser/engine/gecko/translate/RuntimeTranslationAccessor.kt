/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.translate

import mozilla.components.browser.engine.gecko.translate.GeckoTranslationUtils.buildGeckoModelManagementOptions
import mozilla.components.browser.engine.gecko.translate.GeckoTranslationUtils.intoTranslationError
import mozilla.components.browser.engine.gecko.translate.GeckoTranslationUtils.mapGeckoTranslationSupport
import mozilla.components.browser.engine.gecko.translate.GeckoTranslationUtils.mapGeckoViewLanguageModels
import mozilla.components.browser.engine.gecko.translate.GeckoTranslationUtils.mapLanguageSetting
import mozilla.components.browser.engine.gecko.translate.GeckoTranslationUtils.mapToLanguageSettingMap
import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.ModelManagementOptions
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationSupport
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.TranslationsController

/**
 * Accessor interface for interacting with the static methods of
 * [org.mozilla.geckoview.TranslationsController.RuntimeTranslation].
 *
 * This interface provides a way to abstract the static calls, primarily for testability.
 * It mirrors the callback-based asynchronous pattern used by consumers like `GeckoEngine`.
 *
 * Instead of returning a result object (like GeckoResult), each method accepts
 * `onSuccess` and `onError` callbacks to handle the asynchronous outcome.
 */
interface RuntimeTranslationAccessor {

    /**
     * Checks if the translations engine is supported by the current runtime.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.isTranslationsEngineSupported].
     *
     * @param onSuccess Callback invoked with `true` if supported, `false` otherwise.
     * @param onError Callback invoked if the check fails or an error occurs.
     */
    fun isTranslationsEngineSupported(
        onSuccess: (Boolean) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Retrieves the estimated download size for a given language pair.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.checkPairDownloadSize].
     *
     * @param fromLanguage The BCP-47 language code of the source language.
     * @param toLanguage The BCP-47 language code of the target language.
     * @param onSuccess Callback invoked with the download size in bytes.
     * @param onError Callback invoked if the operation fails.
     */
    fun getTranslationsPairDownloadSize(
        fromLanguage: String,
        toLanguage: String,
        onSuccess: (Long) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Lists the download states of all available translation language models.
     * Adapts the result from `TranslationsController` to `List<LanguageModel>`.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.listModelDownloadStates].
     *
     * @param onSuccess Callback invoked with a list of [LanguageModel] objects.
     * @param onError Callback invoked if the operation fails.
     */
    fun getTranslationsModelDownloadStates(
        onSuccess: (List<LanguageModel>) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Retrieves the list of supported "from" and "to" languages for translation.
     * Adapts the result to the `TranslationSupport` data class.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.listSupportedLanguages].
     *
     * @param onSuccess Callback invoked with a [TranslationSupport] object.
     * @param onError Callback invoked if the operation fails.
     */
    fun getSupportedTranslationLanguages(
        onSuccess: (TranslationSupport) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Manages translation language models (e.g., install, remove).
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.manageLanguageModel].
     *
     * @param options The [ManageModelOptions] specifying the operation to perform.
     * @param onSuccess Callback invoked on successful completion.
     * @param onError Callback invoked if the operation fails.
     */
    fun manageTranslationsLanguageModel(
        options: ModelManagementOptions,
        onSuccess: () -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Retrieves the list of user-preferred languages as BCP-47 codes.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.preferredLanguages].
     *
     * @param onSuccess Callback invoked with a list of language code strings.
     * @param onError Callback invoked if the operation fails.
     */
    fun getUserPreferredLanguages(
        onSuccess: (List<String>) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Retrieves the translation setting for a specific language.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.getLanguageSetting].
     *
     * @param languageCode The BCP-47 language code.
     * @param onSuccess Callback invoked with the [LanguageSetting] for the language.
     * @param onError Callback invoked if the operation fails.
     */
    fun getLanguageSetting(
        languageCode: String,
        onSuccess: (LanguageSetting) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Sets the translation setting for a specific language.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.setLanguageSettings].
     *
     * @param languageCode The BCP-47 language code.
     * @param setting The [LanguageSetting] to apply.
     * @param onSuccess Callback invoked on successful completion.
     * @param onError Callback invoked if the operation fails.
     */
    fun setLanguageSetting(
        languageCode: String,
        languageSetting: LanguageSetting,
        onSuccess: () -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Retrieves all language translation settings.
     * Adapts to `Map<String, LanguageSetting>`.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.getLanguageSettings].
     *
     * @param onSuccess Callback invoked with a map of language codes to their [LanguageSetting].
     * @param onError Callback invoked if the operation fails.
     */
    fun getLanguageSettings(
        onSuccess: (Map<String, LanguageSetting>) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Retrieves the list of sites for which translation should never be offered.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.getNeverTranslateSiteList].
     *
     * @param onSuccess Callback invoked with a list of site origin strings.
     * @param onError Callback invoked if the operation fails.
     */
    fun getNeverTranslateSiteList(
        onSuccess: (List<String>) -> Unit,
        onError: (TranslationError) -> Unit,
    )

    /**
     * Sets or unsets a site in the "never translate" list.
     * Corresponds to [org.mozilla.geckoview.TranslationsController.RuntimeTranslation.setNeverTranslateSpecifiedSite].
     *
     * @param origin The origin of the site (e.g., "https://example.com").
     * @param neverTranslate `true` to add the site to the never-translate list, `false` to remove it.
     * @param onSuccess Callback invoked on successful completion.
     * @param onError Callback invoked if the operation fails.
     */
    fun setNeverTranslateSpecifiedSite(
        origin: String,
        neverTranslate: Boolean,
        onSuccess: () -> Unit,
        onError: (TranslationError) -> Unit,
    )
}

/**
 * Default implementation of [RuntimeTranslationAccessor].
 *
 * This class directly delegates calls to the static methods of
 * [TranslationsController.RuntimeTranslation] and invokes the provided
 * `onSuccess` or `onError` callbacks based on the outcome of the underlying
 * [GeckoResult]. It also handles adapting results to the types expected by the callbacks.
 */
internal class DefaultRuntimeTranslationAccessor : RuntimeTranslationAccessor {

    /**
     * Handles the result of a [GeckoResult] operation.
     *
     * This function processes a [GeckoResult], which represents an asynchronous operation
     * that can either succeed with a value or fail with an error.
     *
     * If the `geckoResult` completes successfully:
     * - If the result value is not null, the `onSuccess` callback is invoked with the value.
     * - If the result value is null, the `onError` callback is invoked with an [TranslationError.UnexpectedNull].
     *
     * If the `geckoResult` fails:
     * - The `onError` callback is invoked with a [TranslationError] converted from the `Throwable`.
     *
     * @param T The type of the successful result value. Must be a non-nullable type.
     * @param geckoResult The [GeckoResult] to handle.
     * @param onSuccess A callback function to be invoked if the operation succeeds with a non-null value.
     *                  It takes the successful result of type [T] as a parameter.
     * @param onError A callback function to be invoked if the operation fails or if the successful
     *                result is null. It takes a [TranslationError] as a parameter.
     */
    internal fun <T : Any> handleGeckoResult(
        geckoResult: GeckoResult<T>,
        onSuccess: (T) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        geckoResult.then(
            { resultValue: T? ->
                if (resultValue != null) {
                    onSuccess(resultValue)
                } else {
                    onError(TranslationError.UnexpectedNull())
                }
                GeckoResult<Void>()
            },
            { throwable ->
                onError(throwable.intoTranslationError())
                GeckoResult<Void>()
            },
        )
    }

    /**
     * Handles the result of a [GeckoResult] operation that does not produce a value (i.e., `Void`).
     *
     * This function processes a [GeckoResult] representing an asynchronous operation
     * that completes either successfully (without a specific value) or fails with an error.
     *
     * If the `geckoResult` completes successfully:
     * - The `onSuccess` callback is invoked.
     *
     * If the `geckoResult` fails:
     * - The `onError` callback is invoked with a [TranslationError] converted from the `Throwable`.
     *
     * @param TVoid The type of the result, typically `Void` or a similar type indicating no meaningful value.
     * @param geckoResult The [GeckoResult] to handle.
     * @param onSuccess A callback function to be invoked if the operation succeeds.
     * @param onError A callback function to be invoked if the operation fails,
     *                taking a [TranslationError] as a parameter.
     */
    internal fun <TVoid> handleVoidGeckoResult(
        geckoResult: GeckoResult<TVoid>,
        onSuccess: () -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        geckoResult.then(
            { _ ->
                onSuccess()
                GeckoResult<Void>()
            },
            { throwable ->
                onError(throwable.intoTranslationError())
                GeckoResult<Void>()
            },
        )
    }

    override fun isTranslationsEngineSupported(
        onSuccess: (Boolean) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        handleGeckoResult(
            TranslationsController.RuntimeTranslation.isTranslationsEngineSupported(),
            onSuccess,
            onError,
        )
    }

    override fun getTranslationsPairDownloadSize(
        fromLanguage: String,
        toLanguage: String,
        onSuccess: (Long) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        handleGeckoResult(
            TranslationsController.RuntimeTranslation.checkPairDownloadSize(
                fromLanguage,
                toLanguage,
            ),
            onSuccess,
            onError,
        )
    }

    override fun getTranslationsModelDownloadStates(
        onSuccess: (List<LanguageModel>) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        val geckoResult = mapGeckoViewLanguageModels(
            TranslationsController.RuntimeTranslation.listModelDownloadStates(),
        )

        handleGeckoResult(
            geckoResult,
            onSuccess = onSuccess,
            onError = onError,
        )
    }

    override fun getSupportedTranslationLanguages(
        onSuccess: (TranslationSupport) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        val geckoResult = mapGeckoTranslationSupport(
            TranslationsController.RuntimeTranslation.listSupportedLanguages(),
        )

        handleGeckoResult(
            geckoResult,
            onSuccess = onSuccess,
            onError = onError,
        )
    }

    override fun manageTranslationsLanguageModel(
        options: ModelManagementOptions,
        onSuccess: () -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        val geckoModelManagementOptions = buildGeckoModelManagementOptions(options)

        handleVoidGeckoResult(
            TranslationsController.RuntimeTranslation.manageLanguageModel(geckoModelManagementOptions),
            onSuccess,
            onError,
        )
    }

    override fun getUserPreferredLanguages(
        onSuccess: (List<String>) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        handleGeckoResult(
            TranslationsController.RuntimeTranslation.preferredLanguages(),
            onSuccess,
            onError,
        )
    }

    override fun getLanguageSetting(
        languageCode: String,
        onSuccess: (LanguageSetting) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        val geckoResult = mapLanguageSetting(
            TranslationsController.RuntimeTranslation.getLanguageSetting(languageCode),
        )

        handleGeckoResult(
            geckoResult,
            onSuccess,
            onError,
        )
    }

    override fun setLanguageSetting(
        languageCode: String,
        languageSetting: LanguageSetting,
        onSuccess: () -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        handleVoidGeckoResult(
            TranslationsController.RuntimeTranslation.setLanguageSettings(
                languageCode,
                languageSetting.name,
            ),
            onSuccess,
            onError,
        )
    }

    override fun getLanguageSettings(
        onSuccess: (Map<String, LanguageSetting>) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        val geckoResult = mapToLanguageSettingMap(
            TranslationsController.RuntimeTranslation.getLanguageSettings(),
        )

        handleGeckoResult(
            geckoResult,
            onSuccess = onSuccess,
            onError = onError,
        )
    }

    override fun getNeverTranslateSiteList(
        onSuccess: (List<String>) -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        handleGeckoResult(
            TranslationsController.RuntimeTranslation.getNeverTranslateSiteList(),
            onSuccess,
            onError,
        )
    }

    override fun setNeverTranslateSpecifiedSite(
        origin: String,
        neverTranslate: Boolean,
        onSuccess: () -> Unit,
        onError: (TranslationError) -> Unit,
    ) {
        handleVoidGeckoResult(
            TranslationsController.RuntimeTranslation.setNeverTranslateSpecifiedSite(
                neverTranslate,
                origin,
            ),
            onSuccess,
            onError,
        )
    }
}

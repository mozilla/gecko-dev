/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package mozilla.components.browser.engine.gecko.translate

import mozilla.components.concept.engine.translate.Language
import mozilla.components.concept.engine.translate.LanguageModel
import mozilla.components.concept.engine.translate.LanguageSetting
import mozilla.components.concept.engine.translate.ModelManagementOptions
import mozilla.components.concept.engine.translate.ModelState
import mozilla.components.concept.engine.translate.TranslationError
import mozilla.components.concept.engine.translate.TranslationSupport
import org.mozilla.geckoview.GeckoResult
import org.mozilla.geckoview.TranslationsController.TranslationsException
import org.mozilla.geckoview.TranslationsController.RuntimeTranslation.LanguageModel as GeckoViewLanguageModel
import org.mozilla.geckoview.TranslationsController.RuntimeTranslation.ModelManagementOptions as GeckoViewModelManagementOptions
import org.mozilla.geckoview.TranslationsController.RuntimeTranslation.TranslationSupport as GeckoViewTranslationSupport

/**
 * Utility file for translations functions related to the Gecko implementation.
 */
object GeckoTranslationUtils {

    /**
     * Convenience method for mapping a [TranslationsException] to the Android Components defined
     * error type of [TranslationError].
     *
     * Throwable is the engine throwable that occurred during translating. Ordinarily should be
     * a [TranslationsException].
     */
    fun Throwable.intoTranslationError(): TranslationError {
        return if (this is TranslationsException) {
            when ((this).code) {
                TranslationsException.ERROR_UNKNOWN ->
                    TranslationError.UnknownError(this)

                TranslationsException.ERROR_ENGINE_NOT_SUPPORTED ->
                    TranslationError.EngineNotSupportedError(this)

                TranslationsException.ERROR_COULD_NOT_TRANSLATE ->
                    TranslationError.CouldNotTranslateError(this)

                TranslationsException.ERROR_COULD_NOT_RESTORE ->
                    TranslationError.CouldNotRestoreError(this)

                TranslationsException.ERROR_COULD_NOT_LOAD_LANGUAGES ->
                    TranslationError.CouldNotLoadLanguagesError(this)

                TranslationsException.ERROR_LANGUAGE_NOT_SUPPORTED ->
                    TranslationError.LanguageNotSupportedError(this)

                TranslationsException.ERROR_MODEL_COULD_NOT_RETRIEVE ->
                    TranslationError.ModelCouldNotRetrieveError(this)

                TranslationsException.ERROR_MODEL_COULD_NOT_DELETE ->
                    TranslationError.ModelCouldNotDeleteError(this)

                TranslationsException.ERROR_MODEL_COULD_NOT_DOWNLOAD ->
                    TranslationError.ModelCouldNotDownloadError(this)

                TranslationsException.ERROR_MODEL_LANGUAGE_REQUIRED ->
                    TranslationError.ModelLanguageRequiredError(this)

                TranslationsException.ERROR_MODEL_DOWNLOAD_REQUIRED ->
                    TranslationError.ModelDownloadRequiredError(this)

                else -> TranslationError.UnknownError(this)
            }
        } else {
            TranslationError.UnknownError(this)
        }
    }

    /**
     * Maps a [GeckoResult] of [GeckoViewLanguageModel] to a [GeckoResult] of [LanguageModel].
     *
     * This function transforms the language models from the GeckoView representation to the
     * Android Components representation. It handles nullable values and ensures that only valid
     * language models are included in the result.
     *
     * @param geckoViewLanguageModels The [GeckoResult] containing a list of [GeckoViewLanguageModel]
     * objects to be mapped.
     * @return A [GeckoResult] containing a list of [LanguageModel] objects.
     */
    internal fun mapGeckoViewLanguageModels(
        geckoViewLanguageModels: GeckoResult<List<GeckoViewLanguageModel>>,
    ): GeckoResult<List<LanguageModel>> {
        return geckoViewLanguageModels.map { listInternal: List<GeckoViewLanguageModel?>? ->
            listInternal?.mapNotNull { geckoViewLanguageModel: GeckoViewLanguageModel? ->
                val languageCode = geckoViewLanguageModel?.language?.code ?: return@mapNotNull null
                val localizedDisplayName = geckoViewLanguageModel.language?.localizedDisplayName
                LanguageModel(
                    language = Language(
                        code = languageCode,
                        localizedDisplayName = localizedDisplayName,
                    ),
                    status = if (geckoViewLanguageModel.isDownloaded == true) {
                        ModelState.DOWNLOADED
                    } else {
                        ModelState.NOT_DOWNLOADED
                    },
                    size = geckoViewLanguageModel.size,
                )
            }
        }
    }

    /**
     * Maps a [GeckoResult] of [GeckoViewTranslationSupport] to a [GeckoResult] of [TranslationSupport].
     *
     * This function transforms the translation support information from the GeckoView representation
     * to the Android Components representation. It handles nullable values and ensures that only valid
     * language information is included in the result.
     *
     * @param geckoResultWithGeckoSupport The [GeckoResult] containing [GeckoViewTranslationSupport]
     * to be mapped.
     * @return A [GeckoResult] containing [TranslationSupport].
     */
    internal fun mapGeckoTranslationSupport(
        geckoResultWithGeckoSupport: GeckoResult<GeckoViewTranslationSupport>,
    ): GeckoResult<TranslationSupport> {
        return geckoResultWithGeckoSupport.map { geckoSupportInternal: GeckoViewTranslationSupport? ->
            if (geckoSupportInternal == null) {
                null
            } else {
                val fromLanguages = geckoSupportInternal.fromLanguages?.map { lang ->
                    Language(lang.code, lang.localizedDisplayName)
                } ?: emptyList()
                val toLanguages = geckoSupportInternal.toLanguages?.map { lang ->
                    Language(lang.code, lang.localizedDisplayName)
                } ?: emptyList()

                TranslationSupport(
                    fromLanguages = fromLanguages,
                    toLanguages = toLanguages,
                )
            }
        }
    }

    /**
     * Builds [GeckoViewModelManagementOptions] from Android Components defined [ModelManagementOptions].
     *
     * This function translates the Android Components model management options into the
     * corresponding GeckoView representation.
     *
     * @param options The [ModelManagementOptions] to be converted.
     * @return The equivalent [GeckoViewModelManagementOptions].
     */
    internal fun buildGeckoModelManagementOptions(options: ModelManagementOptions): GeckoViewModelManagementOptions {
        val geckoOptionsBuilder = GeckoViewModelManagementOptions.Builder()
            .operation(options.operation.name)
            .operationLevel(options.operationLevel.name)

        options.languageToManage?.let { geckoOptionsBuilder.languageToManage(it) }

        return geckoOptionsBuilder.build()
    }

    /**
     * Maps a [GeckoResult] of a [String] representing a language setting to a
     * [GeckoResult] of [LanguageSetting].
     *
     * This function transforms the language setting from its string representation
     * (as returned by GeckoView) to the Android Components [LanguageSetting] enum.
     * It requires the input string to be non-null and a valid representation of a
     * [LanguageSetting].
     *
     * @param languageSetting The [GeckoResult] containing the string representation of the
     * language setting.
     * @return A [GeckoResult] containing the corresponding [LanguageSetting] if the
     *         transformation is successful. If the transformation fails due to invalid input,
     *         the returned [GeckoResult] will be completed exceptionally with an [IllegalArgumentException].
     */
    internal fun mapLanguageSetting(
        languageSetting: GeckoResult<String>,
    ): GeckoResult<LanguageSetting> {
        return languageSetting.map { settingString: String? ->
            requireNotNull(settingString) { "Language setting cannot be null" }
            LanguageSetting.fromValue(settingString)
        }
    }

    /**
     * Maps a [GeckoResult] of a [Map] with [String] keys and [String] values to a [GeckoResult] of a
     * [Map] with [String] keys and [LanguageSetting] values.
     *
     * This function transforms the language setting map from a string-based representation to an
     * enum-based representation. It handles potential parsing errors for individual language settings
     * and rethrows them with added context to indicate which language setting failed to parse.
     * If any single item fails to parse, the entire map transformation will fail.
     *
     * @param geckoResultWithStringMap The [GeckoResult] containing a [Map] of language codes
     * ([String]) to language setting strings ([String]) to be mapped.
     * @return A [GeckoResult] containing a [Map] of language codes ([String]) to [LanguageSetting] enum values.
     *         If the transformation encounters an [IllegalArgumentException]
     *         (e.g., if the input settings map or string is null, or if a language setting string cannot be parsed),
     *         the returned [GeckoResult] will be completed exceptionally with that [IllegalArgumentException].
     */
    internal fun mapToLanguageSettingMap(
        geckoResultWithStringMap: GeckoResult<Map<String, String>>,
    ): GeckoResult<Map<String, LanguageSetting>> {
        return geckoResultWithStringMap.map { settingsMap: Map<String, String>? ->
            requireNotNull(settingsMap) { "Language setting map cannot be null" }

            val resultMap = mutableMapOf<String, LanguageSetting>()
            settingsMap.forEach { (langCode, settingString) ->
                try {
                    resultMap[langCode] = LanguageSetting.fromValue(settingString)
                } catch (e: IllegalArgumentException) {
                    // If a single item fails to parse, rethrow to fail the whole map transformation.
                    // Add context to the exception.
                    throw IllegalArgumentException(
                        "Failed to parse language setting for language code '$langCode': " +
                            "Input string was '$settingString'. Reason: ${e.message}",
                        e,
                    )
                }
            }
            resultMap
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.translate

import org.jetbrains.annotations.VisibleForTesting

/**
 * The language model container for representing language model state to the user.
 *
 * Please note, a single LanguageModel is usually comprised of
 * an aggregation of multiple machine learning models on the translations engine level. The engine
 * has already handled this abstraction.
 *
 * @property language The specified language the language model set can process.
 * @property status The download status of the language models,
 * which can be not downloaded, download processing, or downloaded.
 * @property size The size of the total model download(s).
 */
data class LanguageModel(
    val language: Language? = null,
    val status: ModelState = ModelState.NOT_DOWNLOADED,
    val size: Long? = null,
) {
    companion object {
        /**
         * The BCP-47 language code that identifies the translations pivot language.
         *
         * A pivot language is used when there is no direct model to translate between two given
         * translation pairs. For example, de -> es is not a model in the translations engine.
         * To accomplish this translation, the models de -> en and en -> es will be used. English is
         * an intermediary or pivot for this translation.
         */
        const val PIVOT_LANGUAGE_CODE = "en"

        /**
         * Convenience method to determine if the [PIVOT_LANGUAGE_CODE] will need to begin syncing
         * on a given operation
         *
         * @param appLanguage The BCP-47 language code for the current app language.
         * @param languageModels The list and state of the language models.
         * @param options The operation that is requested to change the language model(s).
         * @return Whether the [PIVOT_LANGUAGE_CODE] language model should change state as well
         * based on the given information.
         */
        fun shouldPivotSync(
            appLanguage: String?,
            languageModels: List<LanguageModel>?,
            options: ModelManagementOptions,
        ): Boolean {
            return when {
                options.operationLevel != OperationLevel.LANGUAGE -> false

                options.operation != ModelOperation.DOWNLOAD -> false

                // This sync state will be managed like any other, no need to operate.
                options.languageToManage == PIVOT_LANGUAGE_CODE -> false

                // Downloads happen from the perspective of the app language, so if translating to the
                // pivot language, then there won't be a separate download.
                appLanguage == PIVOT_LANGUAGE_CODE -> false

                else -> !isPivotDownloaded(appLanguage = appLanguage, languageModels = languageModels)
            }
        }

        /**
         * Convenience method to determine if the [PIVOT_LANGUAGE_CODE] is downloaded or not.
         *
         * @param appLanguage The BCP-47 language code for the current app language.
         * @param languageModels The list and state of language models.
         * @return Will return true when the pivot language is listed as downloaded by the engine or
         * otherwise not needed. Will return false when downloaded and needed.
         */
        fun isPivotDownloaded(appLanguage: String?, languageModels: List<LanguageModel>?): Boolean {
            val models = languageModels?.associateBy { it.language?.code ?: "" }

            return when {
                models == null -> false

                // If the app language is the pivot language, then a pivot isn't needed.
                appLanguage == PIVOT_LANGUAGE_CODE -> true

                // Generally, if the engine isn't reporting the pivot language as something to download,
                // then it isn't needed. This can happen if the app language is not supported, then the
                // translation falls back to English.
                !models.containsKey(PIVOT_LANGUAGE_CODE) -> true

                else -> models[PIVOT_LANGUAGE_CODE]?.status == ModelState.DOWNLOADED
            }
        }

        /**
         * Convenience method to determine if any of the models are still processing.
         *
         * @param languageModels The list and state of language models.
         * @return Whether any of the models are currently syncing.
         */
        fun areModelsProcessing(languageModels: List<LanguageModel>?): Boolean {
            if (languageModels == null) {
                return false
            }

            return languageModels.any { model ->
                model.status == ModelState.DOWNLOAD_IN_PROGRESS || model.status == ModelState.DELETION_IN_PROGRESS
            }
        }

        /**
         * Convenience method to make the updated language model state based on an [ModelManagementOptions]
         * operation.
         *
         * @param appLanguage The BCP-47 language code for the current app language.
         * @param currentLanguageModels The current list and state of language models.
         * @param options The operation that is requested to change the language model(s).
         * @param newStatus What the new state should be based on the change.
         * @return The new state of the language models based on the information.
         */
        fun determineNewLanguageModelState(
            appLanguage: String?,
            currentLanguageModels: List<LanguageModel>?,
            options: ModelManagementOptions,
            newStatus: ModelState,
        ): List<LanguageModel>? =
            when (options.operationLevel) {
                OperationLevel.LANGUAGE -> {
                    // Set general model state
                    var updatedModels = currentLanguageModels?.map { model ->
                        if ((model.language?.code == options.languageToManage) &&
                            checkIfOperable(currentStatus = model.status, newStatus = newStatus)
                        ) {
                            model.copy(status = newStatus)
                        } else {
                            model.copy()
                        }
                    }

                    // If the pivot is not downloaded, it will be synced as well.
                    if (shouldPivotSync(
                            appLanguage = appLanguage,
                            languageModels = updatedModels,
                            options = options,
                        )
                    ) {
                        updatedModels = updatedModels?.map { model ->
                            if ((model.language?.code ?: "") == PIVOT_LANGUAGE_CODE) {
                                model.copy(status = newStatus)
                            } else {
                                model.copy()
                            }
                        }
                    }

                    updatedModels
                }

                OperationLevel.CACHE -> {
                    // Cache isn't tracked on the models here, only specific full language models
                    // are tracked, so no state change. This operation is clearing individual model
                    // files not a part of a complete language model package.
                    currentLanguageModels
                }

                OperationLevel.ALL -> {
                    currentLanguageModels?.map { model ->
                        if (checkIfOperable(currentStatus = model.status, newStatus = newStatus)) {
                            model.copy(status = newStatus)
                        } else {
                            model.copy()
                        }
                    }
                }
            }

        /**
         * Helper method to determine if changing from one proposed status to another is possible or
         * if it will result in no operation on the engine side.
         *
         * @param currentStatus The current status of the language model.
         * @param newStatus The proposed status the state should move to.
         * @return Will return true if the status change will result in a change. Will return false if the
         * engine is expected to have a no op operation.
         */
        @VisibleForTesting
        fun checkIfOperable(currentStatus: ModelState, newStatus: ModelState) = when (currentStatus) {
            ModelState.NOT_DOWNLOADED -> newStatus != ModelState.DELETION_IN_PROGRESS
            ModelState.DOWNLOAD_IN_PROGRESS -> true
            ModelState.DELETION_IN_PROGRESS -> true
            ModelState.DOWNLOADED -> newStatus != ModelState.DOWNLOAD_IN_PROGRESS
            ModelState.ERROR_DELETION -> true
            ModelState.ERROR_DOWNLOAD -> true
        }
    }
}

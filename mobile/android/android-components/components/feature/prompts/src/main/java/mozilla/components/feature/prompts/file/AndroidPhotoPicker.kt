/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.file

import android.content.Context
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.Fragment
import mozilla.components.feature.prompts.PromptFeature

/**
 * Provides functionality for picking photos from the device's gallery using native picker.
 *
 * @property context The application [Context].
 * @property singleMediaPicker An [ActivityResultLauncher] for picking a single photo.
 * @property multipleMediaPicker An [ActivityResultLauncher] for picking multiple photos.
 */
class AndroidPhotoPicker(
    val context: Context,
    val singleMediaPicker: ActivityResultLauncher<PickVisualMediaRequest>,
    val multipleMediaPicker: ActivityResultLauncher<PickVisualMediaRequest>,
) {
    internal val isPhotoPickerAvailable =
        ActivityResultContracts.PickVisualMedia.isPhotoPickerAvailable(context)

    companion object {
        /**
         * Registers a photo picker activity launcher in single-select mode.
         * Note that you must call singleMediaPicker before the fragment is created.
         *
         * @param getFragment A function that returns the [Fragment] which hosts the file picker.
         * @param getPromptsFeature A function that returns the [PromptFeature]
         * that handles the result of the photo picker.
         * @return An [ActivityResultLauncher] for picking a single photo.
         */
        fun singleMediaPicker(
            getFragment: () -> Fragment,
            getPromptsFeature: () -> PromptFeature?,
        ): ActivityResultLauncher<PickVisualMediaRequest> {
            return getFragment.invoke()
                .registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
                    uri?.let {
                        getPromptsFeature.invoke()?.onAndroidPhotoPickerResult(arrayOf(uri))
                    }
                }
        }

        /**
         * Registers a photo picker activity launcher in multi-select mode.
         * Note that you must call multipleMediaPicker before the fragment is created.
         *
         * @param getFragment A function that returns the [Fragment] which hosts the file picker.
         * @param getPromptsFeature A function that returns the [PromptFeature]
         * that handles the result of the photo picker.
         * @return An [ActivityResultLauncher] for picking multiple photos.
         */
        fun multipleMediaPicker(
            getFragment: () -> Fragment,
            getPromptsFeature: () -> PromptFeature?,
        ): ActivityResultLauncher<PickVisualMediaRequest> {
            return getFragment.invoke()
                .registerForActivityResult(ActivityResultContracts.PickMultipleVisualMedia()) { uriList ->
                    getPromptsFeature.invoke()?.onAndroidPhotoPickerResult(uriList.toTypedArray())
                }
        }
    }
}

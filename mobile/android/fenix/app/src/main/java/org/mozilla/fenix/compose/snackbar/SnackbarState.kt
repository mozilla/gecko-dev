/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.snackbar

import androidx.compose.material.SnackbarDuration
import com.google.android.material.snackbar.Snackbar.LENGTH_INDEFINITE
import com.google.android.material.snackbar.Snackbar.LENGTH_LONG
import com.google.android.material.snackbar.Snackbar.LENGTH_SHORT
import org.mozilla.fenix.compose.core.Action
import org.mozilla.fenix.compose.snackbar.SnackbarState.Type

private val defaultDuration = SnackbarState.Duration.Preset.Short
private val defaultType = Type.Default
private val defaultAction: Action? = null
private val defaultOnDismiss: () -> Unit = {}

/**
 * The data to display within a Snackbar.
 *
 * @property message The text to display within a Snackbar.
 * @property duration The duration of the Snackbar.
 * @property type The [Type] used to apply styling.
 * @property action Optional action within the Snackbar.
 * @property onDismiss Invoked when the Snackbar is dismissed.
 */
data class SnackbarState(
    val message: String,
    val duration: Duration = defaultDuration,
    val type: Type = defaultType,
    val action: Action? = defaultAction,
    val onDismiss: () -> Unit = defaultOnDismiss,
) {

    /**
     * A sealed type to represent a Snackbar's display duration.
     */
    sealed interface Duration {

        /**
         * A predefined display duration.
         */
        enum class Preset(val durationMs: Int) : Duration {
            Indefinite(durationMs = Int.MAX_VALUE),
            Long(durationMs = 10000),
            Short(durationMs = 4000),
        }

        /**
         * A custom display duration.
         *
         * @property durationMs The duration in milliseconds.
         */
        data class Custom(val durationMs: Int) : Duration
    }

    /**
     * Get the display duration of the Snackbar in milliseconds.
     */
    val durationMs: Int
        get() = when (duration) {
            is Duration.Preset -> duration.durationMs
            is Duration.Custom -> duration.durationMs
        }

    /**
     * Convert [SnackbarState.Duration] to [SnackbarDuration].
     */
    fun toSnackbarDuration(): SnackbarDuration {
        return when (duration) {
            Duration.Preset.Indefinite -> SnackbarDuration.Indefinite
            Duration.Preset.Long -> SnackbarDuration.Long
            Duration.Preset.Short -> SnackbarDuration.Short
            is Duration.Custom -> SnackbarDuration.Short
        }
    }

    /**
     * The type of Snackbar to display.
     */
    enum class Type {
        Default,
        Warning,
    }
}

/**
 * Helper function to convert a Material Integer constant to a [SnackbarState.Duration].
 */
fun Int.toSnackbarDuration(): SnackbarState.Duration = when (this) {
    LENGTH_SHORT -> SnackbarState.Duration.Preset.Short
    LENGTH_LONG -> SnackbarState.Duration.Preset.Long
    LENGTH_INDEFINITE -> SnackbarState.Duration.Preset.Indefinite
    else -> SnackbarState.Duration.Custom(durationMs = this)
}

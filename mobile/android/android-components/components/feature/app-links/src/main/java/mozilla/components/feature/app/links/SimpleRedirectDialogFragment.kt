/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import android.app.Dialog
import android.content.Context
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.CheckBox
import android.widget.FrameLayout
import androidx.annotation.StringRes
import androidx.annotation.StyleRes
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import mozilla.components.support.ktx.util.PromptAbuserDetector
import mozilla.components.ui.widgets.withCenterAlignedButtons

/**
 * This is the default implementation of the [RedirectDialogFragment].
 *
 * It provides an [AlertDialog] giving the user the choice to allow or deny the opening of a
 * third party app.
 *
 * Intents passed are guaranteed to be openable by a non-browser app.
 */
class SimpleRedirectDialogFragment(
    maxSuccessiveDialogMillisLimit: Int = TIME_SHOWN_OFFSET_MILLIS,
) : RedirectDialogFragment() {

    @VisibleForTesting
    internal var promptAbuserDetector =
        PromptAbuserDetector(maxSuccessiveDialogMillisLimit)

    @VisibleForTesting
    internal var testingContext: Context? = null

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        fun getBuilder(themeID: Int): AlertDialog.Builder {
            val context = testingContext ?: requireContext()
            return if (themeID == 0) AlertDialog.Builder(context) else AlertDialog.Builder(context, themeID)
        }

        promptAbuserDetector.updateJSDialogAbusedState()

        return with(requireBundle()) {
            val dialogTitleText = getInt(KEY_TITLE_TEXT, R.string.mozac_feature_applinks_normal_confirm_dialog_title)
            val dialogMessageString = getString(KEY_MESSAGE_STRING, "")
            val positiveButtonText = getInt(KEY_POSITIVE_TEXT, R.string.mozac_feature_applinks_confirm_dialog_confirm)
            val negativeButtonText = getInt(KEY_NEGATIVE_TEXT, R.string.mozac_feature_applinks_confirm_dialog_deny)
            val checkboxText = getInt(KEY_CHECKBOX_TEXT, R.string.mozac_feature_applinks_confirm_dialog_checkbox_label)
            val themeResId = getInt(KEY_THEME_ID, 0)
            val cancelable = getBoolean(KEY_CANCELABLE, false)
            val showCheckbox = getBoolean(KEY_CHECKBOX, false)

            val checkbox = CheckBox(requireContext()).apply {
                layoutParams = ViewGroup.MarginLayoutParams(
                    ViewGroup.MarginLayoutParams.WRAP_CONTENT,
                    ViewGroup.MarginLayoutParams.WRAP_CONTENT,
                )
                id = VIEW_ID
                setText(checkboxText)
            }

            val dialog = getBuilder(themeResId).apply {
                if (showCheckbox) {
                    setView(getLayout(checkbox))
                }
                setTitle(dialogTitleText)
                setMessage(dialogMessageString)
                setPositiveButton(positiveButtonText) { _, _ -> }
                setNegativeButton(negativeButtonText) { _, _ ->
                    onCancelRedirect()
                }
                setCancelable(cancelable)
            }.create()

            dialog.withCenterAlignedButtons()
            dialog.setOnShowListener {
                val okButton = dialog.getButton(AlertDialog.BUTTON_POSITIVE)
                okButton.setOnClickListener {
                    if (promptAbuserDetector.areDialogsBeingAbused()) {
                        promptAbuserDetector.updateJSDialogAbusedState()
                    } else {
                        onConfirmRedirect(showCheckbox && checkbox.isChecked)
                        dialog.dismiss()
                    }
                }
            }
            dialog
        }
    }

    private fun getLayout(view: View) = FrameLayout(requireContext()).apply {
        val leftPadding = resources.getDimension(R.dimen.mozac_feature_applinks_confirm_dialog_checkbox_margin).toInt()

        setPadding(leftPadding, 0, 0, 0)
        addView(view)
    }

    companion object {
        /**
         * A builder method for creating a [SimpleRedirectDialogFragment]
         */
        fun newInstance(
            @StringRes dialogTitleText: Int = R.string.mozac_feature_applinks_normal_confirm_dialog_title,
            dialogMessageString: String = "",
            @StringRes positiveButtonText: Int = R.string.mozac_feature_applinks_confirm_dialog_confirm,
            @StringRes negativeButtonText: Int = R.string.mozac_feature_applinks_confirm_dialog_deny,
            @StringRes checkboxText: Int? = null,
            @StyleRes themeResId: Int = 0,
            cancelable: Boolean = false,
            showCheckbox: Boolean = false,
            maxSuccessiveDialogMillisLimit: Int = TIME_SHOWN_OFFSET_MILLIS,
        ): RedirectDialogFragment {
            val fragment = SimpleRedirectDialogFragment(maxSuccessiveDialogMillisLimit)
            val arguments = fragment.arguments ?: Bundle()

            with(arguments) {
                putInt(KEY_TITLE_TEXT, dialogTitleText)

                putString(KEY_MESSAGE_STRING, dialogMessageString)

                putInt(KEY_POSITIVE_TEXT, positiveButtonText)

                putInt(KEY_NEGATIVE_TEXT, negativeButtonText)

                checkboxText?.let {
                    putInt(KEY_CHECKBOX_TEXT, it)
                }

                putInt(KEY_THEME_ID, themeResId)

                putBoolean(KEY_CANCELABLE, cancelable)

                putBoolean(KEY_CHECKBOX, showCheckbox)
            }

            fragment.arguments = arguments
            fragment.isCancelable = false

            return fragment
        }

        const val KEY_POSITIVE_TEXT = "KEY_POSITIVE_TEXT"

        const val KEY_NEGATIVE_TEXT = "KEY_NEGATIVE_TEXT"

        const val KEY_CHECKBOX_TEXT = "KEY_CHECKBOX_TEXT"

        const val KEY_TITLE_TEXT = "KEY_TITLE_TEXT"

        const val KEY_MESSAGE_STRING = "KEY_MESSAGE_STRING"

        const val KEY_THEME_ID = "KEY_THEME_ID"

        const val KEY_CANCELABLE = "KEY_CANCELABLE"

        private const val KEY_CHECKBOX = "KEY_CHECKBOX"

        private const val TIME_SHOWN_OFFSET_MILLIS = 1000

        internal const val VIEW_ID = 111
    }

    private fun requireBundle(): Bundle {
        return arguments ?: throw IllegalStateException("Fragment $this arguments is not set.")
    }
}

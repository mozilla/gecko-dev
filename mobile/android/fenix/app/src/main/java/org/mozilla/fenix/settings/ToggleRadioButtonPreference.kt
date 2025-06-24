/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.ImageView
import android.widget.RadioButton
import android.widget.TextView
import androidx.core.content.withStyledAttributes
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings

/**
 * A custom [Preference] that displays two mutually exclusive radio button options within a single
 * preference item. This preference stores a single [Boolean] value in [SharedPreferences] based on
 * which radio button is selected:
 *
 * @param context The [Context] this is associated with.
 * @param attrs Optional attribute set used to configure the preference.
 */
class ToggleRadioButtonPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : Preference(context, attrs) {

    private var sharedKey: String = ""
    private var trueOptionTitle: String? = null
    private var falseOptionTitle: String? = null
    private var trueOptionIconRes: Int = 0
    private var falseOptionIconRes: Int = 0

    init {
        layoutResource = R.layout.preference_widget_toggle_radio_button
        isSelectable = false

        context.withStyledAttributes(attrs, R.styleable.TwoOptionTogglePreference) {
            sharedKey = getString(R.styleable.TwoOptionTogglePreference_sharedPreferenceKey) ?: ""
            trueOptionTitle = getString(R.styleable.TwoOptionTogglePreference_trueOptionTitle)
            falseOptionTitle = getString(R.styleable.TwoOptionTogglePreference_falseOptionTitle)
            trueOptionIconRes = getResourceId(R.styleable.TwoOptionTogglePreference_trueOptionIconRes, 0)
            falseOptionIconRes = getResourceId(R.styleable.TwoOptionTogglePreference_falseOptionIconRes, 0)
        }
    }

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        val preferences = context.settings().preferences
        val selected = preferences.getBoolean(sharedKey, false)

        val optionTrueView = holder.findViewById(R.id.option_true)
        val optionFalseView = holder.findViewById(R.id.option_false)

        val optionTrueRadio = optionTrueView.findViewById<RadioButton>(R.id.radio_button)
        val optionFalseRadio = optionFalseView.findViewById<RadioButton>(R.id.radio_button)

        val optionTrueTitle = optionTrueView.findViewById<TextView>(R.id.title)
        val optionFalseTitle = optionFalseView.findViewById<TextView>(R.id.title)

        val optionTrueIconView = optionTrueView.findViewById<ImageView>(R.id.icon_view)
        val optionFalseIconView = optionFalseView.findViewById<ImageView>(R.id.icon_view)

        optionTrueTitle.text = trueOptionTitle
        optionFalseTitle.text = falseOptionTitle
        optionTrueIconView.setImageResource(trueOptionIconRes)
        optionFalseIconView.setImageResource(falseOptionIconRes)
        optionTrueIconView.isSelected = selected
        optionFalseIconView.isSelected = !selected
        optionTrueRadio.isChecked = selected
        optionFalseRadio.isChecked = !selected

        optionTrueView.setOnClickListener {
            optionTrueIconView.isSelected = true
            optionFalseIconView.isSelected = false
            preferences.edit().putBoolean(sharedKey, true).apply()
            notifyChanged()
        }

        optionFalseView.setOnClickListener {
            optionTrueIconView.isSelected = false
            optionFalseIconView.isSelected = true
            preferences.edit().putBoolean(sharedKey, false).apply()
            notifyChanged()
        }
    }
}

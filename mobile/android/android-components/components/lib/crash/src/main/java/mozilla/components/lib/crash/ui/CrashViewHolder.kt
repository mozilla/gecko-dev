/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.ui

import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.format.DateUtils
import android.text.method.LinkMovementMethod
import android.text.style.ClickableSpan
import android.view.View
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.crash.R

internal class CrashViewHolder(
    view: View,
    private val onShareCrashClicked: (DisplayableCrash) -> Unit,
    private val onCrashServiceSelected: (DisplayableCrash.Report) -> Unit,
) : RecyclerView.ViewHolder(view) {
    private val titleView = view.findViewById<TextView>(R.id.mozac_lib_crash_title)
    private val idView = view.findViewById<TextView>(R.id.mozac_lib_crash_id)
    private val footerView = view.findViewById<TextView>(R.id.mozac_lib_crash_footer).apply {
        movementMethod = LinkMovementMethod.getInstance()
    }

    fun bind(crash: DisplayableCrash) {
        idView.text = crash.uuid
        titleView.text = crash.stacktrace.lines().first()

        val time = DateUtils.getRelativeDateTimeString(
            footerView.context,
            crash.createdAt,
            DateUtils.MINUTE_IN_MILLIS,
            DateUtils.WEEK_IN_MILLIS,
            0,
        )

        footerView.text = SpannableStringBuilder(time).apply {
            append(" - ")
            append(itemView.context.getString(R.string.mozac_lib_crash_share)) {
                onShareCrashClicked(crash)
            }
            if (crash.reports.isNotEmpty()) {
                append(" - ")
                crash.reports.forEachIndexed { index, report ->
                    append(report.serviceName) { onCrashServiceSelected(report) }

                    if (index < crash.reports.lastIndex) {
                        append(" ")
                    }
                }
            }
        }
        ViewCompat.enableAccessibleClickableSpanSupport(footerView)
    }

    private fun SpannableStringBuilder.append(text: CharSequence, clickListener: () -> Unit) =
        append(
            text,
            object : ClickableSpan() {
                override fun onClick(widget: View) {
                    clickListener()
                }
            },
            Spanned.SPAN_EXCLUSIVE_EXCLUSIVE,
        )
}

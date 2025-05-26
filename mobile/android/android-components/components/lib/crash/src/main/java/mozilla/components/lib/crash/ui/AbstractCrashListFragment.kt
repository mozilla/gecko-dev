/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.ui

import android.content.Intent
import android.database.sqlite.SQLiteBlobTooBigException
import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.core.view.isGone
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.db.CrashDatabase

/**
 * Fragment displaying the list of crashes.
 */
abstract class AbstractCrashListFragment : Fragment(R.layout.mozac_lib_crash_crashlist) {
    private val database by lazy { CrashDatabase.get(requireContext()) }
    abstract val reporter: CrashReporter

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        requireActivity().setTitle(R.string.mozac_lib_crash_activity_title)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val listView: RecyclerView = view.findViewById(R.id.mozac_lib_crash_list)
        listView.layoutManager = LinearLayoutManager(
            requireContext(),
            LinearLayoutManager.VERTICAL,
            false,
        )

        val emptyView = view.findViewById<TextView>(R.id.mozac_lib_crash_empty)

        val adapter = CrashListAdapter(::onShareCrashClicked, ::onCrashServiceSelected)
        listView.adapter = adapter

        val dividerItemDecoration = DividerItemDecoration(
            requireContext(),
            LinearLayoutManager.VERTICAL,
        )
        listView.addItemDecoration(dividerItemDecoration)

        try {
            database.crashDao().getCrashesWithReports().observe(viewLifecycleOwner) { list ->
                adapter.submitList(list.map { it.toCrash(reporter) })
                emptyView.isGone = list.isNotEmpty()
            }
        } catch (e: SQLiteBlobTooBigException) {
            // recover by deleting all entries
            database.crashDao().deleteAll()
        }
    }

    /**
     * Gets invoked whenever the user selects a crash reporting service.
     *
     * @param url URL pointing to the crash report for the selected crash reporting service.
     */
    abstract fun onCrashServiceSelected(url: String)

    private fun onCrashServiceSelected(entity: DisplayableCrash.Report) {
        entity.url?.let { onCrashServiceSelected(it) }
    }

    private fun onShareCrashClicked(crash: DisplayableCrash) {
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "text/plain"
            putExtra(Intent.EXTRA_TEXT, crash.toString())
        }
        requireActivity().startActivity(Intent.createChooser(intent, "DisplayableCrash"))
    }
}

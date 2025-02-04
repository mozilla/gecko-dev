/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.autofill.ui.search

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import mozilla.components.concept.storage.Login
import mozilla.components.feature.autofill.R

/**
 * Adapter for showing a list of logins.
 */
internal class LoginsAdapter(
    private val onLoginSelected: (Login) -> Unit,
) : ListAdapter<Login, LoginViewHolder>(LoginsDiffCallback) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): LoginViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        val view = inflater.inflate(R.layout.mozac_feature_autofill_login, parent, false)
        return LoginViewHolder(view, onLoginSelected)
    }

    override fun onBindViewHolder(holder: LoginViewHolder, position: Int) {
        val login = getItem(position)
        holder.bind(login)
    }

    internal object LoginsDiffCallback : DiffUtil.ItemCallback<Login>() {
        override fun areItemsTheSame(oldItem: Login, newItem: Login) =
            oldItem.guid == newItem.guid

        override fun areContentsTheSame(oldItem: Login, newItem: Login) =
            oldItem.username == newItem.username && oldItem.password == newItem.password &&
                oldItem.origin == newItem.origin
    }
}

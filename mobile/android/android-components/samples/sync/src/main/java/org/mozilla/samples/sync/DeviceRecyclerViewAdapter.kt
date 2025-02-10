/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.sync

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceType
import org.mozilla.samples.sync.DeviceFragment.OnDeviceListInteractionListener
import org.mozilla.samples.sync.databinding.FragmentDeviceBinding

/**
 * [RecyclerView.Adapter] that can display a [DummyItem] and makes a call to the
 * specified [OnDeviceListInteractionListener].
 */
class DeviceRecyclerViewAdapter(
    var onDeviceClickedListener: OnDeviceListInteractionListener? = null,
) : ListAdapter<Device, DeviceRecyclerViewAdapter.ViewHolder>(DeviceDiffCallback) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = FragmentDeviceBinding
            .inflate(LayoutInflater.from(parent.context), parent, false)
        return ViewHolder(binding)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val item = getItem(position)
        holder.nameView.text = item.displayName
        holder.typeView.text = when (item.deviceType) {
            DeviceType.DESKTOP -> "Desktop"
            DeviceType.MOBILE -> "Mobile"
            DeviceType.TABLET -> "Tablet"
            DeviceType.TV -> "TV"
            DeviceType.VR -> "VR"
            DeviceType.UNKNOWN -> "Unknown"
        }

        holder.itemView.setOnClickListener {
            onDeviceClickedListener?.onDeviceInteraction(item)
        }
    }

    inner class ViewHolder(binding: FragmentDeviceBinding) : RecyclerView.ViewHolder(binding.root) {
        val nameView: TextView = binding.deviceName
        val typeView: TextView = binding.deviceType
    }

    private object DeviceDiffCallback : DiffUtil.ItemCallback<Device>() {
        override fun areItemsTheSame(oldItem: Device, newItem: Device) =
            oldItem.id == newItem.id

        override fun areContentsTheSame(oldItem: Device, newItem: Device) =
            oldItem == newItem
    }
}

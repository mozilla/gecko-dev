/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.shortcut

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.widget.addTextChangedListener
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.selectedTab
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentCreateXiaomiShortcutBinding
import org.mozilla.fenix.ext.loadIntoView
import org.mozilla.fenix.ext.requireComponents

/**
 * A [DialogFragment] for creating a Xiaomi-specific shortcut on the home screen.
 *
 * This fragment allows users to create a shortcut for the current tab on Xiaomi devices,
 * with an option to customize the shortcut name. The fragment handles the layout and interaction
 * for inputting the shortcut name, validating input, and invoking the necessary use case to
 * add the shortcut to the home screen.
 */
class CreateXiaomiShortcutFragment : DialogFragment() {
    private var _binding: FragmentCreateXiaomiShortcutBinding? = null
    private val binding get() = _binding!!

    /**
     * Sets the dialog style for the shortcut creation screen.
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, R.style.CreateShortcutDialogStyle)
    }

    /**
     * Inflates the layout for the fragment.
     *
     * @param inflater The [LayoutInflater] object that can be used to inflate views in the fragment.
     * @param container The parent view that this fragment's UI should be attached to.
     * @param savedInstanceState If non-null, this fragment is being re-constructed from a previous saved state.
     * @return The root view for the fragment's UI.
     */
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        _binding = FragmentCreateXiaomiShortcutBinding.inflate(inflater, container, false)

        return binding.root
    }

    /**
     * Called immediately after the view is created. Sets up the UI and binds events.
     *
     * @param view The view returned by [onCreateView].
     * @param savedInstanceState If non-null, this fragment is being re-constructed from a previous saved state.
     */
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val tab = requireComponents.core.store.state.selectedTab

        if (tab == null) {
            dismiss()
        } else {
            requireComponents.core.icons.loadIntoView(binding.faviconImage, tab.content.url)

            binding.cancelButton.setOnClickListener { dismiss() }
            binding.addButton.setOnClickListener {
                val text = binding.shortcutText.text.toString().trim()
                requireActivity().lifecycleScope.launch {
                    requireComponents.useCases.webAppUseCases.addToHomescreen(text)
                }
                dismiss()
            }

            binding.shortcutText.addTextChangedListener {
                updateAddButtonEnabledState()
            }

            binding.shortcutText.setText(tab.content.title)
        }
    }

    /**
     * Cleans up the view binding when the fragment's view is destroyed.
     */
    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    /**
     * Updates the state of the "Add" button based on the shortcut text.
     */
    private fun updateAddButtonEnabledState() {
        val text = binding.shortcutText.text
        binding.addButton.isEnabled = text.isNotBlank()
        binding.addButton.alpha = if (text.isNotBlank()) ENABLED_ALPHA else DISABLED_ALPHA
    }

    companion object {
        private const val ENABLED_ALPHA = 1.0f
        private const val DISABLED_ALPHA = 0.4f
    }
}

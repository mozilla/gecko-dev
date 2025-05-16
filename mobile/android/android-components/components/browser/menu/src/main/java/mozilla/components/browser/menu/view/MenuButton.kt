/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.view

import android.content.Context
import android.content.res.ColorStateList
import android.util.AttributeSet
import android.view.View
import android.widget.FrameLayout
import android.widget.ImageView
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import androidx.lifecycle.findViewTreeLifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.FlowPreview
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.debounce
import kotlinx.coroutines.flow.flowOn
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenu.Orientation
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.R
import mozilla.components.browser.menu.ext.getHighlight
import mozilla.components.concept.menu.MenuButton
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.concept.menu.candidate.MenuEffect
import mozilla.components.concept.menu.ext.effects
import mozilla.components.concept.menu.ext.max
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.ktx.android.view.hideKeyboard

/**
 * A `three-dot` button used for expanding menus.
 *
 * If you are using a browser toolbar, do not use this class directly.
 */
@OptIn(FlowPreview::class)
class MenuButton @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : FrameLayout(context, attrs, defStyleAttr),
    MenuButton,
    View.OnClickListener,
    Observable<MenuButton.Observer> by ObserverRegistry() {

    /**
     * Trigger [kotlinx.coroutines.flow.Flow] to help debounce the [getHighlight] & [setHighlight]
     * calls when [setHighlightStatus] has been called.
     */
    private val highlightStatusTrigger = MutableSharedFlow<Unit>(extraBufferCapacity = 1)

    /**
     * Flag to let us know if we already started observing the [highlightStatusTrigger]. This is to
     * avoid observing the flow multiple times.
     */
    private var isObservingHighlightStatusTrigger = false

    private val menuControllerObserver = object : MenuController.Observer {
        /**
         * Change the menu button appearance when the menu list changes.
         */
        override fun onMenuListSubmit(list: List<MenuCandidate>) {
            val effect = list.effects().max()

            // If a highlighted item is found, show the indicator
            setEffect(effect)
        }

        override fun onDismiss() = notifyObservers { onDismiss() }
    }

    /**
     * Listener called when the menu is shown.
     */
    @Deprecated("Use the Observable interface to listen for onShow")
    var onShow: () -> Unit = {}

    /**
     * Listener called when the menu is dismissed.
     */
    @Deprecated("Use the Observable interface to listen for onDismiss")
    var onDismiss: () -> Unit = {}

    /**
     * Callback to get the orientation for the menu.
     * This is called every time the menu should be displayed.
     * This has no effect when a [MenuController] is set.
     */
    var getOrientation: () -> Orientation = {
        BrowserMenu.determineMenuOrientation(parent as? View?)
    }

    /**
     * [CoroutineDispatcher] to use for background tasks. Default value is [Dispatchers.Default].
     *
     * This is captured in a variable for overriding in tests.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var backgroundTaskDispatcher: CoroutineDispatcher = Dispatchers.Default

    /**
     * [CoroutineScope] to use for background tasks.
     *
     * In production code, it uses [findViewTreeLifecycleOwner]'s
     * lifecycle scope, and it is set in [onAttachedToWindow], when the view has been attached.
     *
     * This is captured in a variable for overriding in tests.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var coroutineScope: CoroutineScope? = findViewTreeLifecycleOwner()?.lifecycleScope

    /**
     * Sets a [MenuController] that will be used to create a menu when this button is clicked.
     * If present, [menuBuilder] will be ignored.
     */
    override var menuController: MenuController? = null
        set(value) {
            // Clean up old controller
            field?.dismiss()
            field?.unregister(menuControllerObserver)

            // Attach new controller
            field = value
            value?.register(menuControllerObserver, this)
        }

    /**
     * Sets a [BrowserMenuBuilder] that will be used to create a menu when this button is clicked.
     */
    var menuBuilder: BrowserMenuBuilder? = null
        set(value) {
            field = value
            menu?.dismiss()
            if (value == null) menu = null
        }

    var recordClickEvent: () -> Unit = {}

    @VisibleForTesting internal var menu: BrowserMenu? = null

    private val menuIcon: ImageView
    private val highlightView: ImageView
    private val notificationIconView: ImageView

    init {
        View.inflate(context, R.layout.mozac_browser_menu_button, this)
        setOnClickListener(this)
        menuIcon = findViewById(R.id.icon)
        highlightView = findViewById(R.id.highlight)
        notificationIconView = findViewById(R.id.notification_dot)

        // Hook up deprecated callbacks using new observer system
        @Suppress("Deprecation")
        val internalObserver = object : MenuButton.Observer {
            override fun onShow() = this@MenuButton.onShow()
            override fun onDismiss() = this@MenuButton.onDismiss()
        }
        register(internalObserver)
    }

    /**
     * Shows the menu, or dismisses it if already open.
     */
    override fun onClick(v: View) {
        this.hideKeyboard()
        recordClickEvent()

        // If a legacy menu is open, dismiss it.
        if (menu != null) {
            menu?.dismiss()
            return
        }

        val menuController = menuController
        if (menuController != null) {
            // Use the newer menu controller if set
            menuController.show(anchor = this)
        } else {
            menu = menuBuilder?.build(context)
            val endAlwaysVisible = menuBuilder?.endOfMenuAlwaysVisible ?: false
            menu?.show(
                anchor = this,
                orientation = getOrientation(),
                endOfMenuAlwaysVisible = endAlwaysVisible,
            ) {
                menu = null
                notifyObservers { onDismiss() }
            }
        }
        notifyObservers { onShow() }
    }

    /**
     * Show the indicator for a browser menu highlight.
     */
    fun setHighlight(highlight: BrowserMenuHighlight?) =
        setEffect(highlight?.asEffect(context))

    /**
     * Show the indicator for a browser menu effect.
     */
    override fun setEffect(effect: MenuEffect?) {
        when (effect) {
            is HighPriorityHighlightEffect -> {
                highlightView.imageTintList = ColorStateList.valueOf(effect.backgroundTint)
                highlightView.visibility = View.VISIBLE
                notificationIconView.visibility = View.GONE
            }
            is LowPriorityHighlightEffect -> {
                notificationIconView.setColorFilter(effect.notificationTint)
                highlightView.visibility = View.GONE
                notificationIconView.visibility = View.VISIBLE
            }
            null -> {
                highlightView.visibility = View.GONE
                notificationIconView.visibility = View.GONE
            }
        }
    }

    /**
     * Sets the tint of the 3-dot menu icon.
     */
    override fun setColorFilter(@ColorInt color: Int) {
        menuIcon.setColorFilter(color)
    }

    /**
     * Dismiss the menu, if open.
     */
    fun dismissMenu() {
        menuController?.dismiss()
        menu?.dismiss()
    }

    /**
     * Invalidates the [BrowserMenu], if open.
     */
    fun invalidateBrowserMenu() {
        menu?.invalidate()
    }

    /**
     * Check the current [BrowserMenuBuilder], if exists, for highlight effect
     * and apply it.
     */
    fun setHighlightStatus() {
        if (menuBuilder != null) {
            observeAndDebounceSetHighlightStatusRequests()
            highlightStatusTrigger.tryEmit(Unit)
        }
    }

    /**
     * This function helps to listen to and debounce requests to set highlight status using
     * [highlightStatusTrigger].
     *
     * The reason we are debouncing the [setHighlightStatus] calls is because we call [setHighlightStatus]
     * multiple times in quick successions especially during app startups.
     *
     * See [https://bugzilla.mozilla.org/show_bug.cgi?id=1947534](https://bugzilla.mozilla.org/show_bug.cgi?id=1947534)
     * for more.
     */
    private fun observeAndDebounceSetHighlightStatusRequests() {
        if (isObservingHighlightStatusTrigger) return
        isObservingHighlightStatusTrigger = true

        val scope = coroutineScope ?: findViewTreeLifecycleOwner()?.lifecycleScope
        scope?.launch {
            highlightStatusTrigger
                .debounce(HIGHLIGHT_STATUS_DEBOUNCE_MS)
                .map { menuBuilder?.items?.getHighlight() }
                .flowOn(backgroundTaskDispatcher)
                .collectLatest { highlights ->
                    withContext(Dispatchers.Main) {
                        setHighlight(highlights)
                    }
                }
        }
    }

    /**
     * This function should be used only in tests.
     *
     * Returns true when there is a highlight on this menu button. Uses [highlightView]'s
     * or [notificationIconView]'s visibility to infer whether or not there's a highlight.
     */
    @VisibleForTesting
    internal fun hasHighlight(): Boolean {
        return highlightView.isVisible || notificationIconView.isVisible
    }

    internal companion object {
        const val HIGHLIGHT_STATUS_DEBOUNCE_MS = 500L
    }
}

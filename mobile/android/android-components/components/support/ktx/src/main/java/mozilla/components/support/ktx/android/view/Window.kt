/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.os.Build.VERSION.SDK_INT
import android.os.Build.VERSION_CODES
import android.view.View
import android.view.Window
import android.view.WindowManager
import androidx.annotation.ColorInt
import androidx.core.graphics.Insets
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat.Type.displayCutout
import androidx.core.view.WindowInsetsCompat.Type.systemBars
import androidx.core.view.WindowInsetsControllerCompat
import mozilla.components.support.utils.ColorUtils.isDark

/**
 * Sets the status bar background color. If the color is light enough, a light navigation bar with
 * dark icons will be used.
 */
fun Window.setStatusBarTheme(@ColorInt color: Int) {
    createWindowInsetsController().isAppearanceLightStatusBars = !isDark(color)
    setStatusBarColorCompat(color)
}

/**
 * Set the navigation bar background and divider colors. If the color is light enough, a light
 * navigation bar with dark icons will be used.
 */
fun Window.setNavigationBarTheme(
    @ColorInt navBarColor: Int? = null,
    @ColorInt navBarDividerColor: Int? = null,
) {
    navBarColor?.let {
        setNavigationBarColorCompat(it)
        createWindowInsetsController().isAppearanceLightNavigationBars = !isDark(it)
    }
    setNavigationBarDividerColorCompat(navBarDividerColor)
}

/**
 * Creates a {@link WindowInsetsControllerCompat} for the top-level window decor view.
 */
fun Window.createWindowInsetsController(): WindowInsetsControllerCompat {
    return WindowInsetsControllerCompat(this, this.decorView)
}

/**
 * Sets the status bar color.
 *
 * @param color The color to set as the status bar color.
 * Note that If the app targets VANILLA_ICE_CREAM or above,
 * the color will be transparent and cannot be changed.
 */
fun Window.setStatusBarColorCompat(@ColorInt color: Int) {
    if (context.applicationInfo.targetSdkVersion < VERSION_CODES.VANILLA_ICE_CREAM) {
        @Suppress("DEPRECATION")
        statusBarColor = color
    }
}

/**
 * Sets the navigation bar color.
 *
 * @param color The color to set as the navigation bar color.
 * Note that If the app targets VANILLA_ICE_CREAM or above,
 * the color will be transparent and cannot be changed.
 */
fun Window.setNavigationBarColorCompat(@ColorInt color: Int) {
    if (context.applicationInfo.targetSdkVersion < VERSION_CODES.VANILLA_ICE_CREAM) {
        @Suppress("DEPRECATION")
        navigationBarColor = color
    }
}

/**
 * Sets the navigation bar divider color.
 *
 * @param color The color to set as the navigation bar divider color.
 * Note that If the app targets VANILLA_ICE_CREAM or above,
 * the color will be transparent and cannot be changed.
 */
fun Window.setNavigationBarDividerColorCompat(@ColorInt color: Int?) {
    if (SDK_INT >= VERSION_CODES.P &&
        context.applicationInfo.targetSdkVersion < VERSION_CODES.VANILLA_ICE_CREAM
    ) {
        @Suppress("DEPRECATION")
        navigationBarDividerColor = color ?: 0
    }
}

/**
 * Setup handling persistent insets - system bars and display cutouts ourselves instead of the framework.
 * This results in keeping the same behavior for such insets while allowing to separately control the behavior
 * for other dynamic insets.
 *
 * This only works on Android Q and above. On older versions calling this will result in no-op.
 */
fun Window.setupPersistentInsets() {
    if (SDK_INT >= VERSION_CODES.Q) {
        WindowCompat.setDecorFitsSystemWindows(this, false)

        val rootView = decorView.findViewById<View>(android.R.id.content)
        val persistentInsetsTypes = systemBars() or displayCutout()

        ViewCompat.setOnApplyWindowInsetsListener(rootView) { _, windowInsets ->
            val isInImmersiveMode = attributes.flags and WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS != 0
            val persistentInsets = when (isInImmersiveMode) {
                true -> {
                    // If we are in immersive mode we need to reset current paddings and avoid setting others.
                    Insets.of(0, 0, 0, 0)
                }
                false -> windowInsets.getInsets(persistentInsetsTypes)
            }

            rootView.setPadding(
                persistentInsets.left,
                persistentInsets.top,
                persistentInsets.right,
                persistentInsets.bottom,
            )

            // Pass window insets further to allow below listeners also know when there is a change.
            return@setOnApplyWindowInsetsListener windowInsets
        }
    }
}

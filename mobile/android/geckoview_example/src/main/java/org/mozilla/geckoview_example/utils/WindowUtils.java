/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example.utils;

import android.os.Build;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;

/** Collection of helper methods specific to the Android {@link Window}. */
public class WindowUtils {

  /**
   * Setup handling persistent insets - system bars and display cutouts ourselves instead of the
   * framework. This results in keeping the same behavior for such insets while allowing to
   * separately control the behavior for other dynamic insets.
   *
   * <p>This only works on Android Q and above. On older versions calling this will result in no-op.
   */
  public static void setupPersistentInsets(Window window) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      WindowCompat.setDecorFitsSystemWindows(window, false);

      View rootView = window.getDecorView().findViewById(android.R.id.content);
      int persistentInsetsTypes =
          WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout();

      ViewCompat.setOnApplyWindowInsetsListener(
          rootView,
          (v, windowInsets) -> {
            boolean isInImmersiveMode =
                (window.getAttributes().flags & WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)
                    != 0;
            Insets persistentInsets =
                isInImmersiveMode
                    ? Insets.of(0, 0, 0, 0)
                    : windowInsets.getInsets(persistentInsetsTypes);

            rootView.setPadding(
                persistentInsets.left,
                persistentInsets.top,
                persistentInsets.right,
                persistentInsets.bottom);

            // Pass window insets further to allow below listeners also know when there is a change.
            return windowInsets;
          });
    }
  }

  /**
   * Allow the window to be resized to fit the keyboard.
   *
   * <p>This only works on Android P and below. On newer versions calling this will result in no-op.
   */
  @SuppressWarnings("DEPRECATION")
  public static void setupImeBehavior(Window window) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
      window.setSoftInputMode(
          WindowManager.LayoutParams.SOFT_INPUT_STATE_UNSPECIFIED
              | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
    }
  }
}

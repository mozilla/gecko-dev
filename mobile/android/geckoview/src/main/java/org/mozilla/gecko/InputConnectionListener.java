/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.Handler;
import android.view.KeyEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * Interface for interacting with GeckoInputConnection from GeckoView.
 */
interface InputConnectionListener
{
    Handler getHandler(Handler defHandler);
    InputConnection onCreateInputConnection(EditorInfo outAttrs);
    boolean onKeyPreIme(int keyCode, KeyEvent event);
    boolean onKeyDown(int keyCode, KeyEvent event);
    boolean onKeyLongPress(int keyCode, KeyEvent event);
    boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event);
    boolean onKeyUp(int keyCode, KeyEvent event);
    boolean isIMEEnabled();
}

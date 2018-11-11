/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.support.annotation.UiThread;
import android.view.Surface;

import org.mozilla.gecko.util.ThreadUtils;

/**
 * Applications use a GeckoDisplay instance to provide {@link GeckoSession} with a {@link Surface} for
 * displaying content. To ensure drawing only happens on a valid {@link Surface}, {@link GeckoSession}
 * will only use the provided {@link Surface} after {@link #surfaceChanged(Surface, int, int)} is
 * called and before {@link #surfaceDestroyed()} returns.
 */
public class GeckoDisplay {
    private final GeckoSession session;

    protected GeckoDisplay(final GeckoSession session) {
        this.session = session;
    }

    /**
     * Required callback. The display's Surface has been created or changed. Must be
     * called on the application main thread. GeckoSession may block this call to ensure
     * the Surface is valid while resuming drawing.
     *
     * @param surface The new Surface.
     * @param width New width of the Surface.
     * @param height New height of the Surface.
     */
    @UiThread
    public void surfaceChanged(Surface surface, int width, int height) {
        ThreadUtils.assertOnUiThread();

        if (session.getDisplay() == this) {
            session.onSurfaceChanged(surface, width, height);
        }
    }

    /**
     * Required callback. The display's Surface has been destroyed. Must be called on the
     * application main thread. GeckoSession may block this call to ensure the Surface is
     * valid while pausing drawing.
     */
    @UiThread
    public void surfaceDestroyed() {
        ThreadUtils.assertOnUiThread();

        if (session.getDisplay() == this) {
            session.onSurfaceDestroyed();
        }
    }

    /**
     * Optional callback. The display's coordinates on the screen has changed. Must be
     * called on the application main thread.
     *
     * @param left The X coordinate of the display on the screen, in screen pixels.
     * @param top The Y coordinate of the display on the screen, in screen pixels.
     */
    @UiThread
    public void screenOriginChanged(final int left, final int top) {
        ThreadUtils.assertOnUiThread();

        if (session.getDisplay() == this) {
            session.onScreenOriginChanged(left, top);
        }
    }

    /**
     * Return whether the display should be pinned on the screen. When pinned, the display
     * should not be moved on the screen due to animation, scrolling, etc. A common reason
     * for the display being pinned is when the user is dragging a selection caret inside
     * the display; normal user interaction would be disrupted in that case if the display
     * was moved on screen.
     *
     * @return True if display should be pinned on the screen.
     */
    @UiThread
    public boolean shouldPinOnScreen() {
        ThreadUtils.assertOnUiThread();
        return session.getDisplay() == this && session.shouldPinOnScreen();
    }
}

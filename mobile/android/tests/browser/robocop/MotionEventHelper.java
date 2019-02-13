/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tests;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.gfx.LayerView;

import android.app.Instrumentation;
import android.os.SystemClock;
import android.util.Log;
import android.view.MotionEvent;

class MotionEventHelper {
    private static final String LOGTAG = "RobocopMotionEventHelper";

    private static final long DRAG_EVENTS_PER_SECOND = 20; // 20 move events per second when doing a drag

    private final Instrumentation mInstrumentation;
    private final int mSurfaceOffsetX;
    private final int mSurfaceOffsetY;
    private final LayerView layerView;

    public MotionEventHelper(Instrumentation inst, int surfaceOffsetX, int surfaceOffsetY) {
        mInstrumentation = inst;
        mSurfaceOffsetX = surfaceOffsetX;
        mSurfaceOffsetY = surfaceOffsetY;
        layerView = GeckoAppShell.getLayerView();
        Log.i(LOGTAG, "Initialized using offset (" + mSurfaceOffsetX + "," + mSurfaceOffsetY + ")");
    }

    public long down(float x, float y) {
        Log.d(LOGTAG, "Triggering down at (" + x + "," + y + ")");
        long downTime = SystemClock.uptimeMillis();
        MotionEvent event = MotionEvent.obtain(downTime, downTime, MotionEvent.ACTION_DOWN, mSurfaceOffsetX + x, mSurfaceOffsetY + y, 0);
        try {
            mInstrumentation.sendPointerSync(event);
        } finally {
            event.recycle();
            event = null;
        }
        return downTime;
    }

    public long move(long downTime, float x, float y) {
        return move(downTime, SystemClock.uptimeMillis(), x, y);
    }

    public long move(long downTime, long moveTime, float x, float y) {
        Log.d(LOGTAG, "Triggering move to (" + x + "," + y + ")");
        MotionEvent event = MotionEvent.obtain(downTime, moveTime, MotionEvent.ACTION_MOVE, mSurfaceOffsetX + x, mSurfaceOffsetY + y, 0);
        try {
            mInstrumentation.sendPointerSync(event);
        } finally {
            event.recycle();
            event = null;
        }
        return downTime;
    }

    public long up(long downTime, float x, float y) {
        return up(downTime, SystemClock.uptimeMillis(), x, y);
    }

    public long up(long downTime, long upTime, float x, float y) {
        Log.d(LOGTAG, "Triggering up at (" + x + "," + y + ")");
        MotionEvent event = MotionEvent.obtain(downTime, upTime, MotionEvent.ACTION_UP, mSurfaceOffsetX + x, mSurfaceOffsetY + y, 0);
        try {
            mInstrumentation.sendPointerSync(event);
        } finally {
            event.recycle();
            event = null;
        }
        return -1L;
    }

    public Thread dragAsync(final float startX, final float startY, final float endX, final float endY, final long durationMillis) {
        Thread t = new Thread() {
            @Override
            public void run() {
                layerView.setIsLongpressEnabled(false);

                int numEvents = (int)(durationMillis * DRAG_EVENTS_PER_SECOND / 1000);
                float eventDx = (endX - startX) / numEvents;
                float eventDy = (endY - startY) / numEvents;
                long downTime = down(startX, startY);
                for (int i = 0; i < numEvents - 1; i++) {
                    downTime = move(downTime, startX + (eventDx * i), startY + (eventDy * i));
                    try {
                        Thread.sleep(1000L / DRAG_EVENTS_PER_SECOND);
                    } catch (InterruptedException ie) {
                        ie.printStackTrace();
                    }
                }
                // sleep a bit before sending the last move so that the calculated
                // fling velocity is low and we don't end up doing a fling afterwards.
                try {
                    Thread.sleep(1000L);
                } catch (InterruptedException ie) {
                    ie.printStackTrace();
                }
                // do the last one using endX/endY directly to avoid rounding errors
                downTime = move(downTime, endX, endY);
                downTime = up(downTime, endX, endY);

                layerView.setIsLongpressEnabled(true);
            }
        };
        t.start();
        return t;
    }

    public void dragSync(float startX, float startY, float endX, float endY, long durationMillis) {
        try {
            dragAsync(startX, startY, endX, endY, durationMillis).join();
            mInstrumentation.waitForIdleSync();
        } catch (InterruptedException ie) {
            ie.printStackTrace();
        }
    }

    public void dragSync(float startX, float startY, float endX, float endY) {
        dragSync(startX, startY, endX, endY, 1000);
    }

    public Thread flingAsync(final float startX, final float startY, final float endX, final float endY, final float velocity) {
        // note that the first move after the touch-down is used to get over the panning threshold, and
        // is basically cancelled out. this means we need to generate (at least) two move events, with
        // the last move event hitting the target velocity. to do this we just slice the total distance
        // in half, assuming the first half will get us over the panning threshold and the second half
        // will trigger the fling.
        final float dx = (endX - startX) / 2;
        final float dy = (endY - startY) / 2;
        float distance = (float) Math.sqrt((dx * dx) + (dy * dy));
        final long time = (long)(distance / velocity);
        if (time <= 0) {
            throw new IllegalArgumentException( "Fling parameters require too small a time period" );
        }
        Thread t = new Thread() {
            @Override
            public void run() {
                long downTime = down(startX, startY);
                downTime = move(downTime, downTime + time, startX + dx, startY + dy);
                downTime = move(downTime, downTime + time + time, endX, endY);
                downTime = up(downTime, downTime + time, endX, endY);
            }
        };
        t.start();
        return t;
    }

    public void flingSync(float startX, float startY, float endX, float endY, float velocity) {
        try {
            flingAsync(startX, startY, endX, endY, velocity).join();
            mInstrumentation.waitForIdleSync();
        } catch (InterruptedException ie) {
            ie.printStackTrace();
        }
    }

    public void tap(float x, float y) {
        long downTime = down(x, y);
        downTime = up(downTime, x, y);
    }

    public void doubleTap(float x, float y) {
        tap(x, y);
        tap(x, y);
    }
}

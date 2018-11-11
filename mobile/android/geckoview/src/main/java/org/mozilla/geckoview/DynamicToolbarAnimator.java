/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import org.mozilla.gecko.util.ThreadUtils;

import android.graphics.Bitmap;
import android.util.Log;

import java.util.EnumSet;
import java.util.Set;

public final class DynamicToolbarAnimator {
    private static final String LOGTAG = "GeckoDynamicToolbarAnimator";

    public static enum PinReason {
        DISABLED(0),
        RELAYOUT(1),
        ACTION_MODE(2),
        FULL_SCREEN(3),
        CARET_DRAG(4),
        PAGE_LOADING(5),
        CUSTOM_TAB(6);

        public final int value;

        PinReason(final int aValue) {
            value = aValue;
        }
    }

    public interface ToolbarChromeProxy {
        public Bitmap getBitmapOfToolbarChrome();
        public boolean isToolbarChromeVisible();
        public void toggleToolbarChrome(boolean aShow);
    }

    private final Set<PinReason> mPinFlags = EnumSet.noneOf(PinReason.class);

    private final GeckoSession mTarget;
    private final GeckoSession.Compositor mCompositor;
    private ToolbarChromeProxy mToolbarChromeProxy;
    private int mMaxToolbarHeight;

    /* package */ DynamicToolbarAnimator(final GeckoSession aTarget) {
        mTarget = aTarget;
        mCompositor = aTarget.mCompositor;
    }

    public ToolbarChromeProxy getToolbarChromeProxy() {
        ThreadUtils.assertOnUiThread();
        return mToolbarChromeProxy;
    }

    public void setToolbarChromeProxy(ToolbarChromeProxy aToolbarChromeProxy) {
        ThreadUtils.assertOnUiThread();
        mToolbarChromeProxy = aToolbarChromeProxy;
    }

    public void setMaxToolbarHeight(int maxToolbarHeight) {
        ThreadUtils.assertOnUiThread();

        mMaxToolbarHeight = maxToolbarHeight;
        if (mCompositor.isReady()) {
            mCompositor.setMaxToolbarHeight(mMaxToolbarHeight);
        }
    }

    // Keep this package-private because applications should use one of GeckoSession's
    // coordinates APIs instead of dealing with the dynamic toolbar manually.
    /* package */ int getCurrentToolbarHeight() {
        ThreadUtils.assertOnUiThread();

        if ((mToolbarChromeProxy != null) && mToolbarChromeProxy.isToolbarChromeVisible()) {
            return mMaxToolbarHeight;
        }
        return 0;
    }

    /**
     * Return whether the toolbar is pinned and cannot be hidden.
     *
     * @return True if the toolbar is pinned.
     */
    public boolean isPinned() {
        ThreadUtils.assertOnUiThread();

        return !mPinFlags.isEmpty();
    }

    public boolean isPinnedBy(PinReason reason) {
        ThreadUtils.assertOnUiThread();

        return mPinFlags.contains(reason);
    }

    public void setPinned(final boolean pinned, final PinReason reason) {
        ThreadUtils.assertOnUiThread();

        if (pinned != mPinFlags.contains(reason) && mCompositor.isReady()) {
            mCompositor.setPinned(pinned, reason.value);
        }

        if (pinned) {
            mPinFlags.add(reason);
        } else {
            mPinFlags.remove(reason);
        }
    }

    public void showToolbar(boolean immediately) {
        ThreadUtils.assertOnUiThread();

        if (mCompositor.isReady()) {
            mCompositor.sendToolbarAnimatorMessage(
                    immediately ? GeckoSession.REQUEST_SHOW_TOOLBAR_IMMEDIATELY
                                : GeckoSession.REQUEST_SHOW_TOOLBAR_ANIMATED);
        }
    }

    public void hideToolbar(boolean immediately) {
        ThreadUtils.assertOnUiThread();

        if (mCompositor.isReady()) {
            mCompositor.sendToolbarAnimatorMessage(
                    immediately ? GeckoSession.REQUEST_HIDE_TOOLBAR_IMMEDIATELY
                                : GeckoSession.REQUEST_HIDE_TOOLBAR_ANIMATED);
        }
    }

    /* package */ void onCompositorReady() {
        mCompositor.setMaxToolbarHeight(mMaxToolbarHeight);

        if ((mToolbarChromeProxy != null) && mToolbarChromeProxy.isToolbarChromeVisible()) {
            mCompositor.sendToolbarAnimatorMessage(
                    GeckoSession.REQUEST_SHOW_TOOLBAR_IMMEDIATELY);
        } else {
            mCompositor.sendToolbarAnimatorMessage(
                    GeckoSession.REQUEST_HIDE_TOOLBAR_IMMEDIATELY);
        }

        for (final PinReason reason : PinReason.values()) {
            mCompositor.setPinned(mPinFlags.contains(reason), reason.value);
        }
    }

    /* package */ void handleToolbarAnimatorMessage(final int message) {
        if (mToolbarChromeProxy == null || !mCompositor.isReady()) {
            return;
        }

        switch (message) {
            case GeckoSession.STATIC_TOOLBAR_NEEDS_UPDATE: {
                // Send updated toolbar image to compositor.
                final Bitmap bm = mToolbarChromeProxy.getBitmapOfToolbarChrome();
                if (bm == null) {
                    mCompositor.sendToolbarAnimatorMessage(
                            GeckoSession.TOOLBAR_SNAPSHOT_FAILED);
                    break;
                }

                try {
                    final int width = bm.getWidth();
                    final int height = bm.getHeight();
                    final int[] pixels = new int[bm.getByteCount() / 4];
                    bm.getPixels(pixels, /* offset */ 0, /* stride */ width,
                                 /* x */ 0, /* y */ 0, width, height);
                    mCompositor.sendToolbarPixelsToCompositor(width, height, pixels);
                } catch (final Throwable e) {
                    Log.e(LOGTAG, "Cannot get toolbar pixels", e);
                    mCompositor.sendToolbarAnimatorMessage(
                            GeckoSession.TOOLBAR_SNAPSHOT_FAILED);
                }
                break;
            }

            case GeckoSession.STATIC_TOOLBAR_READY: {
                // Hide toolbar and send TOOLBAR_HIDDEN message to compositor
                mToolbarChromeProxy.toggleToolbarChrome(false);
                mCompositor.sendToolbarAnimatorMessage(GeckoSession.TOOLBAR_HIDDEN);
                break;
            }

            case GeckoSession.TOOLBAR_SHOW: {
                // Show toolbar.
                mToolbarChromeProxy.toggleToolbarChrome(true);
                mCompositor.sendToolbarAnimatorMessage(GeckoSession.TOOLBAR_VISIBLE);
                break;
            }

            default:
                Log.e(LOGTAG, "Unhandled Toolbar Animator Message: " + message);
                break;
        }
    }
}

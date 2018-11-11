/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Shader;
import android.util.AttributeSet;

/**
 * Fades the end of the text by gecko:fadeWidth amount,
 * if the text is too long and requires an ellipsis.
 *
 * This implementation is an improvement over Android's built-in fadingEdge
 * and the fastest of Fennec's implementations. However, it only works for
 * text of one color. It works by applying a linear gradient directly to the text.
 */
public class FadedSingleColorTextView extends FadedTextView {
    // Shader for the fading edge.
    private FadedTextGradient mTextGradient;

    public FadedSingleColorTextView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    private void updateGradientShader() {
        final int color = getCurrentTextColor();
        final int width = getAvailableWidth();

        final boolean needsNewGradient = (mTextGradient == null ||
                                          mTextGradient.getColor() != color ||
                                          mTextGradient.getWidth() != width);

        final boolean needsEllipsis = needsEllipsis();
        if (needsEllipsis && needsNewGradient) {
            mTextGradient = new FadedTextGradient(width, fadeWidth, color);
        }

        getPaint().setShader(needsEllipsis ? mTextGradient : null);
    }

    @Override
    public void onDraw(Canvas canvas) {
        updateGradientShader();
        super.onDraw(canvas);
    }

    private static class FadedTextGradient extends LinearGradient {
        private final int mWidth;
        private final int mColor;

        public FadedTextGradient(int width, int fadeWidth, int color) {
            super(0, 0, width, 0,
                  new int[] { color, color, 0x0 },
                  new float[] { 0,  ((float) (width - fadeWidth) / width), 1.0f },
                  Shader.TileMode.CLAMP);

            mWidth = width;
            mColor = color;
        }

        public int getWidth() {
            return mWidth;
        }

        public int getColor() {
            return mColor;
        }
    }
}

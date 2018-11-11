/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.gfx;

import org.mozilla.gecko.annotation.WrapForJNI;

@WrapForJNI
public class ViewTransform {
    public float x;
    public float y;
    public float width;
    public float height;
    public float scale;
    public float fixedLayerMarginLeft;
    public float fixedLayerMarginTop;
    public float fixedLayerMarginRight;
    public float fixedLayerMarginBottom;

    public ViewTransform(float inX, float inY, float inScale) {
        x = inX;
        y = inY;
        scale = inScale;
    }
}


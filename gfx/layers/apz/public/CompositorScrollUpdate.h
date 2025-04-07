/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorScrollUpdate_h
#define mozilla_layers_CompositorScrollUpdate_h

#include "Units.h"

namespace mozilla {
namespace layers {

/**
 * An update sent by APZ to interesting consumers (e.g. GeckoView)
 * to inform them, on every composite, about the effective visual
 * scroll offset and zoom level of the root content APZC at
 * composition time.
 */
struct CompositorScrollUpdate {
  CSSPoint mVisualScrollOffset;
  CSSToParentLayerScale mZoom;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_CompositorScrollUpdate_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_Composer2D_h
#define mozilla_layers_Composer2D_h

#include "gfxTypes.h"
#include "nsISupportsImpl.h"

/**
 * Many platforms have dedicated hardware for simple composition.
 * This hardware is usually faster or more power efficient than the
 * GPU.  However, in exchange for this better performance, generality
 * has to be sacrificed: no 3d transforms, no intermediate surfaces,
 * no special shader effects, loss of other goodies depending on the
 * platform.
 *
 * Composer2D is a very simple interface to this class of hardware
 * that allows an implementation to "try rendering" with the fast
 * path.  If the given layer tree requires more generality than the
 * hardware provides, the implementation should bail and have the
 * layer manager fall back on full GPU composition.
 */

class nsIWidget;

namespace mozilla {
namespace layers {

class Layer;

class Composer2D {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Composer2D)

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~Composer2D() {}

public:
  /**
   * Return true if |aRoot| met the implementation's criteria for fast
   * composition and the render was successful.  Return false to fall
   * back on the GPU.
   *
   * Currently, when TryRender() returns true, the entire framebuffer
   * must have been rendered.
   */
  virtual bool TryRenderWithHwc(Layer* aRoot, nsIWidget* aWidget,
                                bool aGeometryChanged) = 0;

  /**
   * Return true if Composer2D does composition. Return false if Composer2D
   * failed the composition.
   */
  virtual bool Render(nsIWidget* aWidget) = 0;

  /**
   * Return true if Composer2D has a fast composition hardware.
   * Return false if Composer2D does not have a fast composition hardware.
   */
  virtual bool HasHwc() = 0;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_Composer2D_h

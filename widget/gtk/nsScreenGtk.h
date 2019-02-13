/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenGtk_h___
#define nsScreenGtk_h___

#include "nsBaseScreen.h"
#include "nsRect.h"
#include "gdk/gdk.h"
#ifdef MOZ_X11
#include <X11/Xlib.h>

// from Xinerama.h
typedef struct {
   int   screen_number;
   short x_org;
   short y_org;
   short width;
   short height;
} XineramaScreenInfo;
#endif /* MOZ_X11 */

//------------------------------------------------------------------------

class nsScreenGtk : public nsBaseScreen
{
public:
  nsScreenGtk();
  ~nsScreenGtk();

  NS_IMETHOD GetId(uint32_t* aId);
  NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetRectDisplayPix(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight);
  NS_IMETHOD GetAvailRectDisplayPix(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight);
  NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth);
  NS_IMETHOD GetColorDepth(int32_t* aColorDepth);

  void Init(GdkWindow *aRootWindow);
#ifdef MOZ_X11
  void Init(XineramaScreenInfo *aScreenInfo);
#endif /* MOZ_X11 */

  static gint    GetGtkMonitorScaleFactor();
  static double  GetDPIScale();

private:
  uint32_t mScreenNum;
  nsIntRect mRect;
  nsIntRect mAvailRect;
  uint32_t mId;
};

#endif  // nsScreenGtk_h___

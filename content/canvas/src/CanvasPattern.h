/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasPattern_h
#define mozilla_dom_CanvasPattern_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIPrincipal;

namespace mozilla {
namespace gfx {
class SourceSurface;
}

namespace dom {
class SVGMatrix;

class CanvasPattern MOZ_FINAL : public nsWrapperCache
{
  ~CanvasPattern() {}
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CanvasPattern)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(CanvasPattern)

  MOZ_BEGIN_NESTED_ENUM_CLASS(RepeatMode, uint8_t)
    REPEAT,
    REPEATX,
    REPEATY,
    NOREPEAT
  MOZ_END_NESTED_ENUM_CLASS(RepeatMode)

  CanvasPattern(CanvasRenderingContext2D* aContext,
                gfx::SourceSurface* aSurface,
                RepeatMode aRepeat,
                nsIPrincipal* principalForSecurityCheck,
                bool forceWriteOnly,
                bool CORSUsed)
    : mContext(aContext)
    , mSurface(aSurface)
    , mPrincipal(principalForSecurityCheck)
    , mTransform()
    , mForceWriteOnly(forceWriteOnly)
    , mCORSUsed(CORSUsed)
    , mRepeat(aRepeat)
  {
    SetIsDOMBinding();
  }

  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return CanvasPatternBinding::Wrap(aCx, this);
  }

  CanvasRenderingContext2D* GetParentObject()
  {
    return mContext;
  }

  // WebIDL
  void SetTransform(SVGMatrix& matrix);

  nsRefPtr<CanvasRenderingContext2D> mContext;
  RefPtr<gfx::SourceSurface> mSurface;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  mozilla::gfx::Matrix mTransform;
  const bool mForceWriteOnly;
  const bool mCORSUsed;
  const RepeatMode mRepeat;
};

MOZ_FINISH_NESTED_ENUM_CLASS(CanvasPattern::RepeatMode)

}
}

#endif // mozilla_dom_CanvasPattern_h

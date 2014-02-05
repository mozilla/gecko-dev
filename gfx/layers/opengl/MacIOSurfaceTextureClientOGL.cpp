/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureClientOGL.h"
#include "mozilla/gfx/MacIOSurface.h"

namespace mozilla {
namespace layers {

MacIOSurfaceTextureClientOGL::MacIOSurfaceTextureClientOGL(TextureFlags aFlags)
  : TextureClient(aFlags)
  , mIsLocked(false)
{}

MacIOSurfaceTextureClientOGL::~MacIOSurfaceTextureClientOGL()
{}

void
MacIOSurfaceTextureClientOGL::InitWith(MacIOSurface* aSurface)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(!IsAllocated());
  mSurface = aSurface;
}

bool
MacIOSurfaceTextureClientOGL::Lock(OpenMode aMode)
{
  MOZ_ASSERT(!mIsLocked);
  mIsLocked = true;
  return IsValid() && IsAllocated();
}

void
MacIOSurfaceTextureClientOGL::Unlock()
{
  MOZ_ASSERT(mIsLocked);
  mIsLocked = false;
}

bool
MacIOSurfaceTextureClientOGL::IsLocked() const
{
  return mIsLocked;
}

bool
MacIOSurfaceTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }
  aOutDescriptor = SurfaceDescriptorMacIOSurface(mSurface->GetIOSurfaceID(),
                                                 mSurface->GetContentsScaleFactor(),
                                                 mSurface->HasAlpha());
  return true;
}

gfx::IntSize
MacIOSurfaceTextureClientOGL::GetSize() const
{
  return gfx::IntSize(mSurface->GetDevicePixelWidth(), mSurface->GetDevicePixelHeight());
}

class MacIOSurfaceTextureClientData : public TextureClientData
{
public:
  MacIOSurfaceTextureClientData(MacIOSurface* aSurface)
    : mSurface(aSurface)
  {}

  virtual void DeallocateSharedData(ISurfaceAllocator*) MOZ_OVERRIDE
  {
    mSurface = nullptr;
  }

private:
  RefPtr<MacIOSurface> mSurface;
};

TextureClientData*
MacIOSurfaceTextureClientOGL::DropTextureData()
{
  TextureClientData* data = new MacIOSurfaceTextureClientData(mSurface);
  mSurface = nullptr;
  MarkInvalid();
  return data;
}

}
}

/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedSurfaceANGLE.h"

#include <d3d11.h>
#include "GLContextEGL.h"
#include "GLLibraryEGL.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/FileHandleWrapper.h"
#include "mozilla/layers/CompositeProcessD3D11FencesHolderMap.h"
#include "mozilla/layers/FenceD3D11.h"
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc

namespace mozilla {
namespace gl {

static ID3D11Device* GetD3D11DeviceOfEGLDisplay(GLContextEGL* gle) {
  const auto& egl = gle->mEgl;
  MOZ_ASSERT(egl);
  if (!egl ||
      !egl->mLib->IsExtensionSupported(gl::EGLLibExtension::EXT_device_query)) {
    return nullptr;
  }

  // Fetch the D3D11 device.
  EGLDeviceEXT eglDevice = nullptr;
  egl->fQueryDisplayAttribEXT(LOCAL_EGL_DEVICE_EXT, (EGLAttrib*)&eglDevice);
  MOZ_ASSERT(eglDevice);
  ID3D11Device* device = nullptr;
  egl->mLib->fQueryDeviceAttribEXT(eglDevice, LOCAL_EGL_D3D11_DEVICE_ANGLE,
                                   (EGLAttrib*)&device);
  if (!device) {
    return nullptr;
  }
  return device;
}

// Returns `EGL_NO_SURFACE` (`0`) on error.
static EGLSurface CreatePBufferSurface(EglDisplay* egl, EGLConfig config,
                                       const gfx::IntSize& size,
                                       RefPtr<ID3D11Texture2D> texture2D) {
  const EGLint attribs[] = {LOCAL_EGL_WIDTH, size.width, LOCAL_EGL_HEIGHT,
                            size.height, LOCAL_EGL_NONE};
  const auto buffer = reinterpret_cast<EGLClientBuffer>(texture2D.get());

  EGLSurface surface = egl->fCreatePbufferFromClientBuffer(
      LOCAL_EGL_D3D_TEXTURE_ANGLE, buffer, config, attribs);
  if (!surface) {
    EGLint err = egl->mLib->fGetError();
    gfxCriticalError() << "Failed to create Pbuffer surface error: "
                       << gfx::hexa(err) << " Size : " << size;
    return 0;
  }

  return surface;
}

/*static*/
UniquePtr<SharedSurface_ANGLEShareHandle>
SharedSurface_ANGLEShareHandle::Create(const SharedSurfaceDesc& desc) {
  const auto& gle = GLContextEGL::Cast(desc.gl);
  const auto& egl = gle->mEgl;
  MOZ_ASSERT(egl);
  MOZ_ASSERT(egl->IsExtensionSupported(
      EGLExtension::ANGLE_surface_d3d_texture_2d_share_handle));

  auto* device = GetD3D11DeviceOfEGLDisplay(gle);
  if (!device) {
    return nullptr;
  }

  // Create a texture in case we need to readback.
  auto* fencesHolderMap = layers::CompositeProcessD3D11FencesHolderMap::Get();
  const bool useFence =
      fencesHolderMap && layers::FenceD3D11::IsSupported(device);
  const DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
  CD3D11_TEXTURE2D_DESC texDesc(
      format, desc.size.width, desc.size.height, 1, 1,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
  texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
  if (useFence) {
    texDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
  } else {
    texDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  }

  RefPtr<ID3D11Texture2D> texture2D;
  auto hr =
      device->CreateTexture2D(&texDesc, nullptr, getter_AddRefs(texture2D));
  if (FAILED(hr)) {
    return nullptr;
  }

  RefPtr<IDXGIResource1> texDXGI;
  hr = texture2D->QueryInterface(__uuidof(IDXGIResource1),
                                 getter_AddRefs(texDXGI));
  if (FAILED(hr)) {
    return nullptr;
  }

  HANDLE sharedHandle = nullptr;
  texDXGI->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &sharedHandle);

  RefPtr<gfx::FileHandleWrapper> handle =
      new gfx::FileHandleWrapper(UniqueFileHandle(sharedHandle));

  Maybe<layers::CompositeProcessFencesHolderId> fencesHolderId;
  RefPtr<layers::FenceD3D11> fence;
  RefPtr<IDXGIKeyedMutex> keyedMutex;
  if (useFence) {
    fence = layers::FenceD3D11::Create(device);
    if (!fence) {
      return nullptr;
    }
    fencesHolderId = Some(layers::CompositeProcessFencesHolderId::GetNext());
  } else {
    texture2D->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(keyedMutex));
    if (!keyedMutex) {
      return nullptr;
    }
  }

  const auto& config = gle->mSurfaceConfig;
  MOZ_ASSERT(config);

  EGLSurface pbuffer =
      CreatePBufferSurface(egl.get(), config, desc.size, texture2D);
  if (!pbuffer) return nullptr;

  if (useFence) {
    auto* fencesHolderMap = layers::CompositeProcessD3D11FencesHolderMap::Get();
    fencesHolderMap->Register(fencesHolderId.ref());
  }

  return AsUnique(new SharedSurface_ANGLEShareHandle(
      desc, device, egl, pbuffer, std::move(handle), fencesHolderId, fence,
      keyedMutex));
}

SharedSurface_ANGLEShareHandle::SharedSurface_ANGLEShareHandle(
    const SharedSurfaceDesc& desc, const RefPtr<ID3D11Device> aDevice,
    const std::weak_ptr<EglDisplay>& egl, EGLSurface pbuffer,
    RefPtr<gfx::FileHandleWrapper>&& aSharedHandle,
    const Maybe<layers::CompositeProcessFencesHolderId> aFencesHolderId,
    const RefPtr<layers::FenceD3D11>& aWriteFence,
    const RefPtr<IDXGIKeyedMutex>& keyedMutex)
    : SharedSurface(desc, nullptr),
      mDevice(aDevice),
      mEGL(egl),
      mPBuffer(pbuffer),
      mSharedHandle(std::move(aSharedHandle)),
      mFencesHolderId(aFencesHolderId),
      mWriteFence(std::move(aWriteFence)),
      mKeyedMutex(keyedMutex) {
  MOZ_ASSERT((mKeyedMutex && mFencesHolderId.isNothing()) ||
             (!mKeyedMutex && mFencesHolderId.isSome()));
  MOZ_ASSERT_IF(mFencesHolderId.isSome(), mWriteFence);
}

SharedSurface_ANGLEShareHandle::~SharedSurface_ANGLEShareHandle() {
  const auto& gl = mDesc.gl;

  if (gl && GLContextEGL::Cast(gl)->GetEGLSurfaceOverride() == mPBuffer) {
    GLContextEGL::Cast(gl)->SetEGLSurfaceOverride(EGL_NO_SURFACE);
  }
  const auto egl = mEGL.lock();
  if (egl) {
    egl->fDestroySurface(mPBuffer);
  }
}

void SharedSurface_ANGLEShareHandle::LockProdImpl() {
  const auto& gl = mDesc.gl;
  GLContextEGL::Cast(gl)->SetEGLSurfaceOverride(mPBuffer);
}

void SharedSurface_ANGLEShareHandle::UnlockProdImpl() {}

void SharedSurface_ANGLEShareHandle::ProducerAcquireImpl() {
  if (mFencesHolderId.isSome()) {
    auto* fencesHolderMap = layers::CompositeProcessD3D11FencesHolderMap::Get();
    fencesHolderMap->WaitAllFencesAndForget(mFencesHolderId.ref(), mDevice);
  }
  if (mKeyedMutex) {
    HRESULT hr = mKeyedMutex->AcquireSync(0, 10000);
    if (hr == WAIT_TIMEOUT) {
      MOZ_CRASH("GFX: ANGLE share handle timeout");
    }
  }
}

void SharedSurface_ANGLEShareHandle::ProducerReleaseImpl() {
  const auto& gl = mDesc.gl;
  // XXX: ReleaseSync() has an implicit flush of the D3D commands
  // whether we need Flush() or not depends on the ANGLE semantics.
  // For now, we'll just do it
  gl->fFlush();
  if (mFencesHolderId.isSome()) {
    mWriteFence->IncrementAndSignal();
    auto* fencesHolderMap = layers::CompositeProcessD3D11FencesHolderMap::Get();
    fencesHolderMap->SetWriteFence(mFencesHolderId.ref(), mWriteFence);
  }
  if (mKeyedMutex) {
    mKeyedMutex->ReleaseSync(0);
  }
}

void SharedSurface_ANGLEShareHandle::ProducerReadAcquireImpl() {
  ProducerAcquireImpl();
}

void SharedSurface_ANGLEShareHandle::ProducerReadReleaseImpl() {
  if (mKeyedMutex) {
    mKeyedMutex->ReleaseSync(0);
  }
}

Maybe<layers::SurfaceDescriptor>
SharedSurface_ANGLEShareHandle::ToSurfaceDescriptor() {
  const auto format = gfx::SurfaceFormat::B8G8R8A8;
  return Some(layers::SurfaceDescriptorD3D10(
      mSharedHandle, /* gpuProcessTextureId */ Nothing(),
      /* arrayIndex */ 0, format, mDesc.size, mDesc.colorSpace,
      gfx::ColorRange::FULL, !!mKeyedMutex, mFencesHolderId));
}

////////////////////////////////////////////////////////////////////////////////
// Factory

/*static*/
UniquePtr<SurfaceFactory_ANGLEShareHandle>
SurfaceFactory_ANGLEShareHandle::Create(GLContext& gl) {
  if (!gl.IsANGLE()) return nullptr;

  const auto& gle = *GLContextEGL::Cast(&gl);
  const auto& egl = gle.mEgl;

  if (!egl->IsExtensionSupported(
          EGLExtension::ANGLE_surface_d3d_texture_2d_share_handle)) {
    return nullptr;
  }

  if (XRE_IsContentProcess()) {
    gfxPlatform::GetPlatform()->EnsureDevicesInitialized();
  }

  gfx::DeviceManagerDx* dm = gfx::DeviceManagerDx::Get();
  MOZ_ASSERT(dm);
  if (gl.IsWARP() != dm->IsWARP() || !dm->TextureSharingWorks()) {
    return nullptr;
  }

  return AsUnique(new SurfaceFactory_ANGLEShareHandle(
      {&gl, SharedSurfaceType::EGLSurfaceANGLE, layers::TextureType::D3D11,
       true}));
}

} /* namespace gl */
} /* namespace mozilla */

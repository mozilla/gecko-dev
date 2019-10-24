/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCLayerTree.h"

#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/StaticPrefs_gfx.h"

#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINBLUE
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WINBLUE

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>

namespace mozilla {
namespace wr {

// Currently, MinGW build environment does not handle IDCompositionDesktopDevice
// and IDCompositionDevice2
#if !defined(__MINGW32__)

/* static */
UniquePtr<DCLayerTree> DCLayerTree::Create(HWND aHwnd) {
  RefPtr<IDCompositionDevice2> dCompDevice =
      gfx::DeviceManagerDx::Get()->GetDirectCompositionDevice();
  if (!dCompDevice) {
    return nullptr;
  }

  auto layerTree = MakeUnique<DCLayerTree>(dCompDevice);
  if (!layerTree->Initialize(aHwnd)) {
    return nullptr;
  }

  return layerTree;
}

DCLayerTree::DCLayerTree(IDCompositionDevice2* aCompositionDevice)
    : mCompositionDevice(aCompositionDevice),
      mDebugCounter(false),
      mDebugVisualRedrawRegions(false) {}

DCLayerTree::~DCLayerTree() {}

bool DCLayerTree::Initialize(HWND aHwnd) {
  HRESULT hr;

  RefPtr<IDCompositionDesktopDevice> desktopDevice;
  hr = mCompositionDevice->QueryInterface(
      (IDCompositionDesktopDevice**)getter_AddRefs(desktopDevice));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to get IDCompositionDesktopDevice: "
                    << gfx::hexa(hr);
    return false;
  }

  hr = desktopDevice->CreateTargetForHwnd(aHwnd, TRUE,
                                          getter_AddRefs(mCompositionTarget));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create DCompositionTarget: " << gfx::hexa(hr);
    return false;
  }

  hr = mCompositionDevice->CreateVisual(getter_AddRefs(mRootVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
  }

  hr =
      mCompositionDevice->CreateVisual(getter_AddRefs(mDefaultSwapChainVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to create DCompositionVisual: " << gfx::hexa(hr);
    return false;
  }

  mCompositionTarget->SetRoot(mRootVisual);
  // Set interporation mode to Linear.
  // By default, a visual inherits the interpolation mode of the parent visual.
  // If no visuals set the interpolation mode, the default for the entire visual
  // tree is nearest neighbor interpolation.
  mRootVisual->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_LINEAR);
  return true;
}

void DCLayerTree::SetDefaultSwapChain(IDXGISwapChain1* aSwapChain) {
  mRootVisual->AddVisual(mDefaultSwapChainVisual, TRUE, nullptr);
  mDefaultSwapChainVisual->SetContent(aSwapChain);
  // Default SwapChain's visual does not need linear interporation.
  mDefaultSwapChainVisual->SetBitmapInterpolationMode(
      DCOMPOSITION_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
  mCompositionDevice->Commit();
}

void DCLayerTree::MaybeUpdateDebug() {
  bool updated = false;
  updated |= MaybeUpdateDebugCounter();
  updated |= MaybeUpdateDebugVisualRedrawRegions();
  if (updated) {
    mCompositionDevice->Commit();
  }
}

bool DCLayerTree::MaybeUpdateDebugCounter() {
  bool debugCounter = StaticPrefs::gfx_webrender_debug_dcomp_counter();
  if (mDebugCounter == debugCounter) {
    return false;
  }

  RefPtr<IDCompositionDeviceDebug> debugDevice;
  HRESULT hr = mCompositionDevice->QueryInterface(
      (IDCompositionDeviceDebug**)getter_AddRefs(debugDevice));
  if (FAILED(hr)) {
    return false;
  }

  if (debugCounter) {
    debugDevice->EnableDebugCounters();
  } else {
    debugDevice->DisableDebugCounters();
  }

  mDebugCounter = debugCounter;
  return true;
}

bool DCLayerTree::MaybeUpdateDebugVisualRedrawRegions() {
  bool debugVisualRedrawRegions =
      StaticPrefs::gfx_webrender_debug_dcomp_redraw_regions();
  if (mDebugVisualRedrawRegions == debugVisualRedrawRegions) {
    return false;
  }

  RefPtr<IDCompositionVisualDebug> visualDebug;
  HRESULT hr = mRootVisual->QueryInterface(
      (IDCompositionVisualDebug**)getter_AddRefs(visualDebug));
  if (FAILED(hr)) {
    return false;
  }

  if (debugVisualRedrawRegions) {
    visualDebug->EnableRedrawRegions();
  } else {
    visualDebug->DisableRedrawRegions();
  }

  mDebugVisualRedrawRegions = debugVisualRedrawRegions;
  return true;
}

#endif
}  // namespace wr
}  // namespace mozilla

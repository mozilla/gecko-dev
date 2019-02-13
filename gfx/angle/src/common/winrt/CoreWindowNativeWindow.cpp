//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CoreWindowNativeWindow.cpp: NativeWindow for managing ICoreWindow native window types.

#include <windows.graphics.display.h>
#include "common/winrt/CoreWindowNativeWindow.h"
using namespace ABI::Windows::Foundation::Collections;

CoreWindowNativeWindow::~CoreWindowNativeWindow()
{
    unregisterForSizeChangeEvents();
}

bool CoreWindowNativeWindow::initialize(EGLNativeWindowType window, IPropertySet *propertySet)
{
    ComPtr<IPropertySet> props = propertySet;
    ComPtr<IInspectable> win = window;
    SIZE swapChainSize = {};
    bool swapChainSizeSpecified = false;
    HRESULT result = S_OK;

    // IPropertySet is an optional parameter and can null.
    // If one is specified, cache as an IMap and read the properties
    // used for initial host initialization.
    if (propertySet)
    {
        result = props.As(&mPropertyMap);
        if (SUCCEEDED(result))
        {
            // The EGLRenderSurfaceSizeProperty is optional and may be missing.  The IPropertySet
            // was prevalidated to contain the EGLNativeWindowType before being passed to
            // this host.
            result = getOptionalSizePropertyValue(mPropertyMap, EGLRenderSurfaceSizeProperty, &swapChainSize, &swapChainSizeSpecified);
        }
    }

    if (SUCCEEDED(result))
    {
        result = win.As(&mCoreWindow);
    }

    if (SUCCEEDED(result))
    {
        // If a swapchain size is specfied, then the automatic resize
        // behaviors implemented by the host should be disabled.  The swapchain
        // will be still be scaled when being rendered to fit the bounds
        // of the host.
        // Scaling of the swapchain output occurs automatically because if
        // the scaling mode setting DXGI_SCALING_STRETCH on the swapchain.
        if (swapChainSizeSpecified)
        {
            mClientRect = { 0, 0, swapChainSize.cx, swapChainSize.cy };
            mSupportsSwapChainResize = false;
        }
        else
        {
            result = getCoreWindowSizeInPixels(mCoreWindow, &mClientRect);
        }
    }

    if (SUCCEEDED(result))
    {
        mNewClientRect = mClientRect;
        mClientRectChanged = false;
        return registerForSizeChangeEvents();
    }

    return false;
}

bool CoreWindowNativeWindow::registerForSizeChangeEvents()
{
    ComPtr<IWindowSizeChangedEventHandler> sizeChangedHandler;
    HRESULT result = Microsoft::WRL::MakeAndInitialize<CoreWindowSizeChangedHandler>(sizeChangedHandler.ReleaseAndGetAddressOf(), this->shared_from_this());
    if (SUCCEEDED(result))
    {
        result = mCoreWindow->add_SizeChanged(sizeChangedHandler.Get(), &mSizeChangedEventToken);
    }

    if (SUCCEEDED(result))
    {
        return true;
    }

    return false;
}

void CoreWindowNativeWindow::unregisterForSizeChangeEvents()
{
    if (mCoreWindow)
    {
        (void)mCoreWindow->remove_SizeChanged(mSizeChangedEventToken);
    }
    mSizeChangedEventToken.value = 0;
}

HRESULT CoreWindowNativeWindow::createSwapChain(ID3D11Device *device, DXGIFactory *factory, DXGI_FORMAT format, unsigned int width, unsigned int height, DXGISwapChain **swapChain)
{
    if (device == NULL || factory == NULL || swapChain == NULL || width == 0 || height == 0)
    {
        return E_INVALIDARG;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = format;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    *swapChain = nullptr;

    ComPtr<IDXGISwapChain1> newSwapChain;
    HRESULT result = factory->CreateSwapChainForCoreWindow(device, mCoreWindow.Get(), &swapChainDesc, nullptr, newSwapChain.ReleaseAndGetAddressOf());
    if (SUCCEEDED(result))
    {

#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
        // Test if swapchain supports resize.  On Windows Phone devices, this will return DXGI_ERROR_UNSUPPORTED.  On
        // other devices DXGI_ERROR_INVALID_CALL should be returned because the combination of flags passed
        // (DXGI_SWAP_CHAIN_FLAG_NONPREROTATED | DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE) are invalid flag combinations.
        if (newSwapChain->ResizeBuffers(swapChainDesc.BufferCount, swapChainDesc.Width, swapChainDesc.Height, swapChainDesc.Format, DXGI_SWAP_CHAIN_FLAG_NONPREROTATED | DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE) == DXGI_ERROR_UNSUPPORTED)
        {
            mSupportsSwapChainResize = false;
        }
#endif // (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)

        result = newSwapChain.CopyTo(swapChain);
    }

    if (SUCCEEDED(result))
    {
        // If automatic swapchain resize behaviors have been disabled, then
        // unregister for the resize change events.
        if (mSupportsSwapChainResize == false)
        {
            unregisterForSizeChangeEvents();
        }
    }

    return result;
}

HRESULT getCoreWindowSizeInPixels(const ComPtr<ABI::Windows::UI::Core::ICoreWindow>& coreWindow, RECT *windowSize)
{
    ABI::Windows::Foundation::Rect bounds;
    HRESULT result = coreWindow->get_Bounds(&bounds);
    if (SUCCEEDED(result))
    {
        *windowSize = { 0, 0, ConvertDipsToPixels(bounds.Width), ConvertDipsToPixels(bounds.Height) };
    }

    return result;
}

static float GetLogicalDpi()
{
    ComPtr<ABI::Windows::Graphics::Display::IDisplayPropertiesStatics> displayProperties;
    float dpi = 96.0f;

    if (SUCCEEDED(GetActivationFactory(HStringReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(), displayProperties.GetAddressOf())))
    {
        if (SUCCEEDED(displayProperties->get_LogicalDpi(&dpi)))
        {
            return dpi;
        }
    }
    return dpi;
}

long ConvertDipsToPixels(float dips)
{
    static const float dipsPerInch = 96.0f;
    return lround((dips * GetLogicalDpi() / dipsPerInch));
}
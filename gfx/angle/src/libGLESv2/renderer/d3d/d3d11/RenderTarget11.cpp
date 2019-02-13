//
// Copyright (c) 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderTarget11.cpp: Implements a DX11-specific wrapper for ID3D11View pointers
// retained by Renderbuffers.

#include "libGLESv2/renderer/d3d/d3d11/RenderTarget11.h"
#include "libGLESv2/renderer/d3d/d3d11/Renderer11.h"
#include "libGLESv2/renderer/d3d/d3d11/renderer11_utils.h"
#include "libGLESv2/renderer/d3d/d3d11/formatutils11.h"
#include "libGLESv2/main.h"

namespace rx
{

static bool getTextureProperties(ID3D11Resource *resource, unsigned int *mipLevels, unsigned int *samples)
{
    ID3D11Texture1D *texture1D = d3d11::DynamicCastComObject<ID3D11Texture1D>(resource);
    if (texture1D)
    {
        D3D11_TEXTURE1D_DESC texDesc;
        texture1D->GetDesc(&texDesc);
        SafeRelease(texture1D);

        *mipLevels = texDesc.MipLevels;
        *samples = 0;

        return true;
    }

    ID3D11Texture2D *texture2D = d3d11::DynamicCastComObject<ID3D11Texture2D>(resource);
    if (texture2D)
    {
        D3D11_TEXTURE2D_DESC texDesc;
        texture2D->GetDesc(&texDesc);
        SafeRelease(texture2D);

        *mipLevels = texDesc.MipLevels;
        *samples = texDesc.SampleDesc.Count > 1 ? texDesc.SampleDesc.Count : 0;

        return true;
    }

    ID3D11Texture3D *texture3D = d3d11::DynamicCastComObject<ID3D11Texture3D>(resource);
    if (texture3D)
    {
        D3D11_TEXTURE3D_DESC texDesc;
        texture3D->GetDesc(&texDesc);
        SafeRelease(texture3D);

        *mipLevels = texDesc.MipLevels;
        *samples = 0;

        return true;
    }

    return false;
}

static unsigned int getRTVSubresourceIndex(ID3D11Resource *resource, ID3D11RenderTargetView *view)
{
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    view->GetDesc(&rtvDesc);

    unsigned int mipSlice = 0;
    unsigned int arraySlice = 0;

    switch (rtvDesc.ViewDimension)
    {
      case D3D11_RTV_DIMENSION_TEXTURE1D:
        mipSlice = rtvDesc.Texture1D.MipSlice;
        arraySlice = 0;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE1DARRAY:
        mipSlice = rtvDesc.Texture1DArray.MipSlice;
        arraySlice = rtvDesc.Texture1DArray.FirstArraySlice;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2D:
        mipSlice = rtvDesc.Texture2D.MipSlice;
        arraySlice = 0;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DARRAY:
        mipSlice = rtvDesc.Texture2DArray.MipSlice;
        arraySlice = rtvDesc.Texture2DArray.FirstArraySlice;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DMS:
        mipSlice = 0;
        arraySlice = 0;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY:
        mipSlice = 0;
        arraySlice = rtvDesc.Texture2DMSArray.FirstArraySlice;
        break;

      case D3D11_RTV_DIMENSION_TEXTURE3D:
        mipSlice = rtvDesc.Texture3D.MipSlice;
        arraySlice = 0;
        break;

      case D3D11_RTV_DIMENSION_UNKNOWN:
      case D3D11_RTV_DIMENSION_BUFFER:
        UNIMPLEMENTED();
        break;

      default:
        UNREACHABLE();
        break;
    }

    unsigned int mipLevels, samples;
    getTextureProperties(resource,  &mipLevels, &samples);

    return D3D11CalcSubresource(mipSlice, arraySlice, mipLevels);
}

static unsigned int getDSVSubresourceIndex(ID3D11Resource *resource, ID3D11DepthStencilView *view)
{
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    view->GetDesc(&dsvDesc);

    unsigned int mipSlice = 0;
    unsigned int arraySlice = 0;

    switch (dsvDesc.ViewDimension)
    {
      case D3D11_DSV_DIMENSION_TEXTURE1D:
        mipSlice = dsvDesc.Texture1D.MipSlice;
        arraySlice = 0;
        break;

      case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
        mipSlice = dsvDesc.Texture1DArray.MipSlice;
        arraySlice = dsvDesc.Texture1DArray.FirstArraySlice;
        break;

      case D3D11_DSV_DIMENSION_TEXTURE2D:
        mipSlice = dsvDesc.Texture2D.MipSlice;
        arraySlice = 0;
        break;

      case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
        mipSlice = dsvDesc.Texture2DArray.MipSlice;
        arraySlice = dsvDesc.Texture2DArray.FirstArraySlice;
        break;

      case D3D11_DSV_DIMENSION_TEXTURE2DMS:
        mipSlice = 0;
        arraySlice = 0;
        break;

      case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
        mipSlice = 0;
        arraySlice = dsvDesc.Texture2DMSArray.FirstArraySlice;
        break;

      case D3D11_DSV_DIMENSION_UNKNOWN:
        UNIMPLEMENTED();
        break;

      default:
        UNREACHABLE();
        break;
    }

    unsigned int mipLevels, samples;
    getTextureProperties(resource, &mipLevels, &samples);

    return D3D11CalcSubresource(mipSlice, arraySlice, mipLevels);
}

RenderTarget11::RenderTarget11(Renderer *renderer, ID3D11RenderTargetView *rtv, ID3D11Resource *resource,
                               ID3D11ShaderResourceView *srv, GLsizei width, GLsizei height, GLsizei depth,
                               GLenum internalFormatOverride)
{
    mRenderer = Renderer11::makeRenderer11(renderer);

    mTexture = resource;
    if (mTexture)
    {
        mTexture->AddRef();
    }

    mRenderTarget = rtv;
    if (mRenderTarget)
    {
        mRenderTarget->AddRef();
    }

    mDepthStencil = NULL;

    mShaderResource = srv;
    if (mShaderResource)
    {
        mShaderResource->AddRef();
    }

    mSubresourceIndex = 0;

    if (mRenderTarget && mTexture)
    {
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        mRenderTarget->GetDesc(&desc);

        unsigned int mipLevels, samples;
        getTextureProperties(mTexture, &mipLevels, &samples);

        mSubresourceIndex = getRTVSubresourceIndex(mTexture, mRenderTarget);
        mWidth = width;
        mHeight = height;
        mDepth = depth;
        mSamples = samples;

        const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(desc.Format);
        mInternalFormat = dxgiFormatInfo.internalFormat;
        mActualFormat = dxgiFormatInfo.internalFormat;

        if (internalFormatOverride) {
            mInternalFormat = internalFormatOverride;
        }
    }
}

RenderTarget11::RenderTarget11(Renderer *renderer, ID3D11DepthStencilView *dsv, ID3D11Resource *resource,
                               ID3D11ShaderResourceView *srv, GLsizei width, GLsizei height, GLsizei depth)
{
    mRenderer = Renderer11::makeRenderer11(renderer);

    mTexture = resource;
    if (mTexture)
    {
        mTexture->AddRef();
    }

    mRenderTarget = NULL;

    mDepthStencil = dsv;
    if (mDepthStencil)
    {
        mDepthStencil->AddRef();
    }

    mShaderResource = srv;
    if (mShaderResource)
    {
        mShaderResource->AddRef();
    }

    mSubresourceIndex = 0;

    if (mDepthStencil && mTexture)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        mDepthStencil->GetDesc(&desc);

        unsigned int mipLevels, samples;
        getTextureProperties(mTexture, &mipLevels, &samples);

        mSubresourceIndex = getDSVSubresourceIndex(mTexture, mDepthStencil);
        mWidth = width;
        mHeight = height;
        mDepth = depth;
        mSamples = samples;

        const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(desc.Format);
        mInternalFormat = dxgiFormatInfo.internalFormat;
        mActualFormat = dxgiFormatInfo.internalFormat;
    }
}

UINT64 EstimateSize(D3D11_TEXTURE2D_DESC &desc)
{
    //XXX: handle overflow (64 bits should be enough for anyone...)
    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(desc.Format);
    // NVIDIA seems to align the width of buffers by 8 and the height by 64, so we do the same.
    UINT64 total = UINT64(rx::roundUp(desc.Width, UINT(8))) * rx::roundUp(desc.Height, UINT(64)) * desc.SampleDesc.Count * dxgiFormatInfo.pixelBytes;
    return total;
}

RenderTarget11::RenderTarget11(Renderer *renderer, GLsizei width, GLsizei height, GLenum internalFormat, GLsizei samples)
{
    mRenderer = Renderer11::makeRenderer11(renderer);
    mTexture = NULL;
    mRenderTarget = NULL;
    mDepthStencil = NULL;
    mShaderResource = NULL;

    const d3d11::TextureFormat &formatInfo = d3d11::GetTextureFormatInfo(internalFormat);
    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(formatInfo.texFormat);

    const gl::TextureCaps &textureCaps = mRenderer->getRendererTextureCaps().get(internalFormat);
    GLuint supportedSamples = textureCaps.getNearestSamples(samples);

    if (width > 0 && height > 0)
    {
        // Create texture resource
        D3D11_TEXTURE2D_DESC desc;
        desc.Width = width; 
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = formatInfo.texFormat;
        desc.SampleDesc.Count = (supportedSamples == 0) ? 1 : supportedSamples;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        // If a rendertarget or depthstencil format exists for this texture format,
        // we'll flag it to allow binding that way. Shader resource views are a little
        // more complicated.
        bool bindRTV = false, bindDSV = false, bindSRV = false;
        bindRTV = (formatInfo.rtvFormat != DXGI_FORMAT_UNKNOWN);
        bindDSV = (formatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN);
        if (formatInfo.srvFormat != DXGI_FORMAT_UNKNOWN)
        {
            // Multisample targets flagged for binding as depth stencil cannot also be
            // flagged for binding as SRV, so make certain not to add the SRV flag for
            // these targets.
            bindSRV = !(formatInfo.dsvFormat != DXGI_FORMAT_UNKNOWN && desc.SampleDesc.Count > 1);
        }

        desc.BindFlags = (bindRTV ? D3D11_BIND_RENDER_TARGET   : 0) |
                         (bindDSV ? D3D11_BIND_DEPTH_STENCIL   : 0) |
                         (bindSRV ? D3D11_BIND_SHADER_RESOURCE : 0);

        ID3D11Device *device = mRenderer->getDevice();
        ID3D11Texture2D *texture = NULL;
        HRESULT result;

        // Some Nvidia drivers (GeForce GT 610 w/ 9.18.13.3523) crash with very large render targets
        if (EstimateSize(desc) > mRenderer->getMaxResourceSize())
        {
            result = E_OUTOFMEMORY;
        }
        else
        {
            result = device->CreateTexture2D(&desc, NULL, &texture);
        }
        mTexture = texture;

        if (result == E_OUTOFMEMORY)
        {
            gl::error(GL_OUT_OF_MEMORY);
            return;
        }
        ASSERT(SUCCEEDED(result));

        if (bindSRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format = formatInfo.srvFormat;
            srvDesc.ViewDimension = (supportedSamples == 0) ? D3D11_SRV_DIMENSION_TEXTURE2D : D3D11_SRV_DIMENSION_TEXTURE2DMS;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
            result = device->CreateShaderResourceView(mTexture, &srvDesc, &mShaderResource);

            if (result == E_OUTOFMEMORY)
            {
                SafeRelease(mTexture);
                gl::error(GL_OUT_OF_MEMORY);
                return;
            }
            ASSERT(SUCCEEDED(result));
        }

        if (bindDSV)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
            dsvDesc.Format = formatInfo.dsvFormat;
            dsvDesc.ViewDimension = (supportedSamples == 0) ? D3D11_DSV_DIMENSION_TEXTURE2D : D3D11_DSV_DIMENSION_TEXTURE2DMS;
            dsvDesc.Texture2D.MipSlice = 0;
            dsvDesc.Flags = 0;
            result = device->CreateDepthStencilView(mTexture, &dsvDesc, &mDepthStencil);

            if (result == E_OUTOFMEMORY)
            {
                SafeRelease(mTexture);
                SafeRelease(mShaderResource);
                gl::error(GL_OUT_OF_MEMORY);
                return;
            }
            ASSERT(SUCCEEDED(result));
        }

        if (bindRTV)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format = formatInfo.rtvFormat;
            rtvDesc.ViewDimension = (supportedSamples == 0) ? D3D11_RTV_DIMENSION_TEXTURE2D : D3D11_RTV_DIMENSION_TEXTURE2DMS;
            rtvDesc.Texture2D.MipSlice = 0;
            result = device->CreateRenderTargetView(mTexture, &rtvDesc, &mRenderTarget);

            if (result == E_OUTOFMEMORY)
            {
                SafeRelease(mTexture);
                SafeRelease(mShaderResource);
                SafeRelease(mDepthStencil);
                gl::error(GL_OUT_OF_MEMORY);
                return;
            }
            ASSERT(SUCCEEDED(result));

            if (formatInfo.dataInitializerFunction != NULL)
            {
                ID3D11DeviceContext *context = mRenderer->getDeviceContext();

                const float clearValues[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                context->ClearRenderTargetView(mRenderTarget, clearValues);
            }
        }
    }


    mWidth = width;
    mHeight = height;
    mDepth = 1;
    mInternalFormat = internalFormat;
    mSamples = supportedSamples;
    mActualFormat = dxgiFormatInfo.internalFormat;
    mSubresourceIndex = D3D11CalcSubresource(0, 0, 1);
}

RenderTarget11::~RenderTarget11()
{
    SafeRelease(mTexture);
    SafeRelease(mRenderTarget);
    SafeRelease(mDepthStencil);
    SafeRelease(mShaderResource);
}

RenderTarget11 *RenderTarget11::makeRenderTarget11(RenderTarget *target)
{
    ASSERT(HAS_DYNAMIC_TYPE(rx::RenderTarget11*, target));
    return static_cast<rx::RenderTarget11*>(target);
}

void RenderTarget11::invalidate(GLint x, GLint y, GLsizei width, GLsizei height)
{
    // Currently a no-op
}

ID3D11Resource *RenderTarget11::getTexture() const
{
    return mTexture;
}

ID3D11RenderTargetView *RenderTarget11::getRenderTargetView() const
{
    return mRenderTarget;
}

ID3D11DepthStencilView *RenderTarget11::getDepthStencilView() const
{
    return mDepthStencil;
}

ID3D11ShaderResourceView *RenderTarget11::getShaderResourceView() const
{
    return mShaderResource;
}

unsigned int RenderTarget11::getSubresourceIndex() const
{
    return mSubresourceIndex;
}

}

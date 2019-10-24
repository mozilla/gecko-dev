/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define UNICODE

#include <windows.h>
#include <math.h>
#include <dcomp.h>
#include <d3d11.h>
#include <assert.h>
#include <map>

#define EGL_EGL_PROTOTYPES 1
#define EGL_EGLEXT_PROTOTYPES 1
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "EGL/eglext_angle.h"
#include "GL/gl.h"

// The OS compositor representation of a picture cache tile.
struct Tile {
    // Represents the underlying DirectComposition surface texture that gets drawn into.
    IDCompositionSurface *pSurface;
    // Represents the node in the visual tree that defines the properties of this tile (clip, position etc).
    IDCompositionVisual *pVisual;
};

struct Window {
    // Win32 window details
    HWND hWnd;
    HINSTANCE hInstance;
    int width;
    int height;
    bool enable_compositor;
    RECT client_rect;

    // Main interfaces to D3D11 and DirectComposition
    ID3D11Device *pD3D11Device;
    IDCompositionDevice *pDCompDevice;
    IDCompositionTarget *pDCompTarget;
    IDXGIDevice *pDXGIDevice;

    // ANGLE interfaces that wrap the D3D device
    EGLDeviceEXT EGLDevice;
    EGLDisplay EGLDisplay;
    EGLContext EGLContext;
    EGLConfig config;
    // Framebuffer surface for debug mode when we are not using DC
    EGLSurface fb_surface;

    // The currently bound surface, valid during bind() and unbind()
    EGLSurface current_surface;
    IDCompositionSurface *pCurrentSurface;

    // The root of the DC visual tree. Nothing is drawn on this, but
    // all child tiles are parented to here.
    IDCompositionVisual *pRoot;
    // Maps the WR surface IDs to the DC representation of each tile.
    std::map<uint64_t, Tile> tiles;
};

static const wchar_t *CLASS_NAME = L"WR DirectComposite";

static LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 1;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

extern "C" {
    Window *com_dc_create_window(int width, int height, bool enable_compositor) {
        // Create a simple Win32 window
        Window *window = new Window;
        window->hInstance = GetModuleHandle(NULL);
        window->width = width;
        window->height = height;
        window->enable_compositor = enable_compositor;

        WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = window->hInstance;
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);;
        wcex.lpszMenuName = nullptr;
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.lpszClassName = CLASS_NAME;
        RegisterClassEx(&wcex);

        int dpiX = 0;
        int dpiY = 0;
        HDC hdc = GetDC(NULL);
        if (hdc) {
            dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(NULL, hdc);
        }

        window->hWnd = CreateWindow(
            CLASS_NAME,
            L"DirectComposition Demo Application",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            static_cast<UINT>(ceil(float(width) * dpiX / 96.f)),
            static_cast<UINT>(ceil(float(height) * dpiY / 96.f)),
            NULL,
            NULL,
            window->hInstance,
            NULL
        );

        ShowWindow(window->hWnd, SW_SHOWNORMAL);
        UpdateWindow(window->hWnd);
        GetClientRect(window->hWnd, &window->client_rect);

        // Create a D3D11 device
        D3D_FEATURE_LEVEL featureLevelSupported;
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            NULL,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,
            0,
            D3D11_SDK_VERSION,
            &window->pD3D11Device,
            &featureLevelSupported,
            nullptr
        );
        assert(SUCCEEDED(hr));

        hr = window->pD3D11Device->QueryInterface(&window->pDXGIDevice);
        assert(SUCCEEDED(hr));

        // Create a DirectComposition device
        hr = DCompositionCreateDevice(
            window->pDXGIDevice,
            __uuidof(IDCompositionDevice),
            (void **) &window->pDCompDevice
        );
        assert(SUCCEEDED(hr));

        // Create a DirectComposition target for a Win32 window handle
        hr = window->pDCompDevice->CreateTargetForHwnd(
            window->hWnd,
            TRUE,
            &window->pDCompTarget
        );
        assert(SUCCEEDED(hr));

        // Create an ANGLE EGL device that wraps D3D11
        window->EGLDevice = eglCreateDeviceANGLE(
            EGL_D3D11_DEVICE_ANGLE,
            window->pD3D11Device,
            nullptr
        );

        EGLint display_attribs[] = {
            EGL_NONE
        };

        window->EGLDisplay = eglGetPlatformDisplayEXT(
            EGL_PLATFORM_DEVICE_EXT,
            window->EGLDevice,
            display_attribs
        );

        eglInitialize(
            window->EGLDisplay,
            nullptr,
            nullptr
        );

        EGLint num_configs = 0;
        EGLint cfg_attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
        };
        EGLConfig configs[32];

        eglChooseConfig(
            window->EGLDisplay,
            cfg_attribs,
            configs,
            sizeof(configs) / sizeof(EGLConfig),
            &num_configs
        );
        assert(num_configs > 0);
        window->config = configs[0];

        if (window->enable_compositor) {
            window->fb_surface = EGL_NO_SURFACE;
        } else {
            window->fb_surface = eglCreateWindowSurface(
                window->EGLDisplay,
                window->config,
                window->hWnd,
                NULL
            );
            assert(window->fb_surface != EGL_NO_SURFACE);
        }

        EGLint ctx_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };

        // Create an EGL context that can be used for drawing
        window->EGLContext = eglCreateContext(
            window->EGLDisplay,
            window->config,
            EGL_NO_CONTEXT,
            ctx_attribs
        );

        // Create the root of the DirectComposition visual tree
        hr = window->pDCompDevice->CreateVisual(&window->pRoot);
        assert(SUCCEEDED(hr));
        hr = window->pDCompTarget->SetRoot(window->pRoot);
        assert(SUCCEEDED(hr));

        EGLBoolean ok = eglMakeCurrent(
            window->EGLDisplay,
            window->fb_surface,
            window->fb_surface,
            window->EGLContext
        );
        assert(ok);

        return window;
    }

    void com_dc_destroy_window(Window *window) {
        for (auto it=window->tiles.begin() ; it != window->tiles.end() ; ++it) {
            it->second.pSurface->Release();
            it->second.pVisual->Release();
        }

        if (window->fb_surface != EGL_NO_SURFACE) {
            eglDestroySurface(window->EGLDisplay, window->fb_surface);
        }
        eglDestroyContext(window->EGLDisplay, window->EGLContext);
        eglTerminate(window->EGLDisplay);
        eglReleaseDeviceANGLE(window->EGLDevice);

        window->pRoot->Release();
        window->pD3D11Device->Release();
        window->pDXGIDevice->Release();
        window->pDCompDevice->Release();
        window->pDCompTarget->Release();

        CloseWindow(window->hWnd);
        UnregisterClass(CLASS_NAME, window->hInstance);

        delete window;
    }

    bool com_dc_tick(Window *window) {
        // Check and dispatch the windows event loop
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return false;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return true;
    }

    void com_dc_swap_buffers(Window *window) {
        // If not using DC mode, then do a normal EGL swap buffers.
        if (window->fb_surface != EGL_NO_SURFACE) {
            eglSwapBuffers(window->EGLDisplay, window->fb_surface);
        }
    }

    // Create a new DC surface
    void com_dc_create_surface(
        Window *window,
        uint64_t id,
        int width,
        int height
    ) {
        assert(window->tiles.count(id) == 0);

        Tile tile;

        // Create the video memory surface.
        // TODO(gw): We should set alpha mode appropriately so that DC
        //           can do opaque composites when possible!
        HRESULT hr = window->pDCompDevice->CreateSurface(
            width,
            height,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            DXGI_ALPHA_MODE_PREMULTIPLIED,
            &tile.pSurface
        );
        assert(SUCCEEDED(hr));

        // Create the visual node in the DC tree that stores properties
        hr = window->pDCompDevice->CreateVisual(&tile.pVisual);
        assert(SUCCEEDED(hr));

        // Bind the surface memory to this visual
        hr = tile.pVisual->SetContent(tile.pSurface);
        assert(SUCCEEDED(hr));

        window->tiles[id] = tile;
    }

    void com_dc_destroy_surface(
        Window *window,
        uint64_t id
    ) {
        assert(window->tiles.count(id) == 1);

        // Release the video memory and visual in the tree
        Tile &tile = window->tiles[id];
        tile.pVisual->Release();
        tile.pSurface->Release();

        window->tiles.erase(id);
    }

    // Bind a DC surface to allow issuing GL commands to it
    void com_dc_bind_surface(
        Window *window,
        uint64_t id,
        int *x_offset,
        int *y_offset
    ) {
        assert(window->tiles.count(id) == 1);
        Tile &tile = window->tiles[id];

        // Store the current surface for unbinding later
        window->pCurrentSurface = tile.pSurface;

        // Inform DC that we want to draw on this surface. DC uses texture
        // atlases when the tiles are small. It returns an offset where the
        // client code must draw into this surface when this happens.
        POINT offset;
        D3D11_TEXTURE2D_DESC desc;
        ID3D11Texture2D *pTexture;
        HRESULT hr = tile.pSurface->BeginDraw(
            NULL,
            __uuidof(ID3D11Texture2D),
            (void **) &pTexture,
            &offset
        );
        assert(SUCCEEDED(hr));
        pTexture->GetDesc(&desc);

        // Construct an EGL off-screen surface that is bound to the DC surface
        EGLint buffer_attribs[] = {
            EGL_WIDTH, desc.Width,
            EGL_HEIGHT, desc.Height,
            EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE, EGL_TRUE,
            EGL_NONE
        };

        window->current_surface = eglCreatePbufferFromClientBuffer(
            window->EGLDisplay,
            EGL_D3D_TEXTURE_ANGLE,
            pTexture,
            window->config,
            buffer_attribs
        );
        assert(window->current_surface != EGL_NO_SURFACE);

        // Make EGL current on the DC surface
        EGLBoolean ok = eglMakeCurrent(
            window->EGLDisplay,
            window->current_surface,
            window->current_surface,
            window->EGLContext
        );
        assert(ok);

        *x_offset = offset.x;
        *y_offset = offset.y;
    }

    // Unbind a currently bound DC surface
    void com_dc_unbind_surface(Window *window) {
        HRESULT hr = window->pCurrentSurface->EndDraw();
        assert(SUCCEEDED(hr));

        eglDestroySurface(window->EGLDisplay, window->current_surface);
    }

    // At the start of a transaction, remove all visuals from the tree.
    // TODO(gw): This is super simple, maybe it has performance implications
    //           and we should mutate the visual tree instead of rebuilding
    //           it each composition?
    void com_dc_begin_transaction(Window *window) {
        HRESULT hr = window->pRoot->RemoveAllVisuals();
        assert(SUCCEEDED(hr));
    }

    // Add a DC surface to the visual tree. Called per-frame to build the composition.
    void com_dc_add_surface(
        Window *window,
        uint64_t id,
        int x,
        int y,
        int clip_x,
        int clip_y,
        int clip_w,
        int clip_h
    ) {
        Tile &tile = window->tiles[id];

        // Add this visual as the last element in the visual tree (z-order is implicit,
        // based on the order tiles are added).
        HRESULT hr = window->pRoot->AddVisual(
            tile.pVisual,
            FALSE,
            NULL
        );
        assert(SUCCEEDED(hr));

        // Place the visual - this changes frame to frame based on scroll position
        // of the slice.
        int offset_x = x + window->client_rect.left;
        int offset_y = y + window->client_rect.top;
        tile.pVisual->SetOffsetX(offset_x);
        tile.pVisual->SetOffsetY(offset_y);

        // Set the clip rect - converting from world space to the pre-offset space
        // that DC requires for rectangle clips.
        D2D_RECT_F clip_rect;
        clip_rect.left = clip_x - offset_x;
        clip_rect.top = clip_y - offset_y;
        clip_rect.right = clip_rect.left + clip_w;
        clip_rect.bottom = clip_rect.top + clip_h;
        tile.pVisual->SetClip(clip_rect);
    }

    // Finish the composition transaction, telling DC to composite
    void com_dc_end_transaction(Window *window) {
        HRESULT hr = window->pDCompDevice->Commit();
        assert(SUCCEEDED(hr));
    }

    // Get a pointer to an EGL symbol
    void *com_dc_get_proc_address(const char *name) {
        return eglGetProcAddress(name);
    }
}

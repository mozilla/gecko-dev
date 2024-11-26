/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::os::raw::c_void;
use std::ptr;

use glutin::platform::windows::EGLContext;
use webrender::{Compositor2, CompositorInputConfig, CompositorOutputConfig};
use winit::platform::windows::WindowExtWindows;

use crate::WindowWrapper;

use mozangle::egl::ffi::types::{EGLDisplay, EGLSurface, EGLint};
use mozangle::egl;

// A simplistic implementation of the `Compositor2` trait to allow wrench to
// composite via DirectComposition. In this initial version, only a single
// swap-chain is supported. Follow up patches will add layer and external
// surface support.

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct CompositorHandle(usize);

#[repr(C)]
#[derive(Copy, Clone, PartialEq, Debug)]
struct LayerId(usize);

impl LayerId {
    const INVALID: LayerId = LayerId(0);
}

// FFI bindings to `composite.cpp`
#[link(name="wr_composite", kind="static")]
extern "C" {
    fn wrc_new(d3d11_device: *const c_void, hwnd: *const c_void) -> CompositorHandle;
    fn wrc_delete(compositor: CompositorHandle);

    fn wrc_create_layer(compositor: CompositorHandle, width: i32, height: i32) -> LayerId;
    fn wrc_get_layer_backbuffer(layer_id: LayerId) -> *mut c_void;
    fn wrc_present_layer(layer_id: LayerId);

    fn wrc_end_frame(compositor: CompositorHandle);
}

// A basic layer - we only create one primary layer in the initial commit
struct WrLayer {
    // Layer dimensions
    width: i32,
    height: i32,
    // Handle to the FFI layer
    layer_id: LayerId,
    // EGL surface that gets bound for webrender to draw to
    surface: EGLSurface,
}

impl WrLayer {
    fn empty() -> Self {
        WrLayer {
            width: 0,
            height: 0,
            layer_id: LayerId::INVALID,
            surface: ptr::null(),
        }
    }
}

// A basic `Compositor2` implementation for wrench
pub struct WrCompositor {
    // EGL display and content, provided by winit
    context: EGLContext,
    display: EGLDisplay,
    // FFI compositor handle
    compositor: CompositorHandle,
    // The main swapchain layer - in future we'll have a pool of layers here
    main_layer: WrLayer,
}

impl WrCompositor {
    pub fn new(window: &WindowWrapper) -> Self {
        // Retrieve the D3D11 device from winit - this was created when ANGLE was initialized
        let d3d11_device = window.get_d3d11_device();

        // Get the win32 and EGL information needed from the window
        let (hwnd, display, context) = match window {
            WindowWrapper::Angle(window, angle, _, _) => {
                (window.hwnd(), angle.get_display(), angle.get_context())
            }
            _ => unreachable!(),
        };

        // Construct the FFI part of the compositor impl
        let compositor = unsafe {
            wrc_new(d3d11_device, hwnd)
        };

        WrCompositor {
            display,
            context,
            compositor,
            main_layer: WrLayer::empty(),
        }
    }
}

impl Drop for WrCompositor {
    fn drop(&mut self) {
        unsafe {
            wrc_delete(self.compositor);
        }
    }
}

impl Compositor2 for WrCompositor {
    // Begin compositing a frame with the supplied input config
    // The main job of this method is to inspect the input config, create the output
    // config, and create any native OS resources that will be needed as layers get
    // composited.
    fn begin_frame(
        &mut self,
        input: &CompositorInputConfig,
    ) -> CompositorOutputConfig {
        let output = CompositorOutputConfig {
        };

        // See if we need to construct a primary swap-chain layer
        if self.main_layer.width != input.framebuffer_size.width ||
           self.main_layer.height != input.framebuffer_size.height {
            // TODO: Handle resize of layer (not needed for initial commit,
            // but will need to support resizing wrench window)
            assert_eq!(self.main_layer.layer_id, LayerId::INVALID);

            // Construct a DC swap-chain
            let layer_id = unsafe {
                wrc_create_layer(self.compositor, input.framebuffer_size.width, input.framebuffer_size.height)
            };

            let pbuffer_attribs: [EGLint; 5] = [
                egl::ffi::WIDTH as EGLint, input.framebuffer_size.width,
                egl::ffi::HEIGHT as EGLint, input.framebuffer_size.height,
                egl::ffi::NONE as EGLint,
            ];

            let attribs: [EGLint; 18] = [
                egl::ffi::SURFACE_TYPE as EGLint, egl::ffi::WINDOW_BIT as EGLint,
                egl::ffi::RED_SIZE as EGLint, 8,
                egl::ffi::GREEN_SIZE as EGLint, 8,
                egl::ffi::BLUE_SIZE as EGLint, 8,
                egl::ffi::ALPHA_SIZE as EGLint, 8,
                egl::ffi::RENDERABLE_TYPE as EGLint, egl::ffi::OPENGL_ES2_BIT as EGLint,
                // TODO(gw): Can we disable z-buffer for compositing always?
                egl::ffi::DEPTH_SIZE as EGLint, 24,
                egl::ffi::STENCIL_SIZE as EGLint, 8,
                egl::ffi::NONE as EGLint, egl::ffi::NONE as EGLint
            ];

            // Get the D3D backbuffer texture for the swap-chain
            let back_buffer = unsafe {
                wrc_get_layer_backbuffer(layer_id)
            };

            // Create an EGL surface <-> binding to the D3D backbuffer texture
            let mut egl_config = ptr::null();
            let mut cfg_count = 0;

            let ok = unsafe {
                egl::ffi::ChooseConfig(self.display, attribs.as_ptr(), &mut egl_config, 1, &mut cfg_count)
            };

            assert_ne!(ok, 0);
            assert_eq!(cfg_count, 1);
            assert_ne!(egl_config, ptr::null());

            let surface = unsafe {
                egl::ffi::CreatePbufferFromClientBuffer(
                    self.display,
                    egl::ffi::D3D_TEXTURE_ANGLE,
                    back_buffer,
                    egl_config,
                    pbuffer_attribs.as_ptr(),
                )
            };
            assert_ne!(surface, ptr::null());

            self.main_layer = WrLayer {
                width: input.framebuffer_size.width,
                height: input.framebuffer_size.height,
                layer_id,
                surface,
            };
        }

        // Bind the DC surface to EGL so that WR can composite to the layer
        let ok = unsafe {
            egl::ffi::MakeCurrent(
                self.display,
                self.main_layer.surface,
                self.main_layer.surface,
                self.context,
            )
        };
        assert!(ok != 0);

        output
    }

    // Finish compositing this frame
    fn end_frame(&mut self) {
        unsafe {
            // TODO(gw): Once multiple layers are supported, present gets called on each
            //           layer, as required, rather than during `end_frame`.
            wrc_present_layer(self.main_layer.layer_id);

            // Do any final commits to DC
            wrc_end_frame(self.compositor);
        }
    }
}

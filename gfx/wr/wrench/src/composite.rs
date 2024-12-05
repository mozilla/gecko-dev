/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::os::raw::c_void;
use std::ptr;

use glutin::platform::windows::EGLContext;
use webrender::{LayerCompositor, CompositorInputConfig};
use winit::platform::windows::WindowExtWindows;

use crate::WindowWrapper;

use mozangle::egl::ffi::types::{EGLDisplay, EGLSurface, EGLint};
use mozangle::egl;

// A simplistic implementation of the `LayerCompositor` trait to allow wrench to
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

    fn wrc_create_layer(compositor: CompositorHandle, width: i32, height: i32, is_opaque: bool) -> LayerId;
    fn wrc_get_layer_backbuffer(layer_id: LayerId) -> *mut c_void;
    fn wrc_set_layer_position(layer_id: LayerId, x: f32, y: f32);
    fn wrc_present_layer(layer_id: LayerId);
    fn wrc_add_layer(compositor: CompositorHandle, layer_id: LayerId);

    fn wrc_begin_frame(compositor: CompositorHandle);
    fn wrc_end_frame(compositor: CompositorHandle);
}

// A basic layer - we only create one primary layer in the initial commit
struct WrLayer {
    // Layer dimensions
    width: i32,
    height: i32,
    // EGL surface that gets bound for webrender to draw to
    surface: EGLSurface,
    // Handle to the FFI layer
    layer_id: LayerId,
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

// A basic `LayerCompositor` implementation for wrench
pub struct WrCompositor {
    // EGL display and content, provided by winit
    context: EGLContext,
    display: EGLDisplay,
    // FFI compositor handle
    compositor: CompositorHandle,
    // The swapchain layers needed for current scene
    layers: Vec<WrLayer>,
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
            layers: Vec::new(),
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

impl LayerCompositor for WrCompositor {
    // Begin compositing a frame with the supplied input config
    // The main job of this method is to inspect the input config, create the output
    // config, and create any native OS resources that will be needed as layers get
    // composited.
    fn begin_frame(
        &mut self,
        input: &CompositorInputConfig,
    ) {
        unsafe {
            // Reset DC visual tree
            wrc_begin_frame(self.compositor);
        }

        let prev_layer_count = self.layers.len();
        let curr_layer_count = input.layers.len();

        if prev_layer_count > curr_layer_count {
            todo!();
        } else if curr_layer_count > prev_layer_count {
            // Construct new empty layers, they'll get resized below
            for _ in 0 .. curr_layer_count-prev_layer_count {
                self.layers.push(WrLayer::empty());
            }
        }

        assert_eq!(self.layers.len(), input.layers.len());

        for (input, layer) in input.layers.iter().zip(self.layers.iter_mut()) {
            let input_size = input.rect.size();

            // TODO(gwc): Handle External surfaces as swapchains separate from content

            // See if we need to resize the swap-chain layer
            if input_size.width != layer.width ||
                input_size.height != layer.height {
                // TODO: Handle resize of layer (not needed for initial commit,
                // but will need to support resizing wrench window)
                assert_eq!(layer.layer_id, LayerId::INVALID);

                // Construct a DC swap-chain
                layer.width = input_size.width;
                layer.height = input_size.height;
                layer.layer_id = unsafe {
                    wrc_create_layer(self.compositor, layer.width, layer.height, input.is_opaque)
                };

                let pbuffer_attribs: [EGLint; 5] = [
                    egl::ffi::WIDTH as EGLint, layer.width,
                    egl::ffi::HEIGHT as EGLint, layer.height,
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
                    wrc_get_layer_backbuffer(layer.layer_id)
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

                layer.surface = surface;
            }

            unsafe {
                wrc_set_layer_position(
                    layer.layer_id,
                    input.rect.min.x as f32,
                    input.rect.min.y as f32,
                );
            }
        }
    }

    // Bind a layer by index for compositing into
    fn bind_layer(&mut self, index: usize) {
        // Bind the DC surface to EGL so that WR can composite to the layer
        let layer = &self.layers[index];

        let ok = unsafe {
            egl::ffi::MakeCurrent(
                self.display,
                layer.surface,
                layer.surface,
                self.context,
            )
        };
        assert!(ok != 0);
    }

    // Finish compositing a layer and present the swapchain
    fn present_layer(&mut self, index: usize) {
        let layer = &self.layers[index];

        unsafe {
            wrc_present_layer(layer.layer_id);
        }
    }

    fn add_surface(
            &mut self,
            index: usize,
            _clip_rect: webrender::api::units::DeviceIntRect,
            _image_rendering: webrender::api::ImageRendering,
        ) {
        let layer = &self.layers[index];

        unsafe {
            wrc_add_layer(self.compositor, layer.layer_id);
        }
    }

    // Finish compositing this frame
    fn end_frame(&mut self) {
        unsafe {
            // Do any final commits to DC
            wrc_end_frame(self.compositor);
        }
    }
}

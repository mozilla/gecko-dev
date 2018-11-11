/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*!
A GPU based renderer for the web.

It serves as an experimental render backend for [Servo](https://servo.org/),
but it can also be used as such in a standalone application.

# External dependencies
WebRender currently depends on [FreeType](https://www.freetype.org/)

# Api Structure
The main entry point to WebRender is the `webrender::Renderer`.

By calling `Renderer::new(...)` you get a `Renderer`, as well as a `RenderApiSender`.
Your `Renderer` is responsible to render the previously processed frames onto the screen.

By calling `yourRenderApiSender.create_api()`, you'll get a `RenderApi` instance,
which is responsible for managing resources and documents. A worker thread is used internally
to untie the workload from the application thread and therefore be able to make better use of
multicore systems.

## Frame

What is referred to as a `frame`, is the current geometry on the screen.
A new Frame is created by calling [`set_display_list()`][newframe] on the `RenderApi`.
When the geometry is processed, the application will be informed via a `RenderNotifier`,
a callback which you employ with [set_render_notifier][notifier] on the `Renderer`
More information about [stacking contexts][stacking_contexts].

`set_display_list()` also needs to be supplied with `BuiltDisplayList`s.
These are obtained by finalizing a `DisplayListBuilder`. These are used to draw your geometry.
But it doesn't only contain trivial geometry, it can also store another StackingContext, as
they're nestable.

[stacking_contexts]: https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Positioning/Understanding_z_index/The_stacking_context
[newframe]: ../webrender_api/struct.RenderApi.html#method.set_display_list
[notifier]: renderer/struct.Renderer.html#method.set_render_notifier
*/

// Cribbed from the |matches| crate, for simplicity.
macro_rules! matches {
    ($expression:expr, $($pattern:tt)+) => {
        match $expression {
            $($pattern)+ => true,
            _ => false
        }
    }
}

#[macro_use]
extern crate bitflags;
#[macro_use]
extern crate cfg_if;
#[macro_use]
extern crate lazy_static;
#[macro_use]
extern crate log;
#[cfg(any(feature = "serde"))]
#[macro_use]
extern crate serde;
#[macro_use]
extern crate thread_profiler;

mod batch;
mod border;
mod box_shadow;
#[cfg(any(feature = "capture", feature = "replay"))]
mod capture;
mod clip;
mod clip_scroll_tree;
mod debug_colors;
#[cfg(feature = "debug_renderer")]
mod debug_font_data;
#[cfg(feature = "debug_renderer")]
mod debug_render;
#[cfg(feature = "debugger")]
mod debug_server;
mod device;
mod display_list_flattener;
mod ellipse;
mod frame_builder;
mod freelist;
#[cfg(any(target_os = "macos", target_os = "windows"))]
mod gamma_lut;
mod glyph_cache;
mod glyph_rasterizer;
mod gpu_cache;
#[cfg(feature = "pathfinder")]
mod gpu_glyph_renderer;
mod gpu_types;
mod hit_test;
mod image;
mod intern;
mod internal_types;
mod picture;
mod prim_store;
mod print_tree;
mod profiler;
mod record;
mod render_backend;
mod render_task;
mod renderer;
mod resource_cache;
mod scene;
mod scene_builder;
mod segment;
mod shade;
mod spatial_node;
mod surface;
mod texture_allocator;
mod texture_cache;
mod tiling;
mod util;

mod shader_source {
    include!(concat!(env!("OUT_DIR"), "/shaders.rs"));
}

pub use record::{ApiRecordingReceiver, BinaryRecorder, WEBRENDER_RECORDING_HEADER};

mod platform {
    #[cfg(target_os = "macos")]
    pub use platform::macos::font;
    #[cfg(any(target_os = "android", all(unix, not(target_os = "macos"))))]
    pub use platform::unix::font;
    #[cfg(target_os = "windows")]
    pub use platform::windows::font;

    #[cfg(target_os = "macos")]
    pub mod macos {
        pub mod font;
    }
    #[cfg(any(target_os = "android", all(unix, not(target_os = "macos"))))]
    pub mod unix {
        pub mod font;
    }
    #[cfg(target_os = "windows")]
    pub mod windows {
        pub mod font;
    }
}

#[cfg(target_os = "macos")]
extern crate core_foundation;
#[cfg(target_os = "macos")]
extern crate core_graphics;
#[cfg(target_os = "macos")]
extern crate core_text;

#[cfg(all(unix, not(target_os = "macos")))]
extern crate freetype;

#[cfg(target_os = "windows")]
extern crate dwrote;

extern crate app_units;
extern crate bincode;
extern crate byteorder;
extern crate euclid;
extern crate fxhash;
extern crate gleam;
extern crate num_traits;
#[cfg(feature = "pathfinder")]
extern crate pathfinder_font_renderer;
#[cfg(feature = "pathfinder")]
extern crate pathfinder_gfx_utils;
#[cfg(feature = "pathfinder")]
extern crate pathfinder_partitioner;
#[cfg(feature = "pathfinder")]
extern crate pathfinder_path_utils;
extern crate plane_split;
extern crate rayon;
#[cfg(feature = "ron")]
extern crate ron;
#[cfg(feature = "debugger")]
extern crate serde_json;
extern crate smallvec;
extern crate time;
#[cfg(feature = "debugger")]
extern crate ws;
#[cfg(feature = "debugger")]
extern crate image as image_loader;
#[cfg(feature = "debugger")]
extern crate base64;
#[cfg(all(feature = "capture", feature = "png"))]
extern crate png;

pub extern crate webrender_api;

#[doc(hidden)]
pub use device::{build_shader_strings, ReadPixelsFormat, UploadMethod, VertexUsageHint};
pub use device::{ProgramBinary, ProgramCache, ProgramCacheObserver, ProgramSources};
pub use device::{Device, total_gpu_bytes_allocated};
pub use frame_builder::ChasePrimitive;
pub use renderer::{AsyncPropertySampler, CpuProfile, DebugFlags, OutputImageHandler, RendererKind};
pub use renderer::{ExternalImage, ExternalImageHandler, ExternalImageSource, GpuProfile};
pub use renderer::{GraphicsApi, GraphicsApiInfo, PipelineInfo, Renderer, RendererOptions};
pub use renderer::{RendererStats, SceneBuilderHooks, ThreadListener, ShaderPrecacheFlags};
pub use renderer::MAX_VERTEX_TEXTURE_WIDTH;
pub use shade::{Shaders, WrShaders};
pub use webrender_api as api;
pub use resource_cache::intersect_for_tile;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, BorderStyle, DeviceIntPoint, DeviceIntRect, DeviceIntSize, DevicePixelScale};
use api::{DocumentLayer, FilterOp, ImageFormat};
use api::{MixBlendMode, PipelineId, DeviceRect, LayoutSize};
use batch::{AlphaBatchBuilder, AlphaBatchContainer, ClipBatcher, resolve_image};
use clip::ClipStore;
use clip_scroll_tree::{ClipScrollTree};
use device::{Texture};
#[cfg(feature = "pathfinder")]
use euclid::{TypedPoint2D, TypedVector2D};
use gpu_cache::{GpuCache};
use gpu_types::{BorderInstance, BlurDirection, BlurInstance, PrimitiveHeaders, ScalingInstance};
use gpu_types::{TransformData, TransformPalette, ZBufferIdGenerator};
use internal_types::{CacheTextureId, FastHashMap, SavedTargetIndex, TextureSource};
#[cfg(feature = "pathfinder")]
use pathfinder_partitioner::mesh::Mesh;
use picture::SurfaceInfo;
use prim_store::{PrimitiveStore, DeferredResolve, PrimitiveScratchBuffer};
use profiler::FrameProfileCounters;
use render_backend::{FrameId, FrameResources};
use render_task::{BlitSource, RenderTaskAddress, RenderTaskId, RenderTaskKind, TileBlit};
use render_task::{BlurTask, ClearMode, GlyphTask, RenderTaskLocation, RenderTaskTree, ScalingTask};
use resource_cache::ResourceCache;
use std::{cmp, usize, f32, i32, mem};
use texture_allocator::{ArrayAllocationTracker, FreeRectSlice};
#[cfg(feature = "pathfinder")]
use webrender_api::{DevicePixel, FontRenderMode};

const STYLE_SOLID: i32 = ((BorderStyle::Solid as i32) << 8) | ((BorderStyle::Solid as i32) << 16);
const STYLE_MASK: i32 = 0x00FF_FF00;

/// According to apitrace, textures larger than 2048 break fast clear
/// optimizations on some intel drivers. We sometimes need to go larger, but
/// we try to avoid it. This can go away when proper tiling support lands,
/// since we can then split large primitives across multiple textures.
const IDEAL_MAX_TEXTURE_DIMENSION: i32 = 2048;
/// If we ever need a larger texture than the ideal, we better round it up to a
/// reasonable number in order to have a bit of leeway in placing things inside.
const TEXTURE_DIMENSION_MASK: i32 = 0xFF;

/// Identifies a given `RenderTarget` in a `RenderTargetList`.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTargetIndex(pub usize);

pub struct RenderTargetContext<'a, 'rc> {
    pub device_pixel_scale: DevicePixelScale,
    pub prim_store: &'a PrimitiveStore,
    pub resource_cache: &'rc mut ResourceCache,
    pub use_dual_source_blending: bool,
    pub clip_scroll_tree: &'a ClipScrollTree,
    pub resources: &'a FrameResources,
    pub surfaces: &'a [SurfaceInfo],
    pub scratch: &'a PrimitiveScratchBuffer,
}

/// Represents a number of rendering operations on a surface.
///
/// In graphics parlance, a "render target" usually means "a surface (texture or
/// framebuffer) bound to the output of a shader". This trait has a slightly
/// different meaning, in that it represents the operations on that surface
/// _before_ it's actually bound and rendered. So a `RenderTarget` is built by
/// the `RenderBackend` by inserting tasks, and then shipped over to the
/// `Renderer` where a device surface is resolved and the tasks are transformed
/// into draw commands on that surface.
///
/// We express this as a trait to generalize over color and alpha surfaces.
/// a given `RenderTask` will draw to one or the other, depending on its type
/// and sometimes on its parameters. See `RenderTask::target_kind`.
pub trait RenderTarget {
    /// Creates a new RenderTarget of the given type.
    fn new(screen_size: DeviceIntSize) -> Self;

    /// Optional hook to provide additional processing for the target at the
    /// end of the build phase.
    fn build(
        &mut self,
        _ctx: &mut RenderTargetContext,
        _gpu_cache: &mut GpuCache,
        _render_tasks: &mut RenderTaskTree,
        _deferred_resolves: &mut Vec<DeferredResolve>,
        _prim_headers: &mut PrimitiveHeaders,
        _transforms: &mut TransformPalette,
        _z_generator: &mut ZBufferIdGenerator,
    ) {
    }

    /// Associates a `RenderTask` with this target. That task must be assigned
    /// to a region returned by invoking `allocate()` on this target.
    ///
    /// TODO(gw): It's a bit odd that we need the deferred resolves and mutable
    /// GPU cache here. They are typically used by the build step above. They
    /// are used for the blit jobs to allow resolve_image to be called. It's a
    /// bit of extra overhead to store the image key here and the resolve them
    /// in the build step separately.  BUT: if/when we add more texture cache
    /// target jobs, we might want to tidy this up.
    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
        transforms: &mut TransformPalette,
        deferred_resolves: &mut Vec<DeferredResolve>,
    );

    fn needs_depth(&self) -> bool;

    fn used_rect(&self) -> DeviceIntRect;
    fn add_used(&mut self, rect: DeviceIntRect);
}

/// A tag used to identify the output format of a `RenderTarget`.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTargetKind {
    Color, // RGBA8
    Alpha, // R8
}

/// A series of `RenderTarget` instances, serving as the high-level container
/// into which `RenderTasks` are assigned.
///
/// During the build phase, we iterate over the tasks in each `RenderPass`. For
/// each task, we invoke `allocate()` on the `RenderTargetList`, which in turn
/// attempts to allocate an output region in the last `RenderTarget` in the
/// list. If allocation fails (or if the list is empty), a new `RenderTarget` is
/// created and appended to the list. The build phase then assign the task into
/// the target associated with the final allocation.
///
/// The result is that each `RenderPass` is associated with one or two
/// `RenderTargetLists`, depending on whether we have all our tasks have the
/// same `RenderTargetKind`. The lists are then shipped to the `Renderer`, which
/// allocates a device texture array, with one slice per render target in the
/// list.
///
/// The upshot of this scheme is that it maximizes batching. In a given pass,
/// we need to do a separate batch for each individual render target. But with
/// the texture array, we can expose the entirety of the previous pass to each
/// task in the current pass in a single batch, which generally allows each
/// task to be drawn in a single batch regardless of how many results from the
/// previous pass it depends on.
///
/// Note that in some cases (like drop-shadows), we can depend on the output of
/// a pass earlier than the immediately-preceding pass. See `SavedTargetIndex`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTargetList<T> {
    screen_size: DeviceIntSize,
    pub format: ImageFormat,
    /// The maximum width and height of any single primitive we've encountered
    /// that will be drawn to a dynamic location.
    ///
    /// We initially create our per-slice allocators with a width and height of
    /// IDEAL_MAX_TEXTURE_DIMENSION. If we encounter a larger primitive, the
    /// allocation will fail, but we'll bump max_dynamic_size, which will cause the
    /// allocator for the next slice to be just large enough to accomodate it.
    pub max_dynamic_size: DeviceIntSize,
    pub targets: Vec<T>,
    pub saved_index: Option<SavedTargetIndex>,
    pub alloc_tracker: ArrayAllocationTracker,
}

impl<T: RenderTarget> RenderTargetList<T> {
    fn new(
        screen_size: DeviceIntSize,
        format: ImageFormat,
    ) -> Self {
        RenderTargetList {
            screen_size,
            format,
            max_dynamic_size: DeviceIntSize::new(0, 0),
            targets: Vec::new(),
            saved_index: None,
            alloc_tracker: ArrayAllocationTracker::new(),
        }
    }

    fn build(
        &mut self,
        ctx: &mut RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
        saved_index: Option<SavedTargetIndex>,
        prim_headers: &mut PrimitiveHeaders,
        transforms: &mut TransformPalette,
        z_generator: &mut ZBufferIdGenerator,
    ) {
        debug_assert_eq!(None, self.saved_index);
        self.saved_index = saved_index;

        for target in &mut self.targets {
            target.build(
                ctx,
                gpu_cache,
                render_tasks,
                deferred_resolves,
                prim_headers,
                transforms,
                z_generator,
            );
        }
    }

    fn allocate(
        &mut self,
        alloc_size: DeviceIntSize,
    ) -> (RenderTargetIndex, DeviceIntPoint) {
        let (free_rect_slice, origin) = match self.alloc_tracker.allocate(&alloc_size) {
            Some(allocation) => allocation,
            None => {
                // Have the allocator restrict slice sizes to our max ideal
                // dimensions, unless we've already gone bigger on a previous
                // slice.
                let rounded_dimensions = DeviceIntSize::new(
                    (self.max_dynamic_size.width + TEXTURE_DIMENSION_MASK) & !TEXTURE_DIMENSION_MASK,
                    (self.max_dynamic_size.height + TEXTURE_DIMENSION_MASK) & !TEXTURE_DIMENSION_MASK,
                );
                let allocator_dimensions = DeviceIntSize::new(
                    cmp::max(IDEAL_MAX_TEXTURE_DIMENSION, rounded_dimensions.width),
                    cmp::max(IDEAL_MAX_TEXTURE_DIMENSION, rounded_dimensions.height),
                );

                assert!(alloc_size.width <= allocator_dimensions.width &&
                    alloc_size.height <= allocator_dimensions.height);
                let slice = FreeRectSlice(self.targets.len() as u32);
                self.targets.push(T::new(self.screen_size));

                self.alloc_tracker.extend(
                    slice,
                    allocator_dimensions,
                    alloc_size,
                );

                (slice, DeviceIntPoint::zero())
            }
        };

        self.targets[free_rect_slice.0 as usize]
            .add_used(DeviceIntRect::new(origin, alloc_size));

        (RenderTargetIndex(free_rect_slice.0 as usize), origin)
    }

    pub fn needs_depth(&self) -> bool {
        self.targets.iter().any(|target| target.needs_depth())
    }

    pub fn check_ready(&self, t: &Texture) {
        let dimensions = t.get_dimensions();
        assert!(dimensions.width >= self.max_dynamic_size.width);
        assert!(dimensions.height >= self.max_dynamic_size.height);
        assert_eq!(t.get_format(), self.format);
        assert_eq!(t.get_layer_count() as usize, self.targets.len());
        assert!(t.supports_depth() >= self.needs_depth());
    }
}

/// Frame output information for a given pipeline ID.
/// Storing the task ID allows the renderer to find
/// the target rect within the render target that this
/// pipeline exists at.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameOutput {
    pub task_id: RenderTaskId,
    pub pipeline_id: PipelineId,
}

// Defines where the source data for a blit job can be found.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum BlitJobSource {
    Texture(TextureSource, i32, DeviceIntRect),
    RenderTask(RenderTaskId),
}

// Information required to do a blit from a source to a target.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlitJob {
    pub source: BlitJobSource,
    pub target_rect: DeviceIntRect,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct LineDecorationJob {
    pub task_rect: DeviceRect,
    pub local_size: LayoutSize,
    pub wavy_line_thickness: f32,
    pub style: i32,
    pub orientation: i32,
}

#[cfg(feature = "pathfinder")]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GlyphJob {
    pub mesh: Mesh,
    pub target_rect: DeviceIntRect,
    pub origin: DeviceIntPoint,
    pub subpixel_offset: TypedPoint2D<f32, DevicePixel>,
    pub render_mode: FontRenderMode,
    pub embolden_amount: TypedVector2D<f32, DevicePixel>,
}

#[cfg(not(feature = "pathfinder"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GlyphJob;

/// Contains the work (in the form of instance arrays) needed to fill a color
/// color output surface (RGBA8).
///
/// See `RenderTarget`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ColorRenderTarget {
    pub alpha_batch_containers: Vec<AlphaBatchContainer>,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub readbacks: Vec<DeviceIntRect>,
    pub scalings: Vec<ScalingInstance>,
    pub blits: Vec<BlitJob>,
    // List of frame buffer outputs for this render target.
    pub outputs: Vec<FrameOutput>,
    pub tile_blits: Vec<TileBlit>,
    pub color_clears: Vec<RenderTaskId>,
    alpha_tasks: Vec<RenderTaskId>,
    screen_size: DeviceIntSize,
    // Track the used rect of the render target, so that
    // we can set a scissor rect and only clear to the
    // used portion of the target as an optimization.
    pub used_rect: DeviceIntRect,
}

impl RenderTarget for ColorRenderTarget {
    fn new(screen_size: DeviceIntSize) -> Self {
        ColorRenderTarget {
            alpha_batch_containers: Vec::new(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            readbacks: Vec::new(),
            scalings: Vec::new(),
            blits: Vec::new(),
            outputs: Vec::new(),
            alpha_tasks: Vec::new(),
            color_clears: Vec::new(),
            tile_blits: Vec::new(),
            screen_size,
            used_rect: DeviceIntRect::zero(),
        }
    }

    fn build(
        &mut self,
        ctx: &mut RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
        prim_headers: &mut PrimitiveHeaders,
        transforms: &mut TransformPalette,
        z_generator: &mut ZBufferIdGenerator,
    ) {
        let mut merged_batches = AlphaBatchContainer::new(None);

        for task_id in &self.alpha_tasks {
            let task = &render_tasks[*task_id];

            match task.clear_mode {
                ClearMode::One |
                ClearMode::Zero => {
                    panic!("bug: invalid clear mode for color task");
                }
                ClearMode::Transparent => {}
                ClearMode::Color(..) => {
                    self.color_clears.push(*task_id);
                }
            }

            match task.kind {
                RenderTaskKind::Picture(ref pic_task) => {
                    let pic = &ctx.prim_store.pictures[pic_task.pic_index.0];

                    let (target_rect, _) = task.get_target_rect();

                    let mut batch_builder = AlphaBatchBuilder::new(
                        self.screen_size,
                        target_rect,
                        pic_task.can_merge,
                    );

                    batch_builder.add_pic_to_batch(
                        pic,
                        *task_id,
                        ctx,
                        gpu_cache,
                        render_tasks,
                        deferred_resolves,
                        prim_headers,
                        transforms,
                        pic_task.root_spatial_node_index,
                        z_generator,
                    );

                    for blit in &pic_task.blits {
                        self.tile_blits.push(TileBlit {
                            target: blit.target.clone(),
                            offset: DeviceIntPoint::new(
                                blit.offset.x + target_rect.origin.x,
                                blit.offset.y + target_rect.origin.y,
                            ),
                        })
                    }

                    if let Some(batch_container) = batch_builder.build(&mut merged_batches) {
                        self.alpha_batch_containers.push(batch_container);
                    }
                }
                _ => {
                    unreachable!();
                }
            }
        }

        self.alpha_batch_containers.push(merged_batches);
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        _: &ClipStore,
        _: &mut TransformPalette,
        deferred_resolves: &mut Vec<DeferredResolve>,
    ) {
        let task = &render_tasks[task_id];

        match task.kind {
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    BlurDirection::Vertical,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    BlurDirection::Horizontal,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::Picture(ref task_info) => {
                let pic = &ctx.prim_store.pictures[task_info.pic_index.0];
                self.alpha_tasks.push(task_id);

                // If this pipeline is registered as a frame output
                // store the information necessary to do the copy.
                if let Some(pipeline_id) = pic.frame_output_pipeline_id {
                    self.outputs.push(FrameOutput {
                        pipeline_id,
                        task_id,
                    });
                }
            }
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::LineDecoration(..) => {
                panic!("Should not be added to color target!");
            }
            RenderTaskKind::Glyph(..) => {
                // FIXME(pcwalton): Support color glyphs.
                panic!("Glyphs should not be added to color target!");
            }
            RenderTaskKind::Readback(device_rect) => {
                self.readbacks.push(device_rect);
            }
            RenderTaskKind::Scaling(..) => {
                self.scalings.push(ScalingInstance {
                    task_address: render_tasks.get_task_address(task_id),
                    src_task_address: render_tasks.get_task_address(task.children[0]),
                });
            }
            RenderTaskKind::Blit(ref task_info) => {
                match task_info.source {
                    BlitSource::Image { key } => {
                        // Get the cache item for the source texture.
                        let cache_item = resolve_image(
                            key.request,
                            ctx.resource_cache,
                            gpu_cache,
                            deferred_resolves,
                        );

                        // Work out a source rect to copy from the texture, depending on whether
                        // a sub-rect is present or not.
                        let source_rect = key.texel_rect.map_or(cache_item.uv_rect.to_i32(), |sub_rect| {
                            DeviceIntRect::new(
                                DeviceIntPoint::new(
                                    cache_item.uv_rect.origin.x as i32 + sub_rect.origin.x,
                                    cache_item.uv_rect.origin.y as i32 + sub_rect.origin.y,
                                ),
                                sub_rect.size,
                            )
                        });

                        // Store the blit job for the renderer to execute, including
                        // the allocated destination rect within this target.
                        let (target_rect, _) = task.get_target_rect();
                        self.blits.push(BlitJob {
                            source: BlitJobSource::Texture(
                                cache_item.texture_id,
                                cache_item.texture_layer,
                                source_rect,
                            ),
                            target_rect: target_rect.inner_rect(task_info.padding)
                        });
                    }
                    BlitSource::RenderTask { .. } => {
                        panic!("BUG: render task blit jobs to render tasks not supported");
                    }
                }
            }
        }
    }

    fn needs_depth(&self) -> bool {
        self.alpha_batch_containers.iter().any(|ab| {
            !ab.opaque_batches.is_empty()
        })
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.used_rect
    }

    fn add_used(&mut self, rect: DeviceIntRect) {
        self.used_rect = self.used_rect.union(&rect);
    }
}

/// Contains the work (in the form of instance arrays) needed to fill an alpha
/// output surface (R8).
///
/// See `RenderTarget`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct AlphaRenderTarget {
    pub clip_batcher: ClipBatcher,
    // List of blur operations to apply for this render target.
    pub vertical_blurs: Vec<BlurInstance>,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub scalings: Vec<ScalingInstance>,
    pub zero_clears: Vec<RenderTaskId>,
    // Track the used rect of the render target, so that
    // we can set a scissor rect and only clear to the
    // used portion of the target as an optimization.
    pub used_rect: DeviceIntRect,
}

impl RenderTarget for AlphaRenderTarget {
    fn new(_screen_size: DeviceIntSize) -> Self {
        AlphaRenderTarget {
            clip_batcher: ClipBatcher::new(),
            vertical_blurs: Vec::new(),
            horizontal_blurs: Vec::new(),
            scalings: Vec::new(),
            zero_clears: Vec::new(),
            used_rect: DeviceIntRect::zero(),
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        ctx: &RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &RenderTaskTree,
        clip_store: &ClipStore,
        transforms: &mut TransformPalette,
        _: &mut Vec<DeferredResolve>,
    ) {
        let task = &render_tasks[task_id];

        match task.clear_mode {
            ClearMode::Zero => {
                self.zero_clears.push(task_id);
            }
            ClearMode::One => {}
            ClearMode::Color(..) |
            ClearMode::Transparent => {
                panic!("bug: invalid clear mode for alpha task");
            }
        }

        match task.kind {
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Glyph(..) => {
                panic!("BUG: should not be added to alpha target!");
            }
            RenderTaskKind::VerticalBlur(ref info) => {
                info.add_instances(
                    &mut self.vertical_blurs,
                    BlurDirection::Vertical,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    BlurDirection::Horizontal,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
            RenderTaskKind::CacheMask(ref task_info) => {
                let task_address = render_tasks.get_task_address(task_id);
                self.clip_batcher.add(
                    task_address,
                    task_info.clip_node_range,
                    task_info.root_spatial_node_index,
                    ctx.resource_cache,
                    gpu_cache,
                    clip_store,
                    ctx.clip_scroll_tree,
                    transforms,
                    &ctx.resources.clip_data_store,
                );
            }
            RenderTaskKind::ClipRegion(ref task) => {
                let task_address = render_tasks.get_task_address(task_id);
                self.clip_batcher.add_clip_region(
                    task_address,
                    task.clip_data_address,
                );
            }
            RenderTaskKind::Scaling(ref info) => {
                info.add_instances(
                    &mut self.scalings,
                    render_tasks.get_task_address(task_id),
                    render_tasks.get_task_address(task.children[0]),
                );
            }
        }
    }

    fn needs_depth(&self) -> bool {
        false
    }

    fn used_rect(&self) -> DeviceIntRect {
        self.used_rect
    }

    fn add_used(&mut self, rect: DeviceIntRect) {
        self.used_rect = self.used_rect.union(&rect);
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextureCacheRenderTarget {
    pub target_kind: RenderTargetKind,
    pub horizontal_blurs: Vec<BlurInstance>,
    pub blits: Vec<BlitJob>,
    pub glyphs: Vec<GlyphJob>,
    pub border_segments_complex: Vec<BorderInstance>,
    pub border_segments_solid: Vec<BorderInstance>,
    pub clears: Vec<DeviceIntRect>,
    pub line_decorations: Vec<LineDecorationJob>,
}

impl TextureCacheRenderTarget {
    fn new(target_kind: RenderTargetKind) -> Self {
        TextureCacheRenderTarget {
            target_kind,
            horizontal_blurs: vec![],
            blits: vec![],
            glyphs: vec![],
            border_segments_complex: vec![],
            border_segments_solid: vec![],
            clears: vec![],
            line_decorations: vec![],
        }
    }

    fn add_task(
        &mut self,
        task_id: RenderTaskId,
        render_tasks: &mut RenderTaskTree,
    ) {
        let task_address = render_tasks.get_task_address(task_id);
        let src_task_address = render_tasks[task_id].children.get(0).map(|src_task_id| {
            render_tasks.get_task_address(*src_task_id)
        });

        let task = &mut render_tasks[task_id];
        let target_rect = task.get_target_rect();

        match task.kind {
            RenderTaskKind::LineDecoration(ref info) => {
                self.clears.push(target_rect.0);

                self.line_decorations.push(LineDecorationJob {
                    task_rect: target_rect.0.to_f32(),
                    local_size: info.local_size,
                    style: info.style as i32,
                    orientation: info.orientation as i32,
                    wavy_line_thickness: info.wavy_line_thickness,
                });
            }
            RenderTaskKind::HorizontalBlur(ref info) => {
                info.add_instances(
                    &mut self.horizontal_blurs,
                    BlurDirection::Horizontal,
                    task_address,
                    src_task_address.unwrap(),
                );
            }
            RenderTaskKind::Blit(ref task_info) => {
                match task_info.source {
                    BlitSource::Image { .. } => {
                        // reading/writing from the texture cache at the same time
                        // is undefined behavior.
                        panic!("bug: a single blit cannot be to/from texture cache");
                    }
                    BlitSource::RenderTask { task_id } => {
                        // Add a blit job to copy from an existing render
                        // task to this target.
                        self.blits.push(BlitJob {
                            source: BlitJobSource::RenderTask(task_id),
                            target_rect: target_rect.0.inner_rect(task_info.padding),
                        });
                    }
                }
            }
            RenderTaskKind::Border(ref mut task_info) => {
                self.clears.push(target_rect.0);

                let task_origin = target_rect.0.origin.to_f32();
                let instances = mem::replace(&mut task_info.instances, Vec::new());
                for mut instance in instances {
                    // TODO(gw): It may be better to store the task origin in
                    //           the render task data instead of per instance.
                    instance.task_origin = task_origin;
                    if instance.flags & STYLE_MASK == STYLE_SOLID {
                        self.border_segments_solid.push(instance);
                    } else {
                        self.border_segments_complex.push(instance);
                    }
                }
            }
            RenderTaskKind::Glyph(ref mut task_info) => {
                self.add_glyph_task(task_info, target_rect.0)
            }
            RenderTaskKind::VerticalBlur(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) => {
                panic!("BUG: unexpected task kind for texture cache target");
            }
        }
    }

    #[cfg(feature = "pathfinder")]
    fn add_glyph_task(&mut self, task_info: &mut GlyphTask, target_rect: DeviceIntRect) {
        self.glyphs.push(GlyphJob {
            mesh: task_info.mesh.take().unwrap(),
            target_rect: target_rect,
            origin: task_info.origin,
            subpixel_offset: task_info.subpixel_offset,
            render_mode: task_info.render_mode,
            embolden_amount: task_info.embolden_amount,
        })
    }

    #[cfg(not(feature = "pathfinder"))]
    fn add_glyph_task(&mut self, _: &mut GlyphTask, _: DeviceIntRect) {}
}

/// Contains the set of `RenderTarget`s specific to the kind of pass.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderPassKind {
    /// The final pass to the main frame buffer, where we have a single color
    /// target for display to the user.
    MainFramebuffer(ColorRenderTarget),
    /// An intermediate pass, where we may have multiple targets.
    OffScreen {
        alpha: RenderTargetList<AlphaRenderTarget>,
        color: RenderTargetList<ColorRenderTarget>,
        texture_cache: FastHashMap<(CacheTextureId, usize), TextureCacheRenderTarget>,
    },
}

/// A render pass represents a set of rendering operations that don't depend on one
/// another.
///
/// A render pass can have several render targets if there wasn't enough space in one
/// target to do all of the rendering for that pass. See `RenderTargetList`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderPass {
    /// The kind of pass, as well as the set of targets associated with that
    /// kind of pass.
    pub kind: RenderPassKind,
    /// The set of tasks to be performed in this pass, as indices into the
    /// `RenderTaskTree`.
    tasks: Vec<RenderTaskId>,
}

impl RenderPass {
    /// Creates a pass for the main framebuffer. There is only one of these, and
    /// it is always the last pass.
    pub fn new_main_framebuffer(screen_size: DeviceIntSize) -> Self {
        let target = ColorRenderTarget::new(screen_size);
        RenderPass {
            kind: RenderPassKind::MainFramebuffer(target),
            tasks: vec![],
        }
    }

    /// Creates an intermediate off-screen pass.
    pub fn new_off_screen(screen_size: DeviceIntSize) -> Self {
        RenderPass {
            kind: RenderPassKind::OffScreen {
                color: RenderTargetList::new(screen_size, ImageFormat::BGRA8),
                alpha: RenderTargetList::new(screen_size, ImageFormat::R8),
                texture_cache: FastHashMap::default(),
            },
            tasks: vec![],
        }
    }

    /// Adds a task to this pass.
    pub fn add_render_task(
        &mut self,
        task_id: RenderTaskId,
        size: DeviceIntSize,
        target_kind: RenderTargetKind,
        location: &RenderTaskLocation,
    ) {
        if let RenderPassKind::OffScreen { ref mut color, ref mut alpha, .. } = self.kind {
            // If this will be rendered to a dynamically-allocated region on an
            // off-screen render target, update the max-encountered size. We don't
            // need to do this for things drawn to the texture cache, since those
            // don't affect our render target allocation.
            if location.is_dynamic() {
                let max_size = match target_kind {
                    RenderTargetKind::Color => &mut color.max_dynamic_size,
                    RenderTargetKind::Alpha => &mut alpha.max_dynamic_size,
                };
                max_size.width = cmp::max(max_size.width, size.width);
                max_size.height = cmp::max(max_size.height, size.height);
            }
        }

        self.tasks.push(task_id);
    }

    /// Processes this pass to prepare it for rendering.
    ///
    /// Among other things, this allocates output regions for each of our tasks
    /// (added via `add_render_task`) in a RenderTarget and assigns it into that
    /// target.
    pub fn build(
        &mut self,
        ctx: &mut RenderTargetContext,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        deferred_resolves: &mut Vec<DeferredResolve>,
        clip_store: &ClipStore,
        transforms: &mut TransformPalette,
        prim_headers: &mut PrimitiveHeaders,
        z_generator: &mut ZBufferIdGenerator,
    ) {
        profile_scope!("RenderPass::build");

        match self.kind {
            RenderPassKind::MainFramebuffer(ref mut target) => {
                for &task_id in &self.tasks {
                    assert_eq!(render_tasks[task_id].target_kind(), RenderTargetKind::Color);
                    target.add_task(
                        task_id,
                        ctx,
                        gpu_cache,
                        render_tasks,
                        clip_store,
                        transforms,
                        deferred_resolves,
                    );
                }
                target.build(
                    ctx,
                    gpu_cache,
                    render_tasks,
                    deferred_resolves,
                    prim_headers,
                    transforms,
                    z_generator,
                );
            }
            RenderPassKind::OffScreen { ref mut color, ref mut alpha, ref mut texture_cache } => {
                let saved_color = if self.tasks.iter().any(|&task_id| {
                    let t = &render_tasks[task_id];
                    t.target_kind() == RenderTargetKind::Color && t.saved_index.is_some()
                }) {
                    Some(render_tasks.save_target())
                } else {
                    None
                };
                let saved_alpha = if self.tasks.iter().any(|&task_id| {
                    let t = &render_tasks[task_id];
                    t.target_kind() == RenderTargetKind::Alpha && t.saved_index.is_some()
                }) {
                    Some(render_tasks.save_target())
                } else {
                    None
                };

                // Step through each task, adding to batches as appropriate.
                for &task_id in &self.tasks {
                    let (target_kind, texture_target, layer) = {
                        let task = &mut render_tasks[task_id];
                        let target_kind = task.target_kind();

                        // Find a target to assign this task to, or create a new
                        // one if required.
                        let (texture_target, layer) = match task.location {
                            RenderTaskLocation::TextureCache { texture, layer, .. } => {
                                (Some(texture), layer)
                            }
                            RenderTaskLocation::Fixed(..) => {
                                (None, 0)
                            }
                            RenderTaskLocation::Dynamic(ref mut origin, size) => {
                                let (target_index, alloc_origin) =  match target_kind {
                                    RenderTargetKind::Color => color.allocate(size),
                                    RenderTargetKind::Alpha => alpha.allocate(size),
                                };
                                *origin = Some((alloc_origin, target_index));
                                (None, target_index.0)
                            }
                        };

                        // Replace the pending saved index with a real one
                        if let Some(index) = task.saved_index {
                            assert_eq!(index, SavedTargetIndex::PENDING);
                            task.saved_index = match target_kind {
                                RenderTargetKind::Color => saved_color,
                                RenderTargetKind::Alpha => saved_alpha,
                            };
                        }

                        // Give the render task an opportunity to add any
                        // information to the GPU cache, if appropriate.
                        task.write_gpu_blocks(gpu_cache);

                        (target_kind, texture_target, layer)
                    };

                    match texture_target {
                        Some(texture_target) => {
                            let texture = texture_cache
                                .entry((texture_target, layer))
                                .or_insert(
                                    TextureCacheRenderTarget::new(target_kind)
                                );
                            texture.add_task(task_id, render_tasks);
                        }
                        None => {
                            match target_kind {
                                RenderTargetKind::Color => color.targets[layer].add_task(
                                    task_id,
                                    ctx,
                                    gpu_cache,
                                    render_tasks,
                                    clip_store,
                                    transforms,
                                    deferred_resolves,
                                ),
                                RenderTargetKind::Alpha => alpha.targets[layer].add_task(
                                    task_id,
                                    ctx,
                                    gpu_cache,
                                    render_tasks,
                                    clip_store,
                                    transforms,
                                    deferred_resolves,
                                ),
                            }
                        }
                    }
                }

                color.build(
                    ctx,
                    gpu_cache,
                    render_tasks,
                    deferred_resolves,
                    saved_color,
                    prim_headers,
                    transforms,
                    z_generator,
                );
                alpha.build(
                    ctx,
                    gpu_cache,
                    render_tasks,
                    deferred_resolves,
                    saved_alpha,
                    prim_headers,
                    transforms,
                    z_generator,
                );
            }
        }
    }
}

#[derive(Debug, Clone, Default)]
pub struct CompositeOps {
    // Requires only a single texture as input (e.g. most filters)
    pub filters: Vec<FilterOp>,

    // Requires two source textures (e.g. mix-blend-mode)
    pub mix_blend_mode: Option<MixBlendMode>,
}

impl CompositeOps {
    pub fn new(filters: Vec<FilterOp>, mix_blend_mode: Option<MixBlendMode>) -> Self {
        CompositeOps {
            filters,
            mix_blend_mode,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.filters.is_empty() && self.mix_blend_mode.is_none()
    }
}

/// A rendering-oriented representation of the frame built by the render backend
/// and presented to the renderer.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct Frame {
    //TODO: share the fields with DocumentView struct
    pub window_size: DeviceIntSize,
    pub inner_rect: DeviceIntRect,
    pub background_color: Option<ColorF>,
    pub layer: DocumentLayer,
    pub device_pixel_ratio: f32,
    pub passes: Vec<RenderPass>,
    #[cfg_attr(any(feature = "capture", feature = "replay"), serde(default = "FrameProfileCounters::new", skip))]
    pub profile_counters: FrameProfileCounters,

    pub transform_palette: Vec<TransformData>,
    pub render_tasks: RenderTaskTree,
    pub prim_headers: PrimitiveHeaders,

    /// The GPU cache frame that the contents of Self depend on
    pub gpu_cache_frame_id: FrameId,

    /// List of textures that we don't know about yet
    /// from the backend thread. The render thread
    /// will use a callback to resolve these and
    /// patch the data structures.
    pub deferred_resolves: Vec<DeferredResolve>,

    /// True if this frame contains any render tasks
    /// that write to the texture cache.
    pub has_texture_cache_tasks: bool,

    /// True if this frame has been drawn by the
    /// renderer.
    pub has_been_rendered: bool,
}

impl Frame {
    // This frame must be flushed if it writes to the
    // texture cache, and hasn't been drawn yet.
    pub fn must_be_drawn(&self) -> bool {
        self.has_texture_cache_tasks && !self.has_been_rendered
    }
}

impl BlurTask {
    fn add_instances(
        &self,
        instances: &mut Vec<BlurInstance>,
        blur_direction: BlurDirection,
        task_address: RenderTaskAddress,
        src_task_address: RenderTaskAddress,
    ) {
        let instance = BlurInstance {
            task_address,
            src_task_address,
            blur_direction,
        };

        instances.push(instance);
    }
}

impl ScalingTask {
    fn add_instances(
        &self,
        instances: &mut Vec<ScalingInstance>,
        task_address: RenderTaskAddress,
        src_task_address: RenderTaskAddress,
    ) {
        let instance = ScalingInstance {
            task_address,
            src_task_address,
        };

        instances.push(instance);
    }
}

pub struct SpecialRenderPasses {
    pub alpha_glyph_pass: RenderPass,
    pub color_glyph_pass: RenderPass,
}

impl SpecialRenderPasses {
    pub fn new(screen_size: &DeviceIntSize) -> SpecialRenderPasses {
        SpecialRenderPasses {
            alpha_glyph_pass: RenderPass::new_off_screen(*screen_size),
            color_glyph_pass: RenderPass::new_off_screen(*screen_size),
        }
    }
}

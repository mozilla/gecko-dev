/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipMode, ColorF, PictureRect, ColorU, LayoutVector2D};
use api::{DeviceIntRect, DevicePixelScale, DeviceRect, WorldVector2D};
use api::{FilterOp, ImageRendering, TileOffset, RepeatMode, WorldPoint, WorldSize};
use api::{LayoutPoint, LayoutRect, LayoutSideOffsets, LayoutSize};
use api::{PremultipliedColorF, PropertyBinding, Shadow};
use api::{WorldPixel, BoxShadowClipMode, WorldRect, LayoutToWorldScale};
use api::{PicturePixel, RasterPixel, LineStyle, LineOrientation, AuHelpers};
use api::{LayoutPrimitiveInfo};
use api::DevicePoint;
use border::{get_max_scale_for_border, build_border_instances};
use border::BorderSegmentCacheKey;
use clip::{ClipStore};
use clip_scroll_tree::{ClipScrollTree, SpatialNodeIndex, ROOT_SPATIAL_NODE_INDEX};
use clip::{ClipDataStore, ClipNodeFlags, ClipChainId, ClipChainInstance, ClipItem};
use debug_colors;
use debug_render::DebugItem;
use display_list_flattener::{AsInstanceKind, CreateShadow, IsVisible};
use euclid::{SideOffsets2D, TypedTransform3D, TypedRect, TypedScale, TypedSize2D};
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureContext, PictureState};
use frame_builder::{PrimitiveContext, FrameVisibilityContext, FrameVisibilityState};
use glyph_rasterizer::GlyphKey;
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle, GpuDataRequest, ToGpuBlocks};
use gpu_types::BrushFlags;
use image::{Repetition};
use intern;
use malloc_size_of::MallocSizeOf;
use picture::{PictureCompositeMode, PicturePrimitive, PictureUpdateState};
use picture::{ClusterIndex, PrimitiveList, SurfaceIndex, RetainedTiles, RasterConfig};
use prim_store::borders::{ImageBorderDataHandle, NormalBorderDataHandle};
use prim_store::gradient::{LinearGradientDataHandle, RadialGradientDataHandle};
use prim_store::image::{ImageDataHandle, ImageInstance, VisibleImageTile, YuvImageDataHandle};
use prim_store::line_dec::LineDecorationDataHandle;
use prim_store::picture::PictureDataHandle;
use prim_store::text_run::{TextRunDataHandle, TextRunPrimitive};
#[cfg(debug_assertions)]
use render_backend::{FrameId};
use render_backend::DataStores;
use render_task::{RenderTask, RenderTaskCacheKey, to_cache_size};
use render_task::{RenderTaskCacheKeyKind, RenderTaskId, RenderTaskCacheEntryHandle};
use renderer::{MAX_VERTEX_TEXTURE_WIDTH};
use resource_cache::{ImageProperties, ImageRequest};
use scene::SceneProperties;
use segment::SegmentBuilder;
use std::{cmp, fmt, hash, ops, u32, usize, mem};
#[cfg(debug_assertions)]
use std::sync::atomic::{AtomicUsize, Ordering};
use storage;
use util::{ScaleOffset, MatrixHelpers, MaxRect, Recycler};
use util::{pack_as_float, project_rect, raster_rect_to_device_pixels};
use smallvec::SmallVec;

pub mod borders;
pub mod gradient;
pub mod image;
pub mod line_dec;
pub mod picture;
pub mod text_run;

/// Counter for unique primitive IDs for debug tracing.
#[cfg(debug_assertions)]
static NEXT_PRIM_ID: AtomicUsize = AtomicUsize::new(0);

#[cfg(debug_assertions)]
static PRIM_CHASE_ID: AtomicUsize = AtomicUsize::new(usize::MAX);

#[cfg(debug_assertions)]
pub fn register_prim_chase_id(id: PrimitiveDebugId) {
    PRIM_CHASE_ID.store(id.0, Ordering::SeqCst);
}

#[cfg(not(debug_assertions))]
pub fn register_prim_chase_id(_: PrimitiveDebugId) {
}

const MIN_BRUSH_SPLIT_AREA: f32 = 256.0 * 256.0;
pub const VECS_PER_SEGMENT: usize = 2;

#[derive(Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq)]
pub struct ScrollNodeAndClipChain {
    pub spatial_node_index: SpatialNodeIndex,
    pub clip_chain_id: ClipChainId,
}

impl ScrollNodeAndClipChain {
    pub fn new(
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId
    ) -> Self {
        ScrollNodeAndClipChain {
            spatial_node_index,
            clip_chain_id,
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, MallocSizeOf)]
pub struct PrimitiveOpacity {
    pub is_opaque: bool,
}

impl PrimitiveOpacity {
    pub fn opaque() -> PrimitiveOpacity {
        PrimitiveOpacity { is_opaque: true }
    }

    pub fn translucent() -> PrimitiveOpacity {
        PrimitiveOpacity { is_opaque: false }
    }

    pub fn from_alpha(alpha: f32) -> PrimitiveOpacity {
        PrimitiveOpacity {
            is_opaque: alpha >= 1.0,
        }
    }

    pub fn combine(&self, other: PrimitiveOpacity) -> PrimitiveOpacity {
        PrimitiveOpacity{
            is_opaque: self.is_opaque && other.is_opaque
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub enum VisibleFace {
    Front,
    Back,
}

impl ops::Not for VisibleFace {
    type Output = Self;
    fn not(self) -> Self {
        match self {
            VisibleFace::Front => VisibleFace::Back,
            VisibleFace::Back => VisibleFace::Front,
        }
    }
}

#[derive(Debug, Clone)]
pub enum CoordinateSpaceMapping<F, T> {
    Local,
    ScaleOffset(ScaleOffset),
    Transform(TypedTransform3D<f32, F, T>),
}

impl<F, T> CoordinateSpaceMapping<F, T> {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) -> Option<Self> {
        let spatial_nodes = &clip_scroll_tree.spatial_nodes;
        let ref_spatial_node = &spatial_nodes[ref_spatial_node_index.0 as usize];
        let target_spatial_node = &spatial_nodes[target_node_index.0 as usize];

        if ref_spatial_node_index == target_node_index {
            Some(CoordinateSpaceMapping::Local)
        } else if ref_spatial_node.coordinate_system_id == target_spatial_node.coordinate_system_id {
            Some(CoordinateSpaceMapping::ScaleOffset(
                ref_spatial_node.coordinate_system_relative_scale_offset
                    .inverse()
                    .accumulate(
                        &target_spatial_node.coordinate_system_relative_scale_offset
                    )
            ))
        } else {
            let transform = clip_scroll_tree.get_relative_transform(
                target_node_index,
                ref_spatial_node_index,
            );

            transform.map(|transform| {
                CoordinateSpaceMapping::Transform(
                    transform.with_source::<F>().with_destination::<T>()
                )
            })
        }
    }
}

#[derive(Debug, Clone)]
pub struct SpaceMapper<F, T> {
    kind: CoordinateSpaceMapping<F, T>,
    pub ref_spatial_node_index: SpatialNodeIndex,
    pub current_target_spatial_node_index: SpatialNodeIndex,
    pub bounds: TypedRect<f32, T>,
}

impl<F, T> SpaceMapper<F, T> where F: fmt::Debug {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        bounds: TypedRect<f32, T>,
    ) -> Self {
        SpaceMapper {
            kind: CoordinateSpaceMapping::Local,
            ref_spatial_node_index,
            current_target_spatial_node_index: ref_spatial_node_index,
            bounds,
        }
    }

    pub fn new_with_target(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        bounds: TypedRect<f32, T>,
        clip_scroll_tree: &ClipScrollTree,
    ) -> Self {
        let mut mapper = SpaceMapper::new(ref_spatial_node_index, bounds);
        mapper.set_target_spatial_node(target_node_index, clip_scroll_tree);
        mapper
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) {
        if target_node_index != self.current_target_spatial_node_index {
            self.current_target_spatial_node_index = target_node_index;

            self.kind = CoordinateSpaceMapping::new(
                self.ref_spatial_node_index,
                target_node_index,
                clip_scroll_tree,
            ).expect("bug: should have been culled by invalid node");
        }
    }

    pub fn get_transform(&self) -> TypedTransform3D<f32, F, T> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                TypedTransform3D::identity()
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                scale_offset.to_transform()
            }
            CoordinateSpaceMapping::Transform(transform) => {
                transform
            }
        }
    }

    pub fn unmap(&self, rect: &TypedRect<f32, T>) -> Option<TypedRect<f32, F>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(TypedRect::from_untyped(&rect.to_untyped()))
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.unmap_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.inverse_rect_footprint(rect)
            }
        }
    }

    pub fn map(&self, rect: &TypedRect<f32, F>) -> Option<TypedRect<f32, T>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(TypedRect::from_untyped(&rect.to_untyped()))
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.map_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                match project_rect(transform, rect, &self.bounds) {
                    Some(bounds) => {
                        Some(bounds)
                    }
                    None => {
                        warn!("parent relative transform can't transform the primitive rect for {:?}", rect);
                        None
                    }
                }
            }
        }
    }

    pub fn visible_face(&self) -> VisibleFace {
        match self.kind {
            CoordinateSpaceMapping::Local => VisibleFace::Front,
            CoordinateSpaceMapping::ScaleOffset(_) => VisibleFace::Front,
            CoordinateSpaceMapping::Transform(ref transform) => {
                if transform.is_backface_visible() {
                    VisibleFace::Back
                } else {
                    VisibleFace::Front
                }
            }
        }
    }
}

/// For external images, it's not possible to know the
/// UV coords of the image (or the image data itself)
/// until the render thread receives the frame and issues
/// callbacks to the client application. For external
/// images that are visible, a DeferredResolve is created
/// that is stored in the frame. This allows the render
/// thread to iterate this list and update any changed
/// texture data and update the UV rect. Any filtering
/// is handled externally for NativeTexture external
/// images.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct DeferredResolve {
    pub address: GpuCacheAddress,
    pub image_properties: ImageProperties,
    pub rendering: ImageRendering,
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub struct ClipTaskIndex(pub u16);

impl ClipTaskIndex {
    pub const INVALID: ClipTaskIndex = ClipTaskIndex(0);
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureIndex(pub usize);

impl GpuCacheHandle {
    pub fn as_int(&self, gpu_cache: &GpuCache) -> i32 {
        gpu_cache.get_address(self).as_int()
    }
}

impl GpuCacheAddress {
    pub fn as_int(&self) -> i32 {
        // TODO(gw): Temporarily encode GPU Cache addresses as a single int.
        //           In the future, we can change the PrimitiveInstanceData struct
        //           to use 2x u16 for the vertex attribute instead of an i32.
        self.v as i32 * MAX_VERTEX_TEXTURE_WIDTH as i32 + self.u as i32
    }
}

/// The information about an interned primitive that
/// is stored and available in the scene builder
/// thread.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PrimitiveSceneData {
    pub prim_size: LayoutSize,
    pub is_backface_visible: bool,
}

/// Information specific to a primitive type that
/// uniquely identifies a primitive template by key.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub enum PrimitiveKeyKind {
    /// Clear an existing rect, used for special effects on some platforms.
    Clear,
    Rectangle {
        color: ColorU,
    },
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq)]
pub struct RectangleKey {
    x: f32,
    y: f32,
    w: f32,
    h: f32,
}

impl Eq for RectangleKey {}

impl hash::Hash for RectangleKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.x.to_bits().hash(state);
        self.y.to_bits().hash(state);
        self.w.to_bits().hash(state);
        self.h.to_bits().hash(state);
    }
}

impl From<RectangleKey> for LayoutRect {
    fn from(key: RectangleKey) -> LayoutRect {
        LayoutRect {
            origin: LayoutPoint::new(key.x, key.y),
            size: LayoutSize::new(key.w, key.h),
        }
    }
}

impl From<RectangleKey> for WorldRect {
    fn from(key: RectangleKey) -> WorldRect {
        WorldRect {
            origin: WorldPoint::new(key.x, key.y),
            size: WorldSize::new(key.w, key.h),
        }
    }
}

impl From<LayoutRect> for RectangleKey {
    fn from(rect: LayoutRect) -> RectangleKey {
        RectangleKey {
            x: rect.origin.x,
            y: rect.origin.y,
            w: rect.size.width,
            h: rect.size.height,
        }
    }
}

impl From<WorldRect> for RectangleKey {
    fn from(rect: WorldRect) -> RectangleKey {
        RectangleKey {
            x: rect.origin.x,
            y: rect.origin.y,
            w: rect.size.width,
            h: rect.size.height,
        }
    }
}

/// A hashable SideOffset2D that can be used in primitive keys.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq)]
pub struct SideOffsetsKey {
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub left: f32,
}

impl Eq for SideOffsetsKey {}

impl hash::Hash for SideOffsetsKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.top.to_bits().hash(state);
        self.right.to_bits().hash(state);
        self.bottom.to_bits().hash(state);
        self.left.to_bits().hash(state);
    }
}

impl From<SideOffsetsKey> for LayoutSideOffsets {
    fn from(key: SideOffsetsKey) -> LayoutSideOffsets {
        LayoutSideOffsets::new(
            key.top,
            key.right,
            key.bottom,
            key.left,
        )
    }
}

impl From<LayoutSideOffsets> for SideOffsetsKey {
    fn from(offsets: LayoutSideOffsets) -> SideOffsetsKey {
        SideOffsetsKey {
            top: offsets.top,
            right: offsets.right,
            bottom: offsets.bottom,
            left: offsets.left,
        }
    }
}

impl From<SideOffsets2D<f32>> for SideOffsetsKey {
    fn from(offsets: SideOffsets2D<f32>) -> SideOffsetsKey {
        SideOffsetsKey {
            top: offsets.top,
            right: offsets.right,
            bottom: offsets.bottom,
            left: offsets.left,
        }
    }
}

/// A hashable size for using as a key during primitive interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Debug, Clone, MallocSizeOf, PartialEq)]
pub struct SizeKey {
    w: f32,
    h: f32,
}

impl Eq for SizeKey {}

impl hash::Hash for SizeKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.w.to_bits().hash(state);
        self.h.to_bits().hash(state);
    }
}

impl From<SizeKey> for LayoutSize {
    fn from(key: SizeKey) -> LayoutSize {
        LayoutSize::new(key.w, key.h)
    }
}

impl<U> From<TypedSize2D<f32, U>> for SizeKey {
    fn from(size: TypedSize2D<f32, U>) -> SizeKey {
        SizeKey {
            w: size.width,
            h: size.height,
        }
    }
}

/// A hashable vec for using as a key during primitive interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Debug, Clone, MallocSizeOf, PartialEq)]
pub struct VectorKey {
    pub x: f32,
    pub y: f32,
}

impl Eq for VectorKey {}

impl hash::Hash for VectorKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.x.to_bits().hash(state);
        self.y.to_bits().hash(state);
    }
}

impl From<VectorKey> for LayoutVector2D {
    fn from(key: VectorKey) -> LayoutVector2D {
        LayoutVector2D::new(key.x, key.y)
    }
}

impl From<VectorKey> for WorldVector2D {
    fn from(key: VectorKey) -> WorldVector2D {
        WorldVector2D::new(key.x, key.y)
    }
}

impl From<LayoutVector2D> for VectorKey {
    fn from(vec: LayoutVector2D) -> VectorKey {
        VectorKey {
            x: vec.x,
            y: vec.y,
        }
    }
}

impl From<WorldVector2D> for VectorKey {
    fn from(vec: WorldVector2D) -> VectorKey {
        VectorKey {
            x: vec.x,
            y: vec.y,
        }
    }
}

/// A hashable point for using as a key during primitive interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq)]
pub struct PointKey {
    pub x: f32,
    pub y: f32,
}

impl Eq for PointKey {}

impl hash::Hash for PointKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.x.to_bits().hash(state);
        self.y.to_bits().hash(state);
    }
}

impl From<PointKey> for LayoutPoint {
    fn from(key: PointKey) -> LayoutPoint {
        LayoutPoint::new(key.x, key.y)
    }
}

impl From<LayoutPoint> for PointKey {
    fn from(p: LayoutPoint) -> PointKey {
        PointKey {
            x: p.x,
            y: p.y,
        }
    }
}

impl From<WorldPoint> for PointKey {
    fn from(p: WorldPoint) -> PointKey {
        PointKey {
            x: p.x,
            y: p.y,
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct PrimKeyCommonData {
    pub is_backface_visible: bool,
    pub prim_size: SizeKey,
}

impl PrimKeyCommonData {
    pub fn with_info(
        info: &LayoutPrimitiveInfo,
    ) -> Self {
        PrimKeyCommonData {
            is_backface_visible: info.is_backface_visible,
            prim_size: info.rect.size.into(),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct PrimKey<T: MallocSizeOf> {
    pub common: PrimKeyCommonData,
    pub kind: T,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct PrimitiveKey {
    pub common: PrimKeyCommonData,
    pub kind: PrimitiveKeyKind,
}

impl PrimitiveKey {
    pub fn new(
        is_backface_visible: bool,
        prim_size: LayoutSize,
        kind: PrimitiveKeyKind,
    ) -> Self {
        PrimitiveKey {
            common: PrimKeyCommonData {
                is_backface_visible,
                prim_size: prim_size.into(),
            },
            kind,
        }
    }
}

impl intern::InternDebug for PrimitiveKey {}

impl AsInstanceKind<PrimitiveDataHandle> for PrimitiveKey {
    /// Construct a primitive instance that matches the type
    /// of primitive key.
    fn as_instance_kind(
        &self,
        data_handle: PrimitiveDataHandle,
        _: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        match self.kind {
            PrimitiveKeyKind::Clear => {
                PrimitiveInstanceKind::Clear {
                    data_handle
                }
            }
            PrimitiveKeyKind::Rectangle { .. } => {
                PrimitiveInstanceKind::Rectangle {
                    data_handle,
                    opacity_binding_index: OpacityBindingIndex::INVALID,
                    segment_instance_index: SegmentInstanceIndex::INVALID,
                }
            }
        }
    }
}

/// The shared information for a given primitive. This is interned and retained
/// both across frames and display lists, by comparing the matching PrimitiveKey.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub enum PrimitiveTemplateKind {
    Rectangle {
        color: ColorF,
    },
    Clear,
}

/// Construct the primitive template data from a primitive key. This
/// is invoked when a primitive key is created and the interner
/// doesn't currently contain a primitive with this key.
impl PrimitiveKeyKind {
    fn into_template(self) -> PrimitiveTemplateKind {
        match self {
            PrimitiveKeyKind::Clear => {
                PrimitiveTemplateKind::Clear
            }
            PrimitiveKeyKind::Rectangle { color, .. } => {
                PrimitiveTemplateKind::Rectangle {
                    color: color.into(),
                }
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PrimTemplateCommonData {
    pub is_backface_visible: bool,
    pub prim_size: LayoutSize,
    pub opacity: PrimitiveOpacity,
    /// The GPU cache handle for a primitive template. Since this structure
    /// is retained across display lists by interning, this GPU cache handle
    /// also remains valid, which reduces the number of updates to the GPU
    /// cache when a new display list is processed.
    pub gpu_cache_handle: GpuCacheHandle,
}

impl PrimTemplateCommonData {
    pub fn with_key_common(common: PrimKeyCommonData) -> Self {
        PrimTemplateCommonData {
            is_backface_visible: common.is_backface_visible,
            prim_size: common.prim_size.into(),
            gpu_cache_handle: GpuCacheHandle::new(),
            opacity: PrimitiveOpacity::translucent(),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PrimTemplate<T> {
    pub common: PrimTemplateCommonData,
    pub kind: T,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PrimitiveTemplate {
    pub common: PrimTemplateCommonData,
    pub kind: PrimitiveTemplateKind,
}

impl ops::Deref for PrimitiveTemplate {
    type Target = PrimTemplateCommonData;
    fn deref(&self) -> &Self::Target {
        &self.common
    }
}

impl ops::DerefMut for PrimitiveTemplate {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.common
    }
}

impl From<PrimitiveKey> for PrimitiveTemplate {
    fn from(item: PrimitiveKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(item.common);
        let kind = item.kind.into_template();

        PrimitiveTemplate { common, kind, }
    }
}

impl PrimitiveTemplateKind {
    /// Write any GPU blocks for the primitive template to the given request object.
    fn write_prim_gpu_blocks(
        &self,
        request: &mut GpuDataRequest
    ) {
        match *self {
            PrimitiveTemplateKind::Clear => {
                // Opaque black with operator dest out
                request.push(PremultipliedColorF::BLACK);
            }
            PrimitiveTemplateKind::Rectangle { ref color, .. } => {
                request.push(color.premultiplied());
            }
        }
    }
}

impl PrimitiveTemplate {
    /// Update the GPU cache for a given primitive template. This may be called multiple
    /// times per frame, by each primitive reference that refers to this interned
    /// template. The initial request call to the GPU cache ensures that work is only
    /// done if the cache entry is invalid (due to first use or eviction).
    pub fn update(
        &mut self,
        frame_state: &mut FrameBuildingState,
    ) {
        if let Some(mut request) = frame_state.gpu_cache.request(&mut self.common.gpu_cache_handle) {
            self.kind.write_prim_gpu_blocks(&mut request);
        }

        self.opacity = match self.kind {
            PrimitiveTemplateKind::Clear => {
                PrimitiveOpacity::translucent()
            }
            PrimitiveTemplateKind::Rectangle { ref color, .. } => {
                PrimitiveOpacity::from_alpha(color.a)
            }
        };
    }
}

impl intern::Internable for PrimitiveKeyKind {
    type Marker = ::intern_types::prim::Marker;
    type Source = PrimitiveKey;
    type StoreData = PrimitiveTemplate;
    type InternData = PrimitiveSceneData;

    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> PrimitiveKey {
        PrimitiveKey::new(
            info.is_backface_visible,
            info.rect.size,
            self,
        )
    }
}

use intern_types::prim::Handle as PrimitiveDataHandle;

// Maintains a list of opacity bindings that have been collapsed into
// the color of a single primitive. This is an important optimization
// that avoids allocating an intermediate surface for most common
// uses of opacity filters.
#[derive(Debug)]
pub struct OpacityBinding {
    pub bindings: Vec<PropertyBinding<f32>>,
    pub current: f32,
}

impl OpacityBinding {
    pub fn new() -> OpacityBinding {
        OpacityBinding {
            bindings: Vec::new(),
            current: 1.0,
        }
    }

    // Add a new opacity value / binding to the list
    pub fn push(&mut self, binding: PropertyBinding<f32>) {
        self.bindings.push(binding);
    }

    // Resolve the current value of each opacity binding, and
    // store that as a single combined opacity. Returns true
    // if the opacity value changed from last time.
    pub fn update(&mut self, scene_properties: &SceneProperties) {
        let mut new_opacity = 1.0;

        for binding in &self.bindings {
            let opacity = scene_properties.resolve_float(binding);
            new_opacity = new_opacity * opacity;
        }

        self.current = new_opacity;
    }
}

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct VisibleMaskImageTile {
    pub tile_offset: TileOffset,
    pub tile_rect: LayoutRect,
}

#[derive(Debug)]
pub struct VisibleGradientTile {
    pub handle: GpuCacheHandle,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

/// Information about how to cache a border segment,
/// along with the current render task cache entry.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub struct BorderSegmentInfo {
    pub local_task_size: LayoutSize,
    pub cache_key: BorderSegmentCacheKey,
}

bitflags! {
    /// Each bit of the edge AA mask is:
    /// 0, when the edge of the primitive needs to be considered for AA
    /// 1, when the edge of the segment needs to be considered for AA
    ///
    /// *Note*: the bit values have to match the shader logic in
    /// `write_transform_vertex()` function.
    #[cfg_attr(feature = "capture", derive(Serialize))]
    #[cfg_attr(feature = "replay", derive(Deserialize))]
    #[derive(MallocSizeOf)]
    pub struct EdgeAaSegmentMask: u8 {
        const LEFT = 0x1;
        const TOP = 0x2;
        const RIGHT = 0x4;
        const BOTTOM = 0x8;
    }
}

/// Represents the visibility state of a segment (wrt clip masks).
#[derive(Debug, Clone)]
pub enum ClipMaskKind {
    /// The segment has a clip mask, specified by the render task.
    Mask(RenderTaskId),
    /// The segment has no clip mask.
    None,
    /// The segment is made invisible / clipped completely.
    Clipped,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf)]
pub struct BrushSegment {
    pub local_rect: LayoutRect,
    pub may_need_clip_mask: bool,
    pub edge_flags: EdgeAaSegmentMask,
    pub extra_data: [f32; 4],
    pub brush_flags: BrushFlags,
}

impl BrushSegment {
    pub fn new(
        local_rect: LayoutRect,
        may_need_clip_mask: bool,
        edge_flags: EdgeAaSegmentMask,
        extra_data: [f32; 4],
        brush_flags: BrushFlags,
    ) -> Self {
        Self {
            local_rect,
            may_need_clip_mask,
            edge_flags,
            extra_data,
            brush_flags,
        }
    }

    /// Write out to the clip mask instances array the correct clip mask
    /// config for this segment.
    pub fn update_clip_task(
        &self,
        clip_chain: Option<&ClipChainInstance>,
        prim_bounding_rect: WorldRect,
        root_spatial_node_index: SpatialNodeIndex,
        surface_index: SurfaceIndex,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        clip_data_store: &mut ClipDataStore,
    ) -> ClipMaskKind {
        match clip_chain {
            Some(clip_chain) => {
                if !clip_chain.needs_mask ||
                   (!self.may_need_clip_mask && !clip_chain.has_non_local_clips) {
                    return ClipMaskKind::None;
                }

                let (device_rect, _) = match get_raster_rects(
                    clip_chain.pic_clip_rect,
                    &pic_state.map_pic_to_raster,
                    &pic_state.map_raster_to_world,
                    prim_bounding_rect,
                    frame_context.device_pixel_scale,
                ) {
                    Some(info) => info,
                    None => {
                        return ClipMaskKind::Clipped;
                    }
                };

                let clip_task = RenderTask::new_mask(
                    device_rect.to_i32(),
                    clip_chain.clips_range,
                    root_spatial_node_index,
                    frame_state.clip_store,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_state.render_tasks,
                    clip_data_store,
                );

                let clip_task_id = frame_state.render_tasks.add(clip_task);
                frame_state.surfaces[surface_index.0].tasks.push(clip_task_id);
                ClipMaskKind::Mask(clip_task_id)
            }
            None => {
                ClipMaskKind::Clipped
            }
        }
    }
}

#[derive(Debug)]
#[repr(C)]
struct ClipRect {
    rect: LayoutRect,
    mode: f32,
}

#[derive(Debug)]
#[repr(C)]
struct ClipCorner {
    rect: LayoutRect,
    outer_radius_x: f32,
    outer_radius_y: f32,
    inner_radius_x: f32,
    inner_radius_y: f32,
}

impl ToGpuBlocks for ClipCorner {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        self.write(&mut request)
    }
}

impl ClipCorner {
    fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.rect);
        request.push([
            self.outer_radius_x,
            self.outer_radius_y,
            self.inner_radius_x,
            self.inner_radius_y,
        ]);
    }

    fn uniform(rect: LayoutRect, outer_radius: f32, inner_radius: f32) -> ClipCorner {
        ClipCorner {
            rect,
            outer_radius_x: outer_radius,
            outer_radius_y: outer_radius,
            inner_radius_x: inner_radius,
            inner_radius_y: inner_radius,
        }
    }
}

#[derive(Debug)]
#[repr(C)]
pub struct ImageMaskData {
    /// The local size of the whole masked area.
    pub local_mask_size: LayoutSize,
}

impl ToGpuBlocks for ImageMaskData {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push([
            self.local_mask_size.width,
            self.local_mask_size.height,
            0.0,
            0.0,
        ]);
    }
}

#[derive(Debug)]
pub struct ClipData {
    rect: ClipRect,
    top_left: ClipCorner,
    top_right: ClipCorner,
    bottom_left: ClipCorner,
    bottom_right: ClipCorner,
}

impl ClipData {
    pub fn rounded_rect(size: LayoutSize, radii: &BorderRadius, mode: ClipMode) -> ClipData {
        // TODO(gw): For simplicity, keep most of the clip GPU structs the
        //           same as they were, even though the origin is now always
        //           zero, since they are in the clip's local space. In future,
        //           we could reduce the GPU cache size of ClipData.
        let rect = LayoutRect::new(
            LayoutPoint::zero(),
            size,
        );

        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y),
                    LayoutSize::new(radii.top_left.width, radii.top_left.height),
                ),
                outer_radius_x: radii.top_left.width,
                outer_radius_y: radii.top_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            top_right: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radii.top_right.width,
                        rect.origin.y,
                    ),
                    LayoutSize::new(radii.top_right.width, radii.top_right.height),
                ),
                outer_radius_x: radii.top_right.width,
                outer_radius_y: radii.top_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_left: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x,
                        rect.origin.y + rect.size.height - radii.bottom_left.height,
                    ),
                    LayoutSize::new(radii.bottom_left.width, radii.bottom_left.height),
                ),
                outer_radius_x: radii.bottom_left.width,
                outer_radius_y: radii.bottom_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_right: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radii.bottom_right.width,
                        rect.origin.y + rect.size.height - radii.bottom_right.height,
                    ),
                    LayoutSize::new(radii.bottom_right.width, radii.bottom_right.height),
                ),
                outer_radius_x: radii.bottom_right.width,
                outer_radius_y: radii.bottom_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
        }
    }

    pub fn uniform(size: LayoutSize, radius: f32, mode: ClipMode) -> ClipData {
        // TODO(gw): For simplicity, keep most of the clip GPU structs the
        //           same as they were, even though the origin is now always
        //           zero, since they are in the clip's local space. In future,
        //           we could reduce the GPU cache size of ClipData.
        let rect = LayoutRect::new(
            LayoutPoint::zero(),
            size,
        );

        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            top_right: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x + rect.size.width - radius, rect.origin.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_left: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y + rect.size.height - radius),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_right: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radius,
                        rect.origin.y + rect.size.height - radius,
                    ),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
        }
    }

    pub fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.rect.rect);
        request.push([self.rect.mode, 0.0, 0.0, 0.0]);
        for corner in &[
            &self.top_left,
            &self.top_right,
            &self.bottom_left,
            &self.bottom_right,
        ] {
            corner.write(request);
        }
    }
}

/// A hashable descriptor for nine-patches, used by image and
/// gradient borders.
#[derive(Debug, Clone, PartialEq, Eq, Hash, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NinePatchDescriptor {
    pub width: i32,
    pub height: i32,
    pub slice: SideOffsets2D<i32>,
    pub fill: bool,
    pub repeat_horizontal: RepeatMode,
    pub repeat_vertical: RepeatMode,
    pub outset: SideOffsetsKey,
    pub widths: SideOffsetsKey,
}

impl IsVisible for PrimitiveKeyKind {
    // Return true if the primary primitive is visible.
    // Used to trivially reject non-visible primitives.
    // TODO(gw): Currently, primitives other than those
    //           listed here are handled before the
    //           add_primitive() call. In the future
    //           we should move the logic for all other
    //           primitive types to use this.
    fn is_visible(&self) -> bool {
        match *self {
            PrimitiveKeyKind::Clear => {
                true
            }
            PrimitiveKeyKind::Rectangle { ref color, .. } => {
                color.a > 0
            }
        }
    }
}

impl CreateShadow for PrimitiveKeyKind {
    // Create a clone of this PrimitiveContainer, applying whatever
    // changes are necessary to the primitive to support rendering
    // it as part of the supplied shadow.
    fn create_shadow(
        &self,
        shadow: &Shadow,
    ) -> PrimitiveKeyKind {
        match *self {
            PrimitiveKeyKind::Rectangle { .. } => {
                PrimitiveKeyKind::Rectangle {
                    color: shadow.color.into(),
                }
            }
            PrimitiveKeyKind::Clear => {
                panic!("bug: this prim is not supported in shadow contexts");
            }
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveDebugId(pub usize);

#[derive(Clone, Debug)]
pub enum PrimitiveInstanceKind {
    /// Direct reference to a Picture
    Picture {
        /// Handle to the common interned data for this primitive.
        data_handle: PictureDataHandle,
        pic_index: PictureIndex,
    },
    /// A run of glyphs, with associated font parameters.
    TextRun {
        /// Handle to the common interned data for this primitive.
        data_handle: TextRunDataHandle,
        /// Index to the per instance scratch data for this primitive.
        run_index: TextRunIndex,
    },
    /// A line decoration. cache_handle refers to a cached render
    /// task handle, if this line decoration is not a simple solid.
    LineDecoration {
        /// Handle to the common interned data for this primitive.
        data_handle: LineDecorationDataHandle,
        // TODO(gw): For now, we need to store some information in
        //           the primitive instance that is created during
        //           prepare_prims and read during the batching pass.
        //           Once we unify the prepare_prims and batching to
        //           occur at the same time, we can remove most of
        //           the things we store here in the instance, and
        //           use them directly. This will remove cache_handle,
        //           but also the opacity, clip_task_id etc below.
        cache_handle: Option<RenderTaskCacheEntryHandle>,
    },
    NormalBorder {
        /// Handle to the common interned data for this primitive.
        data_handle: NormalBorderDataHandle,
        cache_handles: storage::Range<RenderTaskCacheEntryHandle>,
    },
    ImageBorder {
        /// Handle to the common interned data for this primitive.
        data_handle: ImageBorderDataHandle,
    },
    Rectangle {
        /// Handle to the common interned data for this primitive.
        data_handle: PrimitiveDataHandle,
        opacity_binding_index: OpacityBindingIndex,
        segment_instance_index: SegmentInstanceIndex,
    },
    YuvImage {
        /// Handle to the common interned data for this primitive.
        data_handle: YuvImageDataHandle,
        segment_instance_index: SegmentInstanceIndex,
    },
    Image {
        /// Handle to the common interned data for this primitive.
        data_handle: ImageDataHandle,
        image_instance_index: ImageInstanceIndex,
    },
    LinearGradient {
        /// Handle to the common interned data for this primitive.
        data_handle: LinearGradientDataHandle,
        visible_tiles_range: GradientTileRange,
    },
    RadialGradient {
        /// Handle to the common interned data for this primitive.
        data_handle: RadialGradientDataHandle,
        visible_tiles_range: GradientTileRange,
    },
    /// Clear out a rect, used for special effects.
    Clear {
        /// Handle to the common interned data for this primitive.
        data_handle: PrimitiveDataHandle,
    },
}

#[derive(Debug, Copy, Clone, PartialEq)]
pub struct PrimitiveVisibilityIndex(pub u32);

impl PrimitiveVisibilityIndex {
    pub const INVALID: PrimitiveVisibilityIndex = PrimitiveVisibilityIndex(u32::MAX);
}

/// Information stored for a visible primitive about the visible
/// rect and associated clip information.
pub struct PrimitiveVisibility {
    /// The clip chain instance that was built for this primitive.
    pub clip_chain: ClipChainInstance,

    /// The current world rect, clipped to screen / dirty rect boundaries.
    // TODO(gw): This is only used by a small number of primitives.
    //           It's probably faster to not store this and recalculate
    //           on demand in those cases?
    pub clipped_world_rect: WorldRect,

    /// An index into the clip task instances array in the primitive
    /// store. If this is ClipTaskIndex::INVALID, then the primitive
    /// has no clip mask. Otherwise, it may store the offset of the
    /// global clip mask task for this primitive, or the first of
    /// a list of clip task ids (one per segment).
    pub clip_task_index: ClipTaskIndex,

    /// The current combined local clip for this primitive, from
    /// the primitive local clip above and the current clip chain.
    pub combined_local_clip_rect: LayoutRect,
}

#[derive(Clone, Debug)]
pub struct PrimitiveInstance {
    /// Identifies the kind of primitive this
    /// instance is, and references to where
    /// the relevant information for the primitive
    /// can be found.
    pub kind: PrimitiveInstanceKind,

    /// Local space origin of this primitive. The size
    /// of the primitive is defined by the template.
    pub prim_origin: LayoutPoint,

    /// Local space clip rect for this instance
    pub local_clip_rect: LayoutRect,

    #[cfg(debug_assertions)]
    pub id: PrimitiveDebugId,

    /// The last frame ID (of the `RenderTaskTree`) this primitive
    /// was prepared for rendering in.
    #[cfg(debug_assertions)]
    pub prepared_frame_id: FrameId,

    /// If this primitive is visible, an index into the instance
    /// visibility scratch buffer. If not visible, INVALID.
    pub visibility_info: PrimitiveVisibilityIndex,

    /// The cluster that this primitive belongs to. This is used
    /// for quickly culling out groups of primitives during the
    /// initial picture traversal pass.
    pub cluster_index: ClusterIndex,

    /// ID of the clip chain that this primitive is clipped by.
    pub clip_chain_id: ClipChainId,

    /// ID of the spatial node that this primitive is positioned by.
    pub spatial_node_index: SpatialNodeIndex,
}

impl PrimitiveInstance {
    pub fn new(
        prim_origin: LayoutPoint,
        local_clip_rect: LayoutRect,
        kind: PrimitiveInstanceKind,
        clip_chain_id: ClipChainId,
        spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        PrimitiveInstance {
            prim_origin,
            local_clip_rect,
            kind,
            #[cfg(debug_assertions)]
            prepared_frame_id: FrameId::INVALID,
            #[cfg(debug_assertions)]
            id: PrimitiveDebugId(NEXT_PRIM_ID.fetch_add(1, Ordering::Relaxed)),
            visibility_info: PrimitiveVisibilityIndex::INVALID,
            clip_chain_id,
            spatial_node_index,
            cluster_index: ClusterIndex::INVALID,
        }
    }

    // Reset any pre-frame state for this primitive.
    pub fn reset(&mut self) {
        self.visibility_info = PrimitiveVisibilityIndex::INVALID;
    }

    #[cfg(debug_assertions)]
    pub fn is_chased(&self) -> bool {
        PRIM_CHASE_ID.load(Ordering::SeqCst) == self.id.0
    }

    #[cfg(not(debug_assertions))]
    pub fn is_chased(&self) -> bool {
        false
    }

    pub fn uid(&self) -> intern::ItemUid {
        match &self.kind {
            PrimitiveInstanceKind::Clear { data_handle, .. } |
            PrimitiveInstanceKind::Rectangle { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::Image { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::LineDecoration { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::LinearGradient { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::NormalBorder { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::Picture { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::RadialGradient { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::TextRun { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                data_handle.uid()
            }
        }
    }
}

#[derive(Debug)]
pub struct SegmentedInstance {
    pub gpu_cache_handle: GpuCacheHandle,
    pub segments_range: SegmentsRange,
}

pub type GlyphKeyStorage = storage::Storage<GlyphKey>;
pub type TextRunIndex = storage::Index<TextRunPrimitive>;
pub type TextRunStorage = storage::Storage<TextRunPrimitive>;
pub type OpacityBindingIndex = storage::Index<OpacityBinding>;
pub type OpacityBindingStorage = storage::Storage<OpacityBinding>;
pub type BorderHandleStorage = storage::Storage<RenderTaskCacheEntryHandle>;
pub type SegmentStorage = storage::Storage<BrushSegment>;
pub type SegmentsRange = storage::Range<BrushSegment>;
pub type SegmentInstanceStorage = storage::Storage<SegmentedInstance>;
pub type SegmentInstanceIndex = storage::Index<SegmentedInstance>;
pub type ImageInstanceStorage = storage::Storage<ImageInstance>;
pub type ImageInstanceIndex = storage::Index<ImageInstance>;
pub type GradientTileStorage = storage::Storage<VisibleGradientTile>;
pub type GradientTileRange = storage::Range<VisibleGradientTile>;

/// Contains various vecs of data that is used only during frame building,
/// where we want to recycle the memory each new display list, to avoid constantly
/// re-allocating and moving memory around. Written during primitive preparation,
/// and read during batching.
pub struct PrimitiveScratchBuffer {
    /// Contains a list of clip mask instance parameters
    /// per segment generated.
    pub clip_mask_instances: Vec<ClipMaskKind>,

    /// List of glyphs keys that are allocated by each
    /// text run instance.
    pub glyph_keys: GlyphKeyStorage,

    /// List of render task handles for border segment instances
    /// that have been added this frame.
    pub border_cache_handles: BorderHandleStorage,

    /// A list of brush segments that have been built for this scene.
    pub segments: SegmentStorage,

    /// A list of segment ranges and GPU cache handles for prim instances
    /// that have opted into segment building. In future, this should be
    /// removed in favor of segment building during primitive interning.
    pub segment_instances: SegmentInstanceStorage,

    /// A list of visible tiles that tiled gradients use to store
    /// per-tile information.
    pub gradient_tiles: GradientTileStorage,

    /// List of the visibility information for currently visible primitives.
    pub prim_info: Vec<PrimitiveVisibility>,

    pub debug_items: Vec<DebugItem>,
}

impl PrimitiveScratchBuffer {
    pub fn new() -> Self {
        PrimitiveScratchBuffer {
            clip_mask_instances: Vec::new(),
            glyph_keys: GlyphKeyStorage::new(0),
            border_cache_handles: BorderHandleStorage::new(0),
            segments: SegmentStorage::new(0),
            segment_instances: SegmentInstanceStorage::new(0),
            gradient_tiles: GradientTileStorage::new(0),
            debug_items: Vec::new(),
            prim_info: Vec::new(),
        }
    }

    pub fn recycle(&mut self, recycler: &mut Recycler) {
        recycler.recycle_vec(&mut self.clip_mask_instances);
        recycler.recycle_vec(&mut self.prim_info);
        self.glyph_keys.recycle(recycler);
        self.border_cache_handles.recycle(recycler);
        self.segments.recycle(recycler);
        self.segment_instances.recycle(recycler);
        self.gradient_tiles.recycle(recycler);
        recycler.recycle_vec(&mut self.debug_items);
    }

    pub fn begin_frame(&mut self) {
        // Clear the clip mask tasks for the beginning of the frame. Append
        // a single kind representing no clip mask, at the ClipTaskIndex::INVALID
        // location.
        self.clip_mask_instances.clear();
        self.clip_mask_instances.push(ClipMaskKind::None);

        self.border_cache_handles.clear();

        // TODO(gw): As in the previous code, the gradient tiles store GPU cache
        //           handles that are cleared (and thus invalidated + re-uploaded)
        //           every frame. This maintains the existing behavior, but we
        //           should fix this in the future to retain handles.
        self.gradient_tiles.clear();

        self.prim_info.clear();

        self.debug_items.clear();
    }

    #[allow(dead_code)]
    pub fn push_debug_rect(
        &mut self,
        rect: DeviceRect,
        color: ColorF,
    ) {
        self.debug_items.push(DebugItem::Rect {
            rect,
            color: color.into(),
        });
    }

    #[allow(dead_code)]
    pub fn push_debug_string(
        &mut self,
        position: DevicePoint,
        color: ColorF,
        msg: String,
    ) {
        self.debug_items.push(DebugItem::Text {
            position,
            color: color.into(),
            msg,
        });
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Debug)]
pub struct PrimitiveStoreStats {
    picture_count: usize,
    text_run_count: usize,
    opacity_binding_count: usize,
    image_count: usize,
}

impl PrimitiveStoreStats {
    pub fn empty() -> Self {
        PrimitiveStoreStats {
            picture_count: 0,
            text_run_count: 0,
            opacity_binding_count: 0,
            image_count: 0,
        }
    }
}

pub struct PrimitiveStore {
    pub pictures: Vec<PicturePrimitive>,
    pub text_runs: TextRunStorage,

    /// A list of image instances. These are stored separately as
    /// storing them inline in the instance makes the structure bigger
    /// for other types.
    pub images: ImageInstanceStorage,

    /// List of animated opacity bindings for a primitive.
    pub opacity_bindings: OpacityBindingStorage,
}

impl PrimitiveStore {
    pub fn new(stats: &PrimitiveStoreStats) -> PrimitiveStore {
        PrimitiveStore {
            pictures: Vec::with_capacity(stats.picture_count),
            text_runs: TextRunStorage::new(stats.text_run_count),
            images: ImageInstanceStorage::new(stats.image_count),
            opacity_bindings: OpacityBindingStorage::new(stats.opacity_binding_count),
        }
    }

    pub fn get_stats(&self) -> PrimitiveStoreStats {
        PrimitiveStoreStats {
            picture_count: self.pictures.len(),
            text_run_count: self.text_runs.len(),
            image_count: self.images.len(),
            opacity_binding_count: self.opacity_bindings.len(),
        }
    }

    #[allow(unused)]
    pub fn print_picture_tree(&self, root: PictureIndex) {
        use print_tree::PrintTree;
        let mut pt = PrintTree::new("picture tree");
        self.pictures[root.0].print(&self.pictures, root, &mut pt);
    }

    /// Destroy an existing primitive store. This is called just before
    /// a primitive store is replaced with a newly built scene.
    pub fn destroy(
        self,
        retained_tiles: &mut RetainedTiles,
        clip_scroll_tree: &ClipScrollTree,
    ) {
        for pic in self.pictures {
            pic.destroy(
                retained_tiles,
                clip_scroll_tree,
            );
        }
    }

    /// Returns the total count of primitive instances contained in pictures.
    pub fn prim_count(&self) -> usize {
        self.pictures
            .iter()
            .map(|p| p.prim_list.prim_instances.len())
            .sum()
    }

    /// Update a picture, determining surface configuration,
    /// rasterization roots, and (in future) whether there
    /// are cached surfaces that can be used by this picture.
    pub fn update_picture(
        &mut self,
        pic_index: PictureIndex,
        state: &mut PictureUpdateState,
        frame_context: &FrameBuildingContext,
        gpu_cache: &mut GpuCache,
        data_stores: &DataStores,
        clip_store: &ClipStore,
    ) {
        if let Some(children) = self.pictures[pic_index.0].pre_update(
            state,
            frame_context,
        ) {
            for child_pic_index in &children {
                self.update_picture(
                    *child_pic_index,
                    state,
                    frame_context,
                    gpu_cache,
                    data_stores,
                    clip_store,
                );
            }

            self.pictures[pic_index.0].post_update(
                children,
                state,
                frame_context,
                gpu_cache,
            );
        }
    }

    /// Update visibility pass - update each primitive visibility struct, and
    /// build the clip chain instance if appropriate.
    pub fn update_visibility(
        &mut self,
        pic_index: PictureIndex,
        parent_surface_index: SurfaceIndex,
        frame_context: &FrameVisibilityContext,
        frame_state: &mut FrameVisibilityState,
    ) {
        let (mut prim_list, surface_index, apply_local_clip_rect) = {
            let pic = &mut self.pictures[pic_index.0];

            let prim_list = mem::replace(&mut pic.prim_list, PrimitiveList::empty());
            let surface_index = match pic.raster_config {
                Some(ref raster_config) => raster_config.surface_index,
                None => parent_surface_index,
            };

            if let Some(mut tile_cache) = pic.tile_cache.take() {
                debug_assert!(frame_state.tile_cache.is_none());

                // If we have a tile cache for this picture, see if any of the
                // relative transforms have changed, which means we need to
                // re-map the dependencies of any child primitives.
                tile_cache.pre_update(
                    pic.local_rect,
                    frame_context,
                    frame_state,
                );

                frame_state.tile_cache = Some(tile_cache);
            }

            (prim_list, surface_index, pic.apply_local_clip_rect)
        };

        let surface = &frame_context.surfaces[surface_index.0 as usize];

        let mut map_local_to_surface = surface
            .map_local_to_surface
            .clone();

        let map_surface_to_world = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface.surface_spatial_node_index,
            frame_context.screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        for prim_instance in &mut prim_list.prim_instances {
            prim_instance.reset();

            if prim_instance.is_chased() {
                #[cfg(debug_assertions)]
                println!("\tpreparing {:?} in {:?}",
                    prim_instance.id, pic_index);
            }

            // Get the cluster and see if is visible
            if !prim_list.clusters[prim_instance.cluster_index.0 as usize].is_visible {
                continue;
            }

            let spatial_node = &frame_context
                .clip_scroll_tree
                .spatial_nodes[prim_instance.spatial_node_index.0 as usize];

            // TODO(gw): Although constructing these is cheap, they are often
            //           the same for many consecutive primitives, so it may
            //           be worth caching the most recent context.
            let prim_context = PrimitiveContext::new(
                spatial_node,
                prim_instance.spatial_node_index,
            );

            map_local_to_surface.set_target_spatial_node(
                prim_instance.spatial_node_index,
                frame_context.clip_scroll_tree,
            );

            let (is_passthrough, prim_local_rect, clip_node_collector) = match prim_instance.kind {
                PrimitiveInstanceKind::Picture { pic_index, .. } => {
                    if !self.pictures[pic_index.0].is_visible() {
                        continue;
                    }

                    if let Some(ref raster_config) = self.pictures[pic_index.0].raster_config {
                        if raster_config.establishes_raster_root {
                            let surface = &frame_context.surfaces[raster_config.surface_index.0 as usize];
                            frame_state.clip_store.push_raster_root(surface.surface_spatial_node_index);
                        }
                    }

                    self.update_visibility(
                        pic_index,
                        surface_index,
                        frame_context,
                        frame_state,
                    );

                    let pic = &self.pictures[pic_index.0];

                    let clip_node_collector = pic.raster_config.as_ref().and_then(|rc| {
                        if rc.establishes_raster_root {
                            Some(frame_state.clip_store.pop_raster_root())
                        } else {
                            None
                        }
                    });

                    (pic.raster_config.is_none(), pic.local_rect, clip_node_collector)
                }
                _ => {
                    let prim_data = &frame_state.data_stores.as_common_data(&prim_instance);

                    let prim_rect = LayoutRect::new(
                        prim_instance.prim_origin,
                        prim_data.prim_size,
                    );

                    (false, prim_rect, None)
                }
            };

            if is_passthrough {
                let vis_index = PrimitiveVisibilityIndex(frame_state.scratch.prim_info.len() as u32);

                frame_state.scratch.prim_info.push(
                    PrimitiveVisibility {
                        clipped_world_rect: WorldRect::max_rect(),
                        clip_chain: ClipChainInstance::empty(),
                        clip_task_index: ClipTaskIndex::INVALID,
                        combined_local_clip_rect: LayoutRect::zero(),
                    }
                );

                prim_instance.visibility_info = vis_index;
            } else {
                if prim_local_rect.size.width <= 0.0 || prim_local_rect.size.height <= 0.0 {
                    if prim_instance.is_chased() {
                        println!("\tculled for zero local rectangle");
                    }
                    continue;
                }

                // Inflate the local rect for this primitive by the inflation factor of
                // the picture context. This ensures that even if the primitive itself
                // is not visible, any effects from the blur radius will be correctly
                // taken into account.
                let inflation_factor = surface.inflation_factor;
                let local_rect = prim_local_rect
                    .inflate(inflation_factor, inflation_factor)
                    .intersection(&prim_instance.local_clip_rect);
                let local_rect = match local_rect {
                    Some(local_rect) => local_rect,
                    None => {
                        if prim_instance.is_chased() {
                            println!("\tculled for being out of the local clip rectangle: {:?}",
                                prim_instance.local_clip_rect);
                        }
                        continue;
                    }
                };

                if let Some(ref mut tile_cache) = frame_state.tile_cache {
                    if !tile_cache.update_prim_dependencies(
                        prim_instance,
                        prim_local_rect,
                        frame_context.clip_scroll_tree,
                        frame_state.data_stores,
                        &frame_state.clip_store.clip_chain_nodes,
                        &self.pictures,
                        frame_state.resource_cache,
                        &self.opacity_bindings,
                        &self.images,
                    ) {
                        prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                        continue;
                    }
                }

                let clip_chain = frame_state
                    .clip_store
                    .build_clip_chain_instance(
                        prim_instance,
                        local_rect,
                        prim_instance.local_clip_rect,
                        prim_context.spatial_node_index,
                        &map_local_to_surface,
                        &map_surface_to_world,
                        &frame_context.clip_scroll_tree,
                        frame_state.gpu_cache,
                        frame_state.resource_cache,
                        frame_context.device_pixel_scale,
                        &frame_context.screen_world_rect,
                        clip_node_collector.as_ref(),
                        &mut frame_state.data_stores.clip,
                    );

                let clip_chain = match clip_chain {
                    Some(clip_chain) => clip_chain,
                    None => {
                        if prim_instance.is_chased() {
                            println!("\tunable to build the clip chain, skipping");
                        }
                        prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                        continue;
                    }
                };

                if prim_instance.is_chased() {
                    println!("\teffective clip chain from {:?} {}",
                        clip_chain.clips_range,
                        if apply_local_clip_rect { "(applied)" } else { "" },
                    );
                }

                let combined_local_clip_rect = if apply_local_clip_rect {
                    clip_chain.local_clip_rect
                } else {
                    prim_instance.local_clip_rect
                };

                // Check if the clip bounding rect (in pic space) is visible on screen
                // This includes both the prim bounding rect + local prim clip rect!
                let world_rect = match map_surface_to_world.map(&clip_chain.pic_clip_rect) {
                    Some(world_rect) => world_rect,
                    None => {
                        continue;
                    }
                };

                let clipped_world_rect = match world_rect.intersection(&frame_context.screen_world_rect) {
                    Some(rect) => rect,
                    None => {
                        continue;
                    }
                };

                // When the debug display is enabled, paint a colored rectangle around each
                // primitive.
                if frame_context.debug_flags.contains(::api::DebugFlags::PRIMITIVE_DBG) {
                    let debug_color = match prim_instance.kind {
                        PrimitiveInstanceKind::Picture { .. } => ColorF::TRANSPARENT,
                        PrimitiveInstanceKind::TextRun { .. } => debug_colors::RED,
                        PrimitiveInstanceKind::LineDecoration { .. } => debug_colors::PURPLE,
                        PrimitiveInstanceKind::NormalBorder { .. } |
                        PrimitiveInstanceKind::ImageBorder { .. } => debug_colors::ORANGE,
                        PrimitiveInstanceKind::Rectangle { .. } => ColorF { r: 0.8, g: 0.8, b: 0.8, a: 0.5 },
                        PrimitiveInstanceKind::YuvImage { .. } => debug_colors::BLUE,
                        PrimitiveInstanceKind::Image { .. } => debug_colors::BLUE,
                        PrimitiveInstanceKind::LinearGradient { .. } => debug_colors::PINK,
                        PrimitiveInstanceKind::RadialGradient { .. } => debug_colors::PINK,
                        PrimitiveInstanceKind::Clear { .. } => debug_colors::CYAN,
                    };
                    if debug_color.a != 0.0 {
                        let debug_rect = clipped_world_rect * frame_context.device_pixel_scale;
                        frame_state.scratch.push_debug_rect(debug_rect, debug_color);
                    }
                }

                let vis_index = PrimitiveVisibilityIndex(frame_state.scratch.prim_info.len() as u32);

                frame_state.scratch.prim_info.push(
                    PrimitiveVisibility {
                        clipped_world_rect,
                        clip_chain,
                        clip_task_index: ClipTaskIndex::INVALID,
                        combined_local_clip_rect,
                    }
                );

                prim_instance.visibility_info = vis_index;
            }

        }

        let pic = &mut self.pictures[pic_index.0];

        if let Some(RasterConfig { composite_mode: PictureCompositeMode::TileCache { .. }, .. }) = pic.raster_config {
            let mut tile_cache = frame_state.tile_cache.take().unwrap();

            // Build the dirty region(s) for this tile cache.
            pic.local_clip_rect = tile_cache.post_update(
                frame_state.resource_cache,
                frame_state.gpu_cache,
                frame_context,
                frame_state.scratch,
            );

            pic.tile_cache = Some(tile_cache);
        }

        pic.prim_list = prim_list;
    }

    pub fn get_opacity_binding(
        &self,
        opacity_binding_index: OpacityBindingIndex,
    ) -> f32 {
        if opacity_binding_index == OpacityBindingIndex::INVALID {
            1.0
        } else {
            self.opacity_bindings[opacity_binding_index].current
        }
    }

    // Internal method that retrieves the primitive index of a primitive
    // that can be the target for collapsing parent opacity filters into.
    fn get_opacity_collapse_prim(
        &self,
        pic_index: PictureIndex,
    ) -> Option<PictureIndex> {
        let pic = &self.pictures[pic_index.0];

        // We can only collapse opacity if there is a single primitive, otherwise
        // the opacity needs to be applied to the primitives as a group.
        if pic.prim_list.prim_instances.len() != 1 {
            return None;
        }

        let prim_instance = &pic.prim_list.prim_instances[0];

        // For now, we only support opacity collapse on solid rects and images.
        // This covers the most common types of opacity filters that can be
        // handled by this optimization. In the future, we can easily extend
        // this to other primitives, such as text runs and gradients.
        match prim_instance.kind {
            // If we find a single rect or image, we can use that
            // as the primitive to collapse the opacity into.
            PrimitiveInstanceKind::Rectangle { .. } |
            PrimitiveInstanceKind::Image { .. } => {
                return Some(pic_index);
            }
            PrimitiveInstanceKind::Clear { .. } |
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::NormalBorder { .. } |
            PrimitiveInstanceKind::ImageBorder { .. } |
            PrimitiveInstanceKind::YuvImage { .. } |
            PrimitiveInstanceKind::LinearGradient { .. } |
            PrimitiveInstanceKind::RadialGradient { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                // These prims don't support opacity collapse
            }
            PrimitiveInstanceKind::Picture { pic_index, .. } => {
                let pic = &self.pictures[pic_index.0];

                // If we encounter a picture that is a pass-through
                // (i.e. no composite mode), then we can recurse into
                // that to try and find a primitive to collapse to.
                if pic.requested_composite_mode.is_none() {
                    return self.get_opacity_collapse_prim(pic_index);
                }
            }
        }

        None
    }

    // Apply any optimizations to drawing this picture. Currently,
    // we just support collapsing pictures with an opacity filter
    // by pushing that opacity value into the color of a primitive
    // if that picture contains one compatible primitive.
    pub fn optimize_picture_if_possible(
        &mut self,
        pic_index: PictureIndex,
    ) {
        // Only handle opacity filters for now.
        let binding = match self.pictures[pic_index.0].requested_composite_mode {
            Some(PictureCompositeMode::Filter(FilterOp::Opacity(binding, _))) => {
                binding
            }
            _ => {
                return;
            }
        };

        // See if this picture contains a single primitive that supports
        // opacity collapse.
        match self.get_opacity_collapse_prim(pic_index) {
            Some(pic_index) => {
                let pic = &mut self.pictures[pic_index.0];
                let prim_instance = &mut pic.prim_list.prim_instances[0];
                match prim_instance.kind {
                    PrimitiveInstanceKind::Image { image_instance_index, .. } => {
                        let image_instance = &mut self.images[image_instance_index];
                        // By this point, we know we should only have found a primitive
                        // that supports opacity collapse.
                        if image_instance.opacity_binding_index == OpacityBindingIndex::INVALID {
                            image_instance.opacity_binding_index = self.opacity_bindings.push(OpacityBinding::new());
                        }
                        let opacity_binding = &mut self.opacity_bindings[image_instance.opacity_binding_index];
                        opacity_binding.push(binding);
                    }
                    PrimitiveInstanceKind::Rectangle { ref mut opacity_binding_index, .. } => {
                        // By this point, we know we should only have found a primitive
                        // that supports opacity collapse.
                        if *opacity_binding_index == OpacityBindingIndex::INVALID {
                            *opacity_binding_index = self.opacity_bindings.push(OpacityBinding::new());
                        }
                        let opacity_binding = &mut self.opacity_bindings[*opacity_binding_index];
                        opacity_binding.push(binding);
                    }
                    _ => {
                        unreachable!();
                    }
                }
            }
            None => {
                return;
            }
        }

        // The opacity filter has been collapsed, so mark this picture
        // as a pass though. This means it will no longer allocate an
        // intermediate surface or incur an extra blend / blit. Instead,
        // the collapsed primitive will be drawn directly into the
        // parent picture.
        self.pictures[pic_index.0].requested_composite_mode = None;
    }

    pub fn prepare_prim_for_render(
        &mut self,
        prim_instance: &mut PrimitiveInstance,
        prim_context: &PrimitiveContext,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        plane_split_anchor: usize,
        data_stores: &mut DataStores,
        scratch: &mut PrimitiveScratchBuffer,
    ) -> bool {
        // If we have dependencies, we need to prepare them first, in order
        // to know the actual rect of this primitive.
        // For example, scrolling may affect the location of an item in
        // local space, which may force us to render this item on a larger
        // picture target, if being composited.
        let pic_info = {
            match prim_instance.kind {
                PrimitiveInstanceKind::Picture { pic_index ,.. } => {
                    let pic = &mut self.pictures[pic_index.0];

                    match pic.take_context(
                        pic_index,
                        pic_context.surface_spatial_node_index,
                        pic_context.raster_spatial_node_index,
                        pic_context.surface_index,
                        pic_context.allow_subpixel_aa,
                        frame_state,
                        frame_context,
                    ) {
                        Some(info) => Some(info),
                        None => {
                            if prim_instance.is_chased() {
                                println!("\tculled for carrying an invisible composite filter");
                            }

                            return false;
                        }
                    }
                }
                PrimitiveInstanceKind::TextRun { .. } |
                PrimitiveInstanceKind::Rectangle { .. } |
                PrimitiveInstanceKind::LineDecoration { .. } |
                PrimitiveInstanceKind::NormalBorder { .. } |
                PrimitiveInstanceKind::ImageBorder { .. } |
                PrimitiveInstanceKind::YuvImage { .. } |
                PrimitiveInstanceKind::Image { .. } |
                PrimitiveInstanceKind::LinearGradient { .. } |
                PrimitiveInstanceKind::RadialGradient { .. } |
                PrimitiveInstanceKind::Clear { .. } => {
                    None
                }
            }
        };

        let is_passthrough = match pic_info {
            Some((pic_context_for_children, mut pic_state_for_children, mut prim_list)) => {
                // Mark whether this picture has a complex coordinate system.
                let is_passthrough = pic_context_for_children.is_passthrough;

                self.prepare_primitives(
                    &mut prim_list,
                    &pic_context_for_children,
                    &mut pic_state_for_children,
                    frame_context,
                    frame_state,
                    data_stores,
                    scratch,
                );

                if !pic_state_for_children.is_cacheable {
                    pic_state.is_cacheable = false;
                }

                // Restore the dependencies (borrow check dance)
                self.pictures[pic_context_for_children.pic_index.0]
                    .restore_context(
                        prim_list,
                        pic_context_for_children,
                        pic_state_for_children,
                        frame_state,
                    );

                is_passthrough
            }
            None => {
                false
            }
        };

        if !is_passthrough {
            prim_instance.update_clip_task(
                prim_context,
                pic_context.raster_spatial_node_index,
                pic_context,
                pic_state,
                frame_context,
                frame_state,
                self,
                data_stores,
                scratch,
            );

            if prim_instance.is_chased() {
                println!("\tconsidered visible and ready with local pos {:?}", prim_instance.prim_origin);
            }
        }

        #[cfg(debug_assertions)]
        {
            prim_instance.prepared_frame_id = frame_state.render_tasks.frame_id();
        }

        pic_state.is_cacheable &= prim_instance.is_cacheable(
            &data_stores,
            frame_state.resource_cache,
        );

        match prim_instance.kind {
            PrimitiveInstanceKind::Picture { pic_index, .. } => {
                let pic = &mut self.pictures[pic_index.0];
                let prim_info = &scratch.prim_info[prim_instance.visibility_info.0 as usize];
                if pic.prepare_for_render(
                    pic_index,
                    prim_instance,
                    prim_info.clipped_world_rect,
                    pic_context.surface_index,
                    frame_context,
                    frame_state,
                ) {
                    if let Some(ref mut splitter) = pic_state.plane_splitter {
                        PicturePrimitive::add_split_plane(
                            splitter,
                            frame_state.transforms,
                            prim_instance,
                            pic.local_rect,
                            &prim_info.combined_local_clip_rect,
                            frame_context.screen_world_rect,
                            plane_split_anchor,
                        );
                    }
                } else {
                    prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                }

                if let Some(mut request) = frame_state.gpu_cache.request(&mut pic.gpu_location) {
                    request.push(PremultipliedColorF::WHITE);
                    request.push(PremultipliedColorF::WHITE);
                    request.push([
                        -1.0,       // -ve means use prim rect for stretch size
                        0.0,
                        0.0,
                        0.0,
                    ]);
                }
            }
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::Clear { .. } |
            PrimitiveInstanceKind::Rectangle { .. } |
            PrimitiveInstanceKind::NormalBorder { .. } |
            PrimitiveInstanceKind::ImageBorder { .. } |
            PrimitiveInstanceKind::YuvImage { .. } |
            PrimitiveInstanceKind::Image { .. } |
            PrimitiveInstanceKind::LinearGradient { .. } |
            PrimitiveInstanceKind::RadialGradient { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                self.prepare_interned_prim_for_render(
                    prim_instance,
                    prim_context,
                    pic_context,
                    frame_context,
                    frame_state,
                    data_stores,
                    scratch,
                );
            }
        }

        true
    }

    pub fn prepare_primitives(
        &mut self,
        prim_list: &mut PrimitiveList,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        data_stores: &mut DataStores,
        scratch: &mut PrimitiveScratchBuffer,
    ) {
        for (plane_split_anchor, prim_instance) in prim_list.prim_instances.iter_mut().enumerate() {
            if prim_instance.visibility_info == PrimitiveVisibilityIndex::INVALID {
                continue;
            }

            // The original clipped world rect was calculated during the initial visibility pass.
            // However, it's possible that the dirty rect has got smaller, if tiles were not
            // dirty. Intersecting with the dirty rect here eliminates preparing any primitives
            // outside the dirty rect, and reduces the size of any off-screen surface allocations
            // for clip masks / render tasks that we make.
            {
                let visibility_info = &mut scratch.prim_info[prim_instance.visibility_info.0 as usize];
                let dirty_region = frame_state.current_dirty_region();

                // Check if the primitive world rect intersects with the overall dirty rect first.
                match visibility_info.clipped_world_rect.intersection(&dirty_region.combined.world_rect) {
                    Some(rect) => {
                        // It does intersect the overall dirty rect, so it *might* be visible.
                        // Store this reduced rect here, which is used for clip mask and other
                        // render task size calculations. In future, we may consider creating multiple
                        // render task trees, one per dirty region.
                        visibility_info.clipped_world_rect = rect;

                        // If there is more than one dirty region, it's possible that this primitive
                        // is inside the overal dirty rect, but doesn't intersect any of the individual
                        // dirty rects. If that's the case, then we can skip drawing this primitive too.
                        if dirty_region.dirty_rects.len() > 1 {
                            let in_dirty_rects = dirty_region
                                .dirty_rects
                                .iter()
                                .any(|dirty_rect| {
                                    visibility_info.clipped_world_rect.intersects(&dirty_rect.world_rect)
                                });

                            if !in_dirty_rects {
                                prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                                continue;
                            }
                        }
                    }
                    None => {
                        // Outside the overall dirty rect, so can be skipped.
                        prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                        continue;
                    }
                }
            }

            let spatial_node = &frame_context
                .clip_scroll_tree
                .spatial_nodes[prim_instance.spatial_node_index.0 as usize];

            // TODO(gw): Although constructing these is cheap, they are often
            //           the same for many consecutive primitives, so it may
            //           be worth caching the most recent context.
            let prim_context = PrimitiveContext::new(
                spatial_node,
                prim_instance.spatial_node_index,
            );

            pic_state.map_local_to_pic.set_target_spatial_node(
                prim_instance.spatial_node_index,
                frame_context.clip_scroll_tree,
            );

            if self.prepare_prim_for_render(
                prim_instance,
                &prim_context,
                pic_context,
                pic_state,
                frame_context,
                frame_state,
                plane_split_anchor,
                data_stores,
                scratch,
            ) {
                frame_state.profile_counters.visible_primitives.inc();
            }
        }
    }

    /// Prepare an interned primitive for rendering, by requesting
    /// resources, render tasks etc. This is equivalent to the
    /// prepare_prim_for_render_inner call for old style primitives.
    fn prepare_interned_prim_for_render(
        &mut self,
        prim_instance: &mut PrimitiveInstance,
        prim_context: &PrimitiveContext,
        pic_context: &PictureContext,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        data_stores: &mut DataStores,
        scratch: &mut PrimitiveScratchBuffer,
    ) {
        let is_chased = prim_instance.is_chased();

        match &mut prim_instance.kind {
            PrimitiveInstanceKind::LineDecoration { data_handle, ref mut cache_handle, .. } => {
                let prim_data = &mut data_stores.line_decoration[*data_handle];
                let common_data = &mut prim_data.common;
                let line_dec_data = &mut prim_data.kind;

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                line_dec_data.update(common_data, frame_state);

                // Work out the device pixel size to be used to cache this line decoration.
                if is_chased {
                    println!("\tline decoration key={:?}", line_dec_data.cache_key);
                }

                // If we have a cache key, it's a wavy / dashed / dotted line. Otherwise, it's
                // a simple solid line.
                if let Some(cache_key) = line_dec_data.cache_key.as_ref() {
                    // TODO(gw): Do we ever need / want to support scales for text decorations
                    //           based on the current transform?
                    let scale_factor = TypedScale::new(1.0) * frame_context.device_pixel_scale;
                    let task_size = (LayoutSize::from_au(cache_key.size) * scale_factor).ceil().to_i32();

                    // Request a pre-rendered image task.
                    // TODO(gw): This match is a bit untidy, but it should disappear completely
                    //           once the prepare_prims and batching are unified. When that
                    //           happens, we can use the cache handle immediately, and not need
                    //           to temporarily store it in the primitive instance.
                    *cache_handle = Some(frame_state.resource_cache.request_render_task(
                        RenderTaskCacheKey {
                            size: task_size,
                            kind: RenderTaskCacheKeyKind::LineDecoration(cache_key.clone()),
                        },
                        frame_state.gpu_cache,
                        frame_state.render_tasks,
                        None,
                        false,
                        |render_tasks| {
                            let task = RenderTask::new_line_decoration(
                                task_size,
                                cache_key.style,
                                cache_key.orientation,
                                cache_key.wavy_line_thickness.to_f32_px(),
                                LayoutSize::from_au(cache_key.size),
                            );
                            render_tasks.add(task)
                        }
                    ));
                }
            }
            PrimitiveInstanceKind::TextRun { data_handle, run_index, .. } => {
                let prim_data = &mut data_stores.text_run[*data_handle];
                let run = &mut self.text_runs[*run_index];

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                prim_data.update(frame_state);

                // The transform only makes sense for screen space rasterization
                let transform = prim_context.spatial_node.world_content_transform.to_transform();
                let prim_offset = prim_instance.prim_origin.to_vector() - run.reference_frame_relative_offset;

                // TODO(gw): This match is a bit untidy, but it should disappear completely
                //           once the prepare_prims and batching are unified. When that
                //           happens, we can use the cache handle immediately, and not need
                //           to temporarily store it in the primitive instance.
                run.prepare_for_render(
                    prim_offset,
                    &prim_data.font,
                    &prim_data.glyphs,
                    frame_context.device_pixel_scale,
                    &transform,
                    pic_context,
                    frame_state.resource_cache,
                    frame_state.gpu_cache,
                    frame_state.render_tasks,
                    scratch,
                );
            }
            PrimitiveInstanceKind::Clear { data_handle, .. } => {
                let prim_data = &mut data_stores.prim[*data_handle];

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                prim_data.update(frame_state);
            }
            PrimitiveInstanceKind::NormalBorder { data_handle, ref mut cache_handles, .. } => {
                let prim_data = &mut data_stores.normal_border[*data_handle];
                let common_data = &mut prim_data.common;
                let border_data = &mut prim_data.kind;

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                border_data.update(common_data, frame_state);

                // TODO(gw): When drawing in screen raster mode, we should also incorporate a
                //           scale factor from the world transform to get an appropriately
                //           sized border task.
                let world_scale = LayoutToWorldScale::new(1.0);
                let mut scale = world_scale * frame_context.device_pixel_scale;
                let max_scale = get_max_scale_for_border(&border_data.border.radius,
                                                         &border_data.widths);
                scale.0 = scale.0.min(max_scale.0);

                // For each edge and corner, request the render task by content key
                // from the render task cache. This ensures that the render task for
                // this segment will be available for batching later in the frame.
                let mut handles: SmallVec<[RenderTaskCacheEntryHandle; 8]> = SmallVec::new();

                for segment in &border_data.border_segments {
                    // Update the cache key device size based on requested scale.
                    let cache_size = to_cache_size(segment.local_task_size * scale);
                    let cache_key = RenderTaskCacheKey {
                        kind: RenderTaskCacheKeyKind::BorderSegment(segment.cache_key.clone()),
                        size: cache_size,
                    };

                    handles.push(frame_state.resource_cache.request_render_task(
                        cache_key,
                        frame_state.gpu_cache,
                        frame_state.render_tasks,
                        None,
                        false,          // TODO(gw): We don't calculate opacity for borders yet!
                        |render_tasks| {
                            let task = RenderTask::new_border_segment(
                                cache_size,
                                build_border_instances(
                                    &segment.cache_key,
                                    cache_size,
                                    &border_data.border,
                                    scale,
                                ),
                            );

                            render_tasks.add(task)
                        }
                    ));
                }

                *cache_handles = scratch
                    .border_cache_handles
                    .extend(handles);
            }
            PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                let prim_data = &mut data_stores.image_border[*data_handle];

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                prim_data.kind.update(&mut prim_data.common, frame_state);
            }
            PrimitiveInstanceKind::Rectangle { data_handle, segment_instance_index, opacity_binding_index, .. } => {
                let prim_data = &mut data_stores.prim[*data_handle];

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                prim_data.update(frame_state);

                update_opacity_binding(
                    &mut self.opacity_bindings,
                    *opacity_binding_index,
                    frame_context.scene_properties,
                );

                write_segment(*segment_instance_index, frame_state, scratch, |request| {
                    prim_data.kind.write_prim_gpu_blocks(
                        request,
                    );
                });
            }
            PrimitiveInstanceKind::YuvImage { data_handle, segment_instance_index, .. } => {
                let yuv_image_data = &mut data_stores.yuv_image[*data_handle];

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                yuv_image_data.kind.update(&mut yuv_image_data.common, frame_state);

                write_segment(*segment_instance_index, frame_state, scratch, |request| {
                    yuv_image_data.kind.write_prim_gpu_blocks(request);
                });
            }
            PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
                let prim_data = &mut data_stores.image[*data_handle];
                let common_data = &mut prim_data.common;
                let image_data = &mut prim_data.kind;

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                image_data.update(common_data, frame_state);

                let image_instance = &mut self.images[*image_instance_index];

                update_opacity_binding(
                    &mut self.opacity_bindings,
                    image_instance.opacity_binding_index,
                    frame_context.scene_properties,
                );

                image_instance.visible_tiles.clear();

                let image_properties = frame_state
                    .resource_cache
                    .get_image_properties(image_data.key);

                if let Some(image_properties) = image_properties {
                    if let Some(tile_size) = image_properties.tiling {
                        let device_image_size = image_properties.descriptor.size;

                        // Tighten the clip rect because decomposing the repeated image can
                        // produce primitives that are partially covering the original image
                        // rect and we want to clip these extra parts out.
                        let prim_info = &scratch.prim_info[prim_instance.visibility_info.0 as usize];
                        let prim_rect = LayoutRect::new(
                            prim_instance.prim_origin,
                            common_data.prim_size,
                        );
                        let tight_clip_rect = prim_info
                            .combined_local_clip_rect
                            .intersection(&prim_rect).unwrap();

                        let visible_rect = compute_conservative_visible_rect(
                            prim_context,
                            &frame_state.current_dirty_region().combined.world_rect,
                            &tight_clip_rect
                        );

                        let base_edge_flags = edge_flags_for_tile_spacing(&image_data.tile_spacing);

                        let stride = image_data.stretch_size + image_data.tile_spacing;

                        let repetitions = ::image::repetitions(
                            &prim_rect,
                            &visible_rect,
                            stride,
                        );

                        let request = ImageRequest {
                            key: image_data.key,
                            rendering: image_data.image_rendering,
                            tile: None,
                        };

                        for Repetition { origin, edge_flags } in repetitions {
                            let edge_flags = base_edge_flags | edge_flags;

                            let image_rect = LayoutRect {
                                origin,
                                size: image_data.stretch_size,
                            };

                            let tiles = ::image::tiles(
                                &image_rect,
                                &visible_rect,
                                &device_image_size,
                                tile_size as i32,
                            );

                            for tile in tiles {
                                frame_state.resource_cache.request_image(
                                    request.with_tile(tile.offset),
                                    frame_state.gpu_cache,
                                );

                                let mut handle = GpuCacheHandle::new();
                                if let Some(mut request) = frame_state.gpu_cache.request(&mut handle) {
                                    request.push(PremultipliedColorF::WHITE);
                                    request.push(PremultipliedColorF::WHITE);
                                    request.push([tile.rect.size.width, tile.rect.size.height, 0.0, 0.0]);
                                }

                                image_instance.visible_tiles.push(VisibleImageTile {
                                    tile_offset: tile.offset,
                                    handle,
                                    edge_flags: tile.edge_flags & edge_flags,
                                    local_rect: tile.rect,
                                    local_clip_rect: tight_clip_rect,
                                });
                            }
                        }

                        if image_instance.visible_tiles.is_empty() {
                            // At this point if we don't have tiles to show it means we could probably
                            // have done a better a job at culling during an earlier stage.
                            // Clearing the screen rect has the effect of "culling out" the primitive
                            // from the point of view of the batch builder, and ensures we don't hit
                            // assertions later on because we didn't request any image.
                            prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                        }
                    }
                }

                write_segment(image_instance.segment_instance_index, frame_state, scratch, |request| {
                    image_data.write_prim_gpu_blocks(request);
                });
            }
            PrimitiveInstanceKind::LinearGradient { data_handle, ref mut visible_tiles_range, .. } => {
                let prim_data = &mut data_stores.linear_grad[*data_handle];

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                prim_data.update(frame_state);

                if prim_data.tile_spacing != LayoutSize::zero() {
                    let prim_info = &scratch.prim_info[prim_instance.visibility_info.0 as usize];
                    let prim_rect = LayoutRect::new(
                        prim_instance.prim_origin,
                        prim_data.common.prim_size,
                    );

                    *visible_tiles_range = decompose_repeated_primitive(
                        &prim_info.combined_local_clip_rect,
                        &prim_rect,
                        &prim_data.stretch_size,
                        &prim_data.tile_spacing,
                        prim_context,
                        frame_state,
                        &mut scratch.gradient_tiles,
                        &mut |_, mut request| {
                            request.push([
                                prim_data.start_point.x,
                                prim_data.start_point.y,
                                prim_data.end_point.x,
                                prim_data.end_point.y,
                            ]);
                            request.push([
                                pack_as_float(prim_data.extend_mode as u32),
                                prim_data.stretch_size.width,
                                prim_data.stretch_size.height,
                                0.0,
                            ]);
                        }
                    );

                    if visible_tiles_range.is_empty() {
                        prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                    }
                }

                // TODO(gw): Consider whether it's worth doing segment building
                //           for gradient primitives.
            }
            PrimitiveInstanceKind::RadialGradient { data_handle, ref mut visible_tiles_range, .. } => {
                let prim_data = &mut data_stores.radial_grad[*data_handle];

                // Update the template this instane references, which may refresh the GPU
                // cache with any shared template data.
                prim_data.update(frame_state);

                if prim_data.tile_spacing != LayoutSize::zero() {
                    let prim_info = &scratch.prim_info[prim_instance.visibility_info.0 as usize];
                    let prim_rect = LayoutRect::new(
                        prim_instance.prim_origin,
                        prim_data.common.prim_size,
                    );

                    *visible_tiles_range = decompose_repeated_primitive(
                        &prim_info.combined_local_clip_rect,
                        &prim_rect,
                        &prim_data.stretch_size,
                        &prim_data.tile_spacing,
                        prim_context,
                        frame_state,
                        &mut scratch.gradient_tiles,
                        &mut |_, mut request| {
                            request.push([
                                prim_data.center.x,
                                prim_data.center.y,
                                prim_data.params.start_radius,
                                prim_data.params.end_radius,
                            ]);
                            request.push([
                                prim_data.params.ratio_xy,
                                pack_as_float(prim_data.extend_mode as u32),
                                prim_data.stretch_size.width,
                                prim_data.stretch_size.height,
                            ]);
                        },
                    );

                    if visible_tiles_range.is_empty() {
                        prim_instance.visibility_info = PrimitiveVisibilityIndex::INVALID;
                    }
                }

                // TODO(gw): Consider whether it's worth doing segment building
                //           for gradient primitives.
            }
            _ => {
                unreachable!();
            }
        };
    }
}

fn write_segment<F>(
    segment_instance_index: SegmentInstanceIndex,
    frame_state: &mut FrameBuildingState,
    scratch: &mut PrimitiveScratchBuffer,
    f: F,
) where F: Fn(&mut GpuDataRequest) {
    debug_assert!(segment_instance_index != SegmentInstanceIndex::INVALID);
    if segment_instance_index != SegmentInstanceIndex::UNUSED {
        let segment_instance = &mut scratch.segment_instances[segment_instance_index];

        if let Some(mut request) = frame_state.gpu_cache.request(&mut segment_instance.gpu_cache_handle) {
            let segments = &scratch.segments[segment_instance.segments_range];

            f(&mut request);

            for segment in segments {
                request.write_segment(
                    segment.local_rect,
                    [0.0; 4],
                );
            }
        }
    }
}

fn decompose_repeated_primitive(
    combined_local_clip_rect: &LayoutRect,
    prim_local_rect: &LayoutRect,
    stretch_size: &LayoutSize,
    tile_spacing: &LayoutSize,
    prim_context: &PrimitiveContext,
    frame_state: &mut FrameBuildingState,
    gradient_tiles: &mut GradientTileStorage,
    callback: &mut FnMut(&LayoutRect, GpuDataRequest),
) -> GradientTileRange {
    let mut visible_tiles = Vec::new();
    let world_rect = frame_state.current_dirty_region().combined.world_rect;

    // Tighten the clip rect because decomposing the repeated image can
    // produce primitives that are partially covering the original image
    // rect and we want to clip these extra parts out.
    let tight_clip_rect = combined_local_clip_rect
        .intersection(prim_local_rect).unwrap();

    let visible_rect = compute_conservative_visible_rect(
        prim_context,
        &world_rect,
        &tight_clip_rect
    );
    let stride = *stretch_size + *tile_spacing;

    let repetitions = ::image::repetitions(prim_local_rect, &visible_rect, stride);
    for Repetition { origin, .. } in repetitions {
        let mut handle = GpuCacheHandle::new();
        let rect = LayoutRect {
            origin: origin,
            size: *stretch_size,
        };

        if let Some(request) = frame_state.gpu_cache.request(&mut handle) {
            callback(&rect, request);
        }

        visible_tiles.push(VisibleGradientTile {
            local_rect: rect,
            local_clip_rect: tight_clip_rect,
            handle
        });
    }

    // At this point if we don't have tiles to show it means we could probably
    // have done a better a job at culling during an earlier stage.
    // Clearing the screen rect has the effect of "culling out" the primitive
    // from the point of view of the batch builder, and ensures we don't hit
    // assertions later on because we didn't request any image.
    if visible_tiles.is_empty() {
        GradientTileRange::empty()
    } else {
        gradient_tiles.extend(visible_tiles)
    }
}

fn compute_conservative_visible_rect(
    prim_context: &PrimitiveContext,
    world_rect: &WorldRect,
    local_clip_rect: &LayoutRect,
) -> LayoutRect {
    if let Some(layer_screen_rect) = prim_context
        .spatial_node
        .world_content_transform
        .unapply(world_rect) {

        return local_clip_rect.intersection(&layer_screen_rect).unwrap_or(LayoutRect::zero());
    }

    *local_clip_rect
}

fn edge_flags_for_tile_spacing(tile_spacing: &LayoutSize) -> EdgeAaSegmentMask {
    let mut flags = EdgeAaSegmentMask::empty();

    if tile_spacing.width > 0.0 {
        flags |= EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::RIGHT;
    }
    if tile_spacing.height > 0.0 {
        flags |= EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::BOTTOM;
    }

    flags
}

impl<'a> GpuDataRequest<'a> {
    // Write the GPU cache data for an individual segment.
    fn write_segment(
        &mut self,
        local_rect: LayoutRect,
        extra_data: [f32; 4],
    ) {
        let _ = VECS_PER_SEGMENT;
        self.push(local_rect);
        self.push(extra_data);
    }
}

    fn write_brush_segment_description(
        prim_local_rect: LayoutRect,
        prim_local_clip_rect: LayoutRect,
        clip_chain: &ClipChainInstance,
        segment_builder: &mut SegmentBuilder,
        clip_store: &ClipStore,
        data_stores: &DataStores,
    ) -> bool {
        // If the brush is small, we generally want to skip building segments
        // and just draw it as a single primitive with clip mask. However,
        // if the clips are purely rectangles that have no per-fragment
        // clip masks, we will segment anyway. This allows us to completely
        // skip allocating a clip mask in these cases.
        let is_large = prim_local_rect.size.area() > MIN_BRUSH_SPLIT_AREA;

        // TODO(gw): We should probably detect and store this on each
        //           ClipSources instance, to avoid having to iterate
        //           the clip sources here.
        let mut rect_clips_only = true;

        segment_builder.initialize(
            prim_local_rect,
            None,
            prim_local_clip_rect
        );

        // Segment the primitive on all the local-space clip sources that we can.
        let mut local_clip_count = 0;
        for i in 0 .. clip_chain.clips_range.count {
            let clip_instance = clip_store
                .get_instance_from_range(&clip_chain.clips_range, i);
            let clip_node = &data_stores.clip[clip_instance.handle];

            // If this clip item is positioned by another positioning node, its relative position
            // could change during scrolling. This means that we would need to resegment. Instead
            // of doing that, only segment with clips that have the same positioning node.
            // TODO(mrobinson, #2858): It may make sense to include these nodes, resegmenting only
            // when necessary while scrolling.
            if !clip_instance.flags.contains(ClipNodeFlags::SAME_SPATIAL_NODE) {
                continue;
            }

            local_clip_count += 1;

            let (local_clip_rect, radius, mode) = match clip_node.item {
                ClipItem::RoundedRectangle(size, radii, clip_mode) => {
                    rect_clips_only = false;
                    (LayoutRect::new(clip_instance.local_pos, size), Some(radii), clip_mode)
                }
                ClipItem::Rectangle(size, mode) => {
                    (LayoutRect::new(clip_instance.local_pos, size), None, mode)
                }
                ClipItem::BoxShadow(ref info) => {
                    rect_clips_only = false;

                    // For inset box shadows, we can clip out any
                    // pixels that are inside the shadow region
                    // and are beyond the inner rect, as they can't
                    // be affected by the blur radius.
                    let inner_clip_mode = match info.clip_mode {
                        BoxShadowClipMode::Outset => None,
                        BoxShadowClipMode::Inset => Some(ClipMode::ClipOut),
                    };

                    // Push a region into the segment builder where the
                    // box-shadow can have an effect on the result. This
                    // ensures clip-mask tasks get allocated for these
                    // pixel regions, even if no other clips affect them.
                    let prim_shadow_rect = info.prim_shadow_rect.translate(
                        &LayoutVector2D::new(clip_instance.local_pos.x, clip_instance.local_pos.y),
                    );
                    segment_builder.push_mask_region(
                        prim_shadow_rect,
                        prim_shadow_rect.inflate(
                            -0.5 * info.original_alloc_size.width,
                            -0.5 * info.original_alloc_size.height,
                        ),
                        inner_clip_mode,
                    );

                    continue;
                }
                ClipItem::Image { .. } => {
                    rect_clips_only = false;
                    continue;
                }
            };

            segment_builder.push_clip_rect(local_clip_rect, radius, mode);
        }

        if is_large || rect_clips_only {
            // If there were no local clips, then we will subdivide the primitive into
            // a uniform grid (up to 8x8 segments). This will typically result in
            // a significant number of those segments either being completely clipped,
            // or determined to not need a clip mask for that segment.
            if local_clip_count == 0 && clip_chain.clips_range.count > 0 {
                let x_clip_count = cmp::min(8, (prim_local_rect.size.width / 128.0).ceil() as i32);
                let y_clip_count = cmp::min(8, (prim_local_rect.size.height / 128.0).ceil() as i32);

                for y in 0 .. y_clip_count {
                    let y0 = prim_local_rect.size.height * y as f32 / y_clip_count as f32;
                    let y1 = prim_local_rect.size.height * (y+1) as f32 / y_clip_count as f32;

                    for x in 0 .. x_clip_count {
                        let x0 = prim_local_rect.size.width * x as f32 / x_clip_count as f32;
                        let x1 = prim_local_rect.size.width * (x+1) as f32 / x_clip_count as f32;

                        let rect = LayoutRect::new(
                            LayoutPoint::new(
                                x0 + prim_local_rect.origin.x,
                                y0 + prim_local_rect.origin.y,
                            ),
                            LayoutSize::new(
                                x1 - x0,
                                y1 - y0,
                            ),
                        );

                        segment_builder.push_mask_region(rect, LayoutRect::zero(), None);
                    }
                }
            }

            return true
        }

        false
    }

impl PrimitiveInstance {
    fn build_segments_if_needed(
        &mut self,
        prim_clip_chain: &ClipChainInstance,
        frame_state: &mut FrameBuildingState,
        prim_store: &mut PrimitiveStore,
        data_stores: &DataStores,
        segments_store: &mut SegmentStorage,
        segment_instances_store: &mut SegmentInstanceStorage,
    ) {
        let prim_data = &data_stores.as_common_data(self);
        let prim_local_rect = LayoutRect::new(
            self.prim_origin,
            prim_data.prim_size,
        );

        let segment_instance_index = match self.kind {
            PrimitiveInstanceKind::Rectangle { ref mut segment_instance_index, .. } |
            PrimitiveInstanceKind::YuvImage { ref mut segment_instance_index, .. } => {
                segment_instance_index
            }
            PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
                let image_data = &data_stores.image[data_handle].kind;
                let image_instance = &mut prim_store.images[image_instance_index];
                // tiled images don't support segmentation
                if frame_state
                    .resource_cache
                    .get_image_properties(image_data.key)
                    .and_then(|properties| properties.tiling)
                    .is_some() {
                        image_instance.segment_instance_index = SegmentInstanceIndex::UNUSED;
                        return;
                    }
                &mut image_instance.segment_instance_index
            }
            PrimitiveInstanceKind::Picture { .. } |
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::NormalBorder { .. } |
            PrimitiveInstanceKind::ImageBorder { .. } |
            PrimitiveInstanceKind::Clear { .. } |
            PrimitiveInstanceKind::LinearGradient { .. } |
            PrimitiveInstanceKind::RadialGradient { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                // These primitives don't support / need segments.
                return;
            }
        };

        if *segment_instance_index == SegmentInstanceIndex::INVALID {
            let mut segments: SmallVec<[BrushSegment; 8]> = SmallVec::new();

            if write_brush_segment_description(
                prim_local_rect,
                self.local_clip_rect,
                prim_clip_chain,
                &mut frame_state.segment_builder,
                frame_state.clip_store,
                data_stores,
            ) {
                frame_state.segment_builder.build(|segment| {
                    segments.push(
                        BrushSegment::new(
                            segment.rect.translate(&LayoutVector2D::new(-prim_local_rect.origin.x, -prim_local_rect.origin.y)),
                            segment.has_mask,
                            segment.edge_flags,
                            [0.0; 4],
                            BrushFlags::PERSPECTIVE_INTERPOLATION,
                        ),
                    );
                });
            }

            if segments.is_empty() {
                *segment_instance_index = SegmentInstanceIndex::UNUSED;
            } else {
                let segments_range = segments_store.extend(segments);

                let instance = SegmentedInstance {
                    segments_range,
                    gpu_cache_handle: GpuCacheHandle::new(),
                };

                *segment_instance_index = segment_instances_store.push(instance);
            };
        }
    }

    fn update_clip_task_for_brush(
        &self,
        prim_info: &mut PrimitiveVisibility,
        root_spatial_node_index: SpatialNodeIndex,
        prim_context: &PrimitiveContext,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        prim_store: &PrimitiveStore,
        data_stores: &mut DataStores,
        segments_store: &mut SegmentStorage,
        segment_instances_store: &mut SegmentInstanceStorage,
        clip_mask_instances: &mut Vec<ClipMaskKind>,
    ) -> bool {
        let segments = match self.kind {
            PrimitiveInstanceKind::Picture { .. } |
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::Clear { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                return false;
            }
            PrimitiveInstanceKind::Image { image_instance_index, .. } => {
                let segment_instance_index = prim_store
                    .images[image_instance_index]
                    .segment_instance_index;

                if segment_instance_index == SegmentInstanceIndex::UNUSED {
                    return false;
                }

                let segment_instance = &segment_instances_store[segment_instance_index];

                &segments_store[segment_instance.segments_range]
            }
            PrimitiveInstanceKind::YuvImage { segment_instance_index, .. } |
            PrimitiveInstanceKind::Rectangle { segment_instance_index, .. } => {
                debug_assert!(segment_instance_index != SegmentInstanceIndex::INVALID);

                if segment_instance_index == SegmentInstanceIndex::UNUSED {
                    return false;
                }

                let segment_instance = &segment_instances_store[segment_instance_index];

                &segments_store[segment_instance.segments_range]
            }
            PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                let border_data = &data_stores.image_border[data_handle].kind;

                // TODO: This is quite messy - once we remove legacy primitives we
                //       can change this to be a tuple match on (instance, template)
                border_data.brush_segments.as_slice()
            }
            PrimitiveInstanceKind::NormalBorder { data_handle, .. } => {
                let border_data = &data_stores.normal_border[data_handle].kind;

                // TODO: This is quite messy - once we remove legacy primitives we
                //       can change this to be a tuple match on (instance, template)
                border_data.brush_segments.as_slice()
            }
            PrimitiveInstanceKind::LinearGradient { data_handle, .. } => {
                let prim_data = &data_stores.linear_grad[data_handle];

                // TODO: This is quite messy - once we remove legacy primitives we
                //       can change this to be a tuple match on (instance, template)
                if prim_data.brush_segments.is_empty() {
                    return false;
                }

                prim_data.brush_segments.as_slice()
            }
            PrimitiveInstanceKind::RadialGradient { data_handle, .. } => {
                let prim_data = &data_stores.radial_grad[data_handle];

                // TODO: This is quite messy - once we remove legacy primitives we
                //       can change this to be a tuple match on (instance, template)
                if prim_data.brush_segments.is_empty() {
                    return false;
                }

                prim_data.brush_segments.as_slice()
            }
        };

        // If there are no segments, early out to avoid setting a valid
        // clip task instance location below.
        if segments.is_empty() {
            return true;
        }

        // Set where in the clip mask instances array the clip mask info
        // can be found for this primitive. Each segment will push the
        // clip mask information for itself in update_clip_task below.
        prim_info.clip_task_index = ClipTaskIndex(clip_mask_instances.len() as _);

        // If we only built 1 segment, there is no point in re-running
        // the clip chain builder. Instead, just use the clip chain
        // instance that was built for the main primitive. This is a
        // significant optimization for the common case.
        if segments.len() == 1 {
            let clip_mask_kind = segments[0].update_clip_task(
                Some(&prim_info.clip_chain),
                prim_info.clipped_world_rect,
                root_spatial_node_index,
                pic_context.surface_index,
                pic_state,
                frame_context,
                frame_state,
                &mut data_stores.clip,
            );
            clip_mask_instances.push(clip_mask_kind);
        } else {
            let dirty_world_rect = frame_state.current_dirty_region().combined.world_rect;

            for segment in segments {
                // Build a clip chain for the smaller segment rect. This will
                // often manage to eliminate most/all clips, and sometimes
                // clip the segment completely.

                let segment_clip_chain = frame_state
                    .clip_store
                    .build_clip_chain_instance(
                        self,
                        segment.local_rect.translate(&LayoutVector2D::new(
                            self.prim_origin.x,
                            self.prim_origin.y,
                        )),
                        self.local_clip_rect,
                        prim_context.spatial_node_index,
                        &pic_state.map_local_to_pic,
                        &pic_state.map_pic_to_world,
                        &frame_context.clip_scroll_tree,
                        frame_state.gpu_cache,
                        frame_state.resource_cache,
                        frame_context.device_pixel_scale,
                        &dirty_world_rect,
                        None,
                        &mut data_stores.clip,
                    );

                let clip_mask_kind = segment.update_clip_task(
                    segment_clip_chain.as_ref(),
                    prim_info.clipped_world_rect,
                    root_spatial_node_index,
                    pic_context.surface_index,
                    pic_state,
                    frame_context,
                    frame_state,
                    &mut data_stores.clip,
                );
                clip_mask_instances.push(clip_mask_kind);
            }
        }

        true
    }

    fn update_clip_task(
        &mut self,
        prim_context: &PrimitiveContext,
        root_spatial_node_index: SpatialNodeIndex,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        prim_store: &mut PrimitiveStore,
        data_stores: &mut DataStores,
        scratch: &mut PrimitiveScratchBuffer,
    ) {
        let prim_info = &mut scratch.prim_info[self.visibility_info.0 as usize];

        if self.is_chased() {
            println!("\tupdating clip task with pic rect {:?}", prim_info.clip_chain.pic_clip_rect);
        }

        self.build_segments_if_needed(
            &prim_info.clip_chain,
            frame_state,
            prim_store,
            data_stores,
            &mut scratch.segments,
            &mut scratch.segment_instances,
        );

        // First try to  render this primitive's mask using optimized brush rendering.
        if self.update_clip_task_for_brush(
            prim_info,
            root_spatial_node_index,
            prim_context,
            pic_context,
            pic_state,
            frame_context,
            frame_state,
            prim_store,
            data_stores,
            &mut scratch.segments,
            &mut scratch.segment_instances,
            &mut scratch.clip_mask_instances,
        ) {
            if self.is_chased() {
                println!("\tsegment tasks have been created for clipping");
            }
            return;
        }

        if prim_info.clip_chain.needs_mask {
            if let Some((device_rect, _)) = get_raster_rects(
                prim_info.clip_chain.pic_clip_rect,
                &pic_state.map_pic_to_raster,
                &pic_state.map_raster_to_world,
                prim_info.clipped_world_rect,
                frame_context.device_pixel_scale,
            ) {
                let clip_task = RenderTask::new_mask(
                    device_rect,
                    prim_info.clip_chain.clips_range,
                    root_spatial_node_index,
                    frame_state.clip_store,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_state.render_tasks,
                    &mut data_stores.clip,
                );

                let clip_task_id = frame_state.render_tasks.add(clip_task);
                if self.is_chased() {
                    println!("\tcreated task {:?} with device rect {:?}",
                        clip_task_id, device_rect);
                }
                // Set the global clip mask instance for this primitive.
                let clip_task_index = ClipTaskIndex(scratch.clip_mask_instances.len() as _);
                scratch.clip_mask_instances.push(ClipMaskKind::Mask(clip_task_id));
                prim_info.clip_task_index = clip_task_index;
                frame_state.surfaces[pic_context.surface_index.0].tasks.push(clip_task_id);
            }
        }
    }
}

pub fn get_raster_rects(
    pic_rect: PictureRect,
    map_to_raster: &SpaceMapper<PicturePixel, RasterPixel>,
    map_to_world: &SpaceMapper<RasterPixel, WorldPixel>,
    prim_bounding_rect: WorldRect,
    device_pixel_scale: DevicePixelScale,
) -> Option<(DeviceIntRect, DeviceRect)> {
    let unclipped_raster_rect = map_to_raster.map(&pic_rect)?;

    let unclipped = raster_rect_to_device_pixels(
        unclipped_raster_rect,
        device_pixel_scale,
    );

    let unclipped_world_rect = map_to_world.map(&unclipped_raster_rect)?;

    let clipped_world_rect = unclipped_world_rect.intersection(&prim_bounding_rect)?;

    let clipped_raster_rect = map_to_world.unmap(&clipped_world_rect)?;

    let clipped_raster_rect = clipped_raster_rect.intersection(&unclipped_raster_rect)?;

    let clipped = raster_rect_to_device_pixels(
        clipped_raster_rect,
        device_pixel_scale,
    );

    // Ensure that we won't try to allocate a zero-sized clip render task.
    if clipped.is_empty() {
        return None;
    }

    Some((clipped.to_i32(), unclipped))
}

/// Get the inline (horizontal) and block (vertical) sizes
/// for a given line decoration.
pub fn get_line_decoration_sizes(
    rect_size: &LayoutSize,
    orientation: LineOrientation,
    style: LineStyle,
    wavy_line_thickness: f32,
) -> Option<(f32, f32)> {
    let h = match orientation {
        LineOrientation::Horizontal => rect_size.height,
        LineOrientation::Vertical => rect_size.width,
    };

    // TODO(gw): The formulae below are based on the existing gecko and line
    //           shader code. They give reasonable results for most inputs,
    //           but could definitely do with a detailed pass to get better
    //           quality on a wider range of inputs!
    //           See nsCSSRendering::PaintDecorationLine in Gecko.

    match style {
        LineStyle::Solid => {
            None
        }
        LineStyle::Dashed => {
            let dash_length = (3.0 * h).min(64.0).max(1.0);

            Some((2.0 * dash_length, 4.0))
        }
        LineStyle::Dotted => {
            let diameter = h.min(64.0).max(1.0);
            let period = 2.0 * diameter;

            Some((period, diameter))
        }
        LineStyle::Wavy => {
            let line_thickness = wavy_line_thickness.max(1.0);
            let slope_length = h - line_thickness;
            let flat_length = ((line_thickness - 1.0) * 2.0).max(1.0);
            let approx_period = 2.0 * (slope_length + flat_length);

            Some((approx_period, h))
        }
    }
}

fn update_opacity_binding(
    opacity_bindings: &mut OpacityBindingStorage,
    opacity_binding_index: OpacityBindingIndex,
    scene_properties: &SceneProperties,
) -> f32 {
    if opacity_binding_index == OpacityBindingIndex::INVALID {
        1.0
    } else {
        let binding = &mut opacity_bindings[opacity_binding_index];
        binding.update(scene_properties);
        binding.current
    }
}

#[test]
#[cfg(target_pointer_width = "64")]
fn test_struct_sizes() {
    use std::mem;
    // The sizes of these structures are critical for performance on a number of
    // talos stress tests. If you get a failure here on CI, there's two possibilities:
    // (a) You made a structure smaller than it currently is. Great work! Update the
    //     test expectations and move on.
    // (b) You made a structure larger. This is not necessarily a problem, but should only
    //     be done with care, and after checking if talos performance regresses badly.
    assert_eq!(mem::size_of::<PrimitiveInstance>(), 96, "PrimitiveInstance size changed");
    assert_eq!(mem::size_of::<PrimitiveInstanceKind>(), 40, "PrimitiveInstanceKind size changed");
    assert_eq!(mem::size_of::<PrimitiveTemplate>(), 40, "PrimitiveTemplate size changed");
    assert_eq!(mem::size_of::<PrimitiveTemplateKind>(), 20, "PrimitiveTemplateKind size changed");
    assert_eq!(mem::size_of::<PrimitiveKey>(), 20, "PrimitiveKey size changed");
    assert_eq!(mem::size_of::<PrimitiveKeyKind>(), 5, "PrimitiveKeyKind size changed");
}

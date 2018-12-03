/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DebugFlags, DeviceIntPoint, DeviceIntRect, DeviceIntSize, DirtyRect, ImageDirtyRect};
use api::{ExternalImageType, ImageFormat};
use api::ImageDescriptor;
use device::{TextureFilter, total_gpu_bytes_allocated};
use freelist::{FreeList, FreeListHandle, UpsertResult, WeakFreeListHandle};
use gpu_cache::{GpuCache, GpuCacheHandle};
use gpu_types::{ImageSource, UvRectKind};
use internal_types::{CacheTextureId, LayerIndex, TextureUpdateList, TextureUpdateSource};
use internal_types::{TextureSource, TextureCacheAllocInfo, TextureCacheUpdate};
use profiler::{ResourceProfileCounter, TextureCacheProfileCounters};
use render_backend::{FrameId, FrameStamp};
use resource_cache::{CacheItem, CachedImageData};
use std::cell::Cell;
use std::cmp;
use std::mem;
use std::time::{Duration, SystemTime};
use std::rc::Rc;

/// The size of each region/layer in shared cache texture arrays.
const TEXTURE_REGION_DIMENSIONS: i32 = 512;

/// The number of pixels in a region. Derived from the above.
const TEXTURE_REGION_PIXELS: usize =
    (TEXTURE_REGION_DIMENSIONS as usize) * (TEXTURE_REGION_DIMENSIONS as usize);

/// Items in the texture cache can either be standalone textures,
/// or a sub-rect inside the shared cache.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
enum EntryDetails {
    Standalone,
    Cache {
        // Origin within the texture layer where this item exists.
        origin: DeviceIntPoint,
        // The layer index of the texture array.
        layer_index: usize,
    },
}

impl EntryDetails {
    /// Returns the kind associated with the details.
    fn kind(&self) -> EntryKind {
        match *self {
            EntryDetails::Standalone => EntryKind::Standalone,
            EntryDetails::Cache { .. } => EntryKind::Shared,
        }
    }
}

/// Tag identifying standalone-versus-shared, without the details.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum EntryKind {
    Standalone,
    Shared,
}

#[derive(Debug)]
pub enum CacheEntryMarker {}

// Stores information related to a single entry in the texture
// cache. This is stored for each item whether it's in the shared
// cache or a standalone texture.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct CacheEntry {
    /// Size the requested item, in device pixels.
    size: DeviceIntSize,
    /// Details specific to standalone or shared items.
    details: EntryDetails,
    /// Arbitrary user data associated with this item.
    user_data: [f32; 3],
    /// The last frame this item was requested for rendering.
    last_access: FrameStamp,
    /// Handle to the resource rect in the GPU cache.
    uv_rect_handle: GpuCacheHandle,
    /// Image format of the item.
    format: ImageFormat,
    filter: TextureFilter,
    /// The actual device texture ID this is part of.
    texture_id: CacheTextureId,
    /// Optional notice when the entry is evicted from the cache.
    eviction_notice: Option<EvictionNotice>,
    /// The type of UV rect this entry specifies.
    uv_rect_kind: UvRectKind,
    /// If set to `Auto` the cache entry may be evicted if unused for a number of frames.
    eviction: Eviction,
}

impl CacheEntry {
    // Create a new entry for a standalone texture.
    fn new_standalone(
        texture_id: CacheTextureId,
        last_access: FrameStamp,
        params: &CacheAllocParams,
    ) -> Self {
        CacheEntry {
            size: params.descriptor.size,
            user_data: params.user_data,
            last_access,
            details: EntryDetails::Standalone,
            texture_id,
            format: params.descriptor.format,
            filter: params.filter,
            uv_rect_handle: GpuCacheHandle::new(),
            eviction_notice: None,
            uv_rect_kind: params.uv_rect_kind,
            eviction: Eviction::Auto,
        }
    }

    // Update the GPU cache for this texture cache entry.
    // This ensures that the UV rect, and texture layer index
    // are up to date in the GPU cache for vertex shaders
    // to fetch from.
    fn update_gpu_cache(&mut self, gpu_cache: &mut GpuCache) {
        if let Some(mut request) = gpu_cache.request(&mut self.uv_rect_handle) {
            let (origin, layer_index) = match self.details {
                EntryDetails::Standalone { .. } => (DeviceIntPoint::zero(), 0.0),
                EntryDetails::Cache {
                    origin,
                    layer_index,
                    ..
                } => (origin, layer_index as f32),
            };
            let image_source = ImageSource {
                p0: origin.to_f32(),
                p1: (origin + self.size).to_f32(),
                texture_layer: layer_index,
                user_data: self.user_data,
                uv_rect_kind: self.uv_rect_kind,
            };
            image_source.write_gpu_blocks(&mut request);
        }
    }

    fn evict(&self) {
        if let Some(eviction_notice) = self.eviction_notice.as_ref() {
            eviction_notice.notify();
        }
    }
}


/// A texture cache handle is a weak reference to a cache entry.
///
/// If the handle has not been inserted into the cache yet, or if the entry was
/// previously inserted and then evicted, lookup of the handle will fail, and
/// the cache handle needs to re-upload this item to the texture cache (see
/// request() below).
pub type TextureCacheHandle = WeakFreeListHandle<CacheEntryMarker>;

/// Describes the eviction policy for a given entry in the texture cache.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum Eviction {
    /// The entry will be evicted under the normal rules (which differ between
    /// standalone and shared entries).
    Auto,
    /// The entry will not be evicted until the policy is explicitly set to a
    /// different value.
    Manual,
    /// The entry will be evicted if it was not used in the last frame.
    ///
    /// FIXME(bholley): Currently this only applies to the standalone case.
    Eager,
}

// An eviction notice is a shared condition useful for detecting
// when a TextureCacheHandle gets evicted from the TextureCache.
// It is optionally installed to the TextureCache when an update()
// is scheduled. A single notice may be shared among any number of
// TextureCacheHandle updates. The notice may then be subsequently
// checked to see if any of the updates using it have been evicted.
#[derive(Clone, Debug, Default)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct EvictionNotice {
    evicted: Rc<Cell<bool>>,
}

impl EvictionNotice {
    fn notify(&self) {
        self.evicted.set(true);
    }

    pub fn check(&self) -> bool {
        if self.evicted.get() {
            self.evicted.set(false);
            true
        } else {
            false
        }
    }
}

/// A set of lazily allocated, fixed size, texture arrays for each format the
/// texture cache supports.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct SharedTextures {
    array_rgba8_nearest: TextureArray,
    array_a8_linear: TextureArray,
    array_a16_linear: TextureArray,
    array_rgba8_linear: TextureArray,
}

impl SharedTextures {
    /// Mints a new set of shared textures.
    fn new() -> Self {
        Self {
            // Used primarily for cached shadow masks. There can be lots of
            // these on some pages like francine, but most pages don't use it
            // much.
            array_a8_linear: TextureArray::new(
                ImageFormat::R8,
                TextureFilter::Linear,
            ),
            // Used for experimental hdr yuv texture support, but not used in
            // production Firefox.
            array_a16_linear: TextureArray::new(
                ImageFormat::R16,
                TextureFilter::Linear,
            ),
            // The primary cache for images, glyphs, etc.
            array_rgba8_linear: TextureArray::new(
                ImageFormat::BGRA8,
                TextureFilter::Linear,
            ),
            // Used for image-rendering: crisp. This is mostly favicons, which
            // are small. Some other images use it too, but those tend to be
            // larger than 512x512 and thus don't use the shared cache anyway.
            array_rgba8_nearest: TextureArray::new(
                ImageFormat::BGRA8,
                TextureFilter::Nearest,
            ),
        }
    }

    /// Returns the cumulative number of GPU bytes consumed by all the shared textures.
    fn size_in_bytes(&self) -> usize {
        self.array_a8_linear.size_in_bytes() +
        self.array_a16_linear.size_in_bytes() +
        self.array_rgba8_linear.size_in_bytes() +
        self.array_rgba8_nearest.size_in_bytes()
    }

    /// Returns the cumulative number of GPU bytes consumed by empty regions.
    fn empty_region_bytes(&self) -> usize {
        self.array_a8_linear.empty_region_bytes() +
        self.array_a16_linear.empty_region_bytes() +
        self.array_rgba8_linear.empty_region_bytes() +
        self.array_rgba8_nearest.empty_region_bytes()
    }

    /// Clears each texture in the set, with the given set of pending updates.
    fn clear(&mut self, updates: &mut TextureUpdateList) {
        self.array_a8_linear.clear(updates);
        self.array_a16_linear.clear(updates);
        self.array_rgba8_linear.clear(updates);
        self.array_rgba8_nearest.clear(updates);
    }

    /// Returns a mutable borrow for the shared texture array matching the parameters.
    fn select(&mut self, format: ImageFormat, filter: TextureFilter) -> &mut TextureArray {
        match (format, filter) {
            (ImageFormat::R8, TextureFilter::Linear) => &mut self.array_a8_linear,
            (ImageFormat::R16, TextureFilter::Linear) => &mut self.array_a16_linear,
            (ImageFormat::BGRA8, TextureFilter::Linear) => &mut self.array_rgba8_linear,
            (ImageFormat::BGRA8, TextureFilter::Nearest) => &mut self.array_rgba8_nearest,
            (_, _) => unreachable!(),
        }
    }
}

/// Lists of strong handles owned by the texture cache. There is only one strong
/// handle for each entry, but unlimited weak handles. Consumers receive the weak
/// handles, and `TextureCache` owns the strong handles internally.
#[derive(Default)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct EntryHandles {
    /// Handles for each standalone texture cache entry.
    standalone: Vec<FreeListHandle<CacheEntryMarker>>,
    /// Handles for each shared texture cache entry.
    shared: Vec<FreeListHandle<CacheEntryMarker>>,
}

impl EntryHandles {
    /// Mutably borrows the requested handle list.
    fn select(&mut self, kind: EntryKind) -> &mut Vec<FreeListHandle<CacheEntryMarker>> {
        if kind == EntryKind::Standalone {
            &mut self.standalone
        } else {
            &mut self.shared
        }
    }
}

/// Container struct for the various parameters used in cache allocation.
struct CacheAllocParams {
    descriptor: ImageDescriptor,
    filter: TextureFilter,
    user_data: [f32; 3],
    uv_rect_kind: UvRectKind,
}

/// Criterion to determine whether a cache entry should be evicted. Generated
/// with `EvictionThresholdBuilder`.
///
/// Our eviction scheme is based on the age of the entry, both in terms of
/// number of frames and ellapsed time. It does not directly consider the size
/// of the entry, but may consider overall memory usage by WebRender, by making
/// eviction increasingly aggressive as overall memory usage increases.
///
/// Note that we don't just wrap a `FrameStamp` here, because `FrameStamp`
/// requires that if the id fields are the same, the time fields will be as
/// well. The pair of values in our eviction threshold generally do not match
/// the stamp of any actual frame, and the comparison semantics are also
/// different - so it's best to use a distinct type.
struct EvictionThreshold {
    id: FrameId,
    time: SystemTime,
}

impl EvictionThreshold {
    /// Returns true if the entry with the given access record should be evicted
    /// under this threshold.
    fn should_evict(&self, last_access: FrameStamp) -> bool {
        last_access.frame_id() < self.id &&
        last_access.time() < self.time
    }
}

/// Helper to generate an `EvictionThreshold` with the desired policy.
///
/// Without any constraints, the builder will generate a threshold that evicts
/// all frames other than the current one. Constraints are additive, i.e. setting
/// a frame limit and a time limit only evicts frames with an id and time each
/// less than the respective limits.
struct EvictionThresholdBuilder {
    now: FrameStamp,
    max_frames: Option<usize>,
    max_time_ms: Option<usize>,
    scale_by_pressure: bool,
}

impl EvictionThresholdBuilder {
    fn new(now: FrameStamp) -> Self {
        Self {
            now,
            max_frames: None,
            max_time_ms: None,
            scale_by_pressure: false,
        }
    }

    fn max_frames(mut self, frames: usize) -> Self {
        self.max_frames = Some(frames);
        self
    }

    fn max_time_s(mut self, seconds: usize) -> Self {
        self.max_time_ms = Some(seconds * 1000);
        self
    }

    fn scale_by_pressure(mut self) -> Self {
        self.scale_by_pressure = true;
        self
    }

    fn build(self) -> EvictionThreshold {
        const MAX_MEMORY_PRESSURE_BYTES: f64 = (500 * 1024 * 1024) as f64;
        // Compute the memory pressure factor in the range of [0, 1.0].
        let pressure_factor = if self.scale_by_pressure {
            let bytes_allocated = total_gpu_bytes_allocated() as f64;
            1.0 - (bytes_allocated / MAX_MEMORY_PRESSURE_BYTES).min(1.0)
        } else {
            1.0
        };

        // Compute the maximum period an entry can go unused before eviction.
        // If a category (frame or time) wasn't specified, we set the
        // threshold for that category to |now|, which lets the other category
        // be the deciding factor. If neither category is specified, we'll evict
        // everything but the current frame.
        //
        // Note that we need to clamp the frame id to avoid it going negative or
        // matching FrameId::INVALID early in execution. We don't need to clamp
        // the time because it's unix-epoch-relative.
        let max_frames = self.max_frames
            .map(|f| (f as f64 * pressure_factor) as usize)
            .unwrap_or(0)
            .min(self.now.frame_id().as_usize() - 1);
        let max_time_ms = self.max_time_ms
            .map(|f| (f as f64 * pressure_factor) as usize)
            .unwrap_or(0) as u64;

        EvictionThreshold {
            id: self.now.frame_id() - max_frames,
            time: self.now.time() - Duration::from_millis(max_time_ms),
        }
    }
}

/// General-purpose manager for images in GPU memory. This includes images,
/// rasterized glyphs, rasterized blobs, cached render tasks, etc.
///
/// The texture cache is owned and managed by the RenderBackend thread, and
/// produces a series of commands to manipulate the textures on the Renderer
/// thread. These commands are executed before any rendering is performed for
/// a given frame.
///
/// Entries in the texture cache are not guaranteed to live past the end of the
/// frame in which they are requested, and may be evicted. The API supports
/// querying whether an entry is still available.
///
/// The TextureCache is different from the GpuCache in that the former stores
/// images, whereas the latter stores data and parameters for use in the shaders.
/// This means that the texture cache can be visualized, which is a good way to
/// understand how it works. Enabling gfx.webrender.debug.texture-cache shows a
/// live view of its contents in Firefox.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextureCache {
    /// Set of texture arrays in different formats used for the shared cache.
    shared_textures: SharedTextures,

    /// Maximum texture size supported by hardware.
    max_texture_size: i32,

    /// Maximum number of texture layers supported by hardware.
    max_texture_layers: usize,

    /// The current set of debug flags.
    debug_flags: DebugFlags,

    /// The next unused virtual texture ID. Monotonically increasing.
    next_id: CacheTextureId,

    /// A list of allocations and updates that need to be applied to the texture
    /// cache in the rendering thread this frame.
    #[cfg_attr(all(feature = "serde", any(feature = "capture", feature = "replay")), serde(skip))]
    pending_updates: TextureUpdateList,

    /// The current `FrameStamp`. Used for cache eviction policies.
    now: FrameStamp,

    /// The last `FrameStamp` in which we expired the shared cache.
    last_shared_cache_expiration: FrameStamp,

    /// The time at which we first reached the byte threshold for reclaiming
    /// cache memory. `None if we haven't reached the threshold.
    reached_reclaim_threshold: Option<SystemTime>,

    /// Maintains the list of all current items in the texture cache.
    entries: FreeList<CacheEntry, CacheEntryMarker>,

    /// Strong handles for all entries allocated from the above `FreeList`.
    handles: EntryHandles,
}

impl TextureCache {
    pub fn new(max_texture_size: i32, mut max_texture_layers: usize) -> Self {
        if cfg!(target_os = "macos") {
            // On MBP integrated Intel GPUs, texture arrays appear to be
            // implemented as a single texture of stacked layers, and that
            // texture appears to be subject to the texture size limit. As such,
            // allocating more than 32 512x512 regions results in a dimension
            // longer than 16k (the max texture size), causing incorrect behavior.
            //
            // So we clamp the number of layers on mac. This results in maximum
            // texture array size of 32MB, which isn't ideal but isn't terrible
            // either. OpenGL on mac is not long for this earth, so this may be
            // good enough until we have WebRender on gfx-rs (on Metal).
            //
            // Note that we could also define this more generally in terms of
            // |max_texture_size / TEXTURE_REGION_DIMENSION|, except:
            //   * max_texture_size is actually clamped beyond the device limit
            //     by Gecko to 8192, so we'd need to thread the raw device value
            //     here, and:
            //   * The bug we're working around is likely specific to a single
            //     driver family, and those drivers are also likely to share
            //     the same max texture size of 16k. If we do encounter a driver
            //     with the same bug but a lower max texture size, we might need
            //     to rethink our strategy anyway, since a limit below 32MB might
            //     start to introduce performance issues.
            max_texture_layers = max_texture_layers.min(32);
        }

        TextureCache {
            shared_textures: SharedTextures::new(),
            max_texture_size,
            max_texture_layers,
            debug_flags: DebugFlags::empty(),
            next_id: CacheTextureId(1),
            pending_updates: TextureUpdateList::new(),
            now: FrameStamp::INVALID,
            last_shared_cache_expiration: FrameStamp::INVALID,
            reached_reclaim_threshold: None,
            entries: FreeList::new(),
            handles: EntryHandles::default(),
        }
    }

    /// Creates a TextureCache and sets it up with a valid `FrameStamp`, which
    /// is useful for avoiding panics when instantiating the `TextureCache`
    /// directly from unit test code.
    #[allow(dead_code)]
    pub fn new_for_testing(max_texture_size: i32, max_texture_layers: usize) -> Self {
        let mut cache = Self::new(max_texture_size, max_texture_layers);
        let mut now = FrameStamp::first();
        now.advance();
        cache.begin_frame(now);
        cache
    }

    pub fn set_debug_flags(&mut self, flags: DebugFlags) {
        self.debug_flags = flags;
    }

    pub fn clear(&mut self) {
        let standalone_entry_handles = mem::replace(
            &mut self.handles.standalone,
            Vec::new(),
        );

        for handle in standalone_entry_handles {
            let entry = self.entries.free(handle);
            entry.evict();
            self.free(entry);
        }

        let shared_entry_handles = mem::replace(
            &mut self.handles.shared,
            Vec::new(),
        );

        for handle in shared_entry_handles {
            let entry = self.entries.free(handle);
            entry.evict();
            self.free(entry);
        }

        assert!(self.entries.len() == 0);

        self.shared_textures.clear(&mut self.pending_updates);
    }

    /// Called at the beginning of each frame.
    pub fn begin_frame(&mut self, stamp: FrameStamp) {
        self.now = stamp;
        self.maybe_reclaim_shared_cache_memory();
    }

    /// Called at the beginning of each frame to periodically GC and reclaim
    /// storage if the cache has grown too large.
    fn maybe_reclaim_shared_cache_memory(&mut self) {
        // The minimum number of bytes that we must be able to reclaim in order
        // to justify clearing the entire shared cache in order to shrink it.
        const RECLAIM_THRESHOLD_BYTES: usize = 5 * 1024 * 1024;

        // Normally the shared cache only gets GCed when we fail to allocate.
        // However, we also perform a periodic, conservative GC to ensure that
        // we recover unused memory in bounded time, rather than having it
        // depend on allocation patterns of subsequent content.
        let time_since_last_gc = self.now.time()
            .duration_since(self.last_shared_cache_expiration.time())
            .unwrap_or(Duration::default());
        let do_periodic_gc = time_since_last_gc >= Duration::from_secs(5) &&
            self.shared_textures.size_in_bytes() >= RECLAIM_THRESHOLD_BYTES * 2;
        if do_periodic_gc {
            let threshold = EvictionThresholdBuilder::new(self.now)
                .max_frames(1)
                .max_time_s(10)
                .build();
            self.maybe_expire_old_shared_entries(threshold);
        }

        // If we've had a sufficient number of unused layers for a sufficiently
        // long time, just blow the whole cache away to shrink it.
        //
        // We could do this more intelligently with a resize+blit, but that would
        // add complexity for a rare case.
        if self.shared_textures.empty_region_bytes() >= RECLAIM_THRESHOLD_BYTES {
            self.reached_reclaim_threshold.get_or_insert(self.now.time());
        } else {
            self.reached_reclaim_threshold = None;
        }
        if let Some(t) = self.reached_reclaim_threshold {
            let dur = self.now.time().duration_since(t).unwrap_or(Duration::default());
            if dur >= Duration::from_secs(5) {
                self.clear();
                self.reached_reclaim_threshold = None;
            }
        }

    }

    pub fn end_frame(&mut self, texture_cache_profile: &mut TextureCacheProfileCounters) {
        // Expire standalone entries.
        //
        // Most of the time, standalone cache entries correspond to images whose
        // width or height is greater than the region size in the shared cache, i.e.
        // 512 pixels. Cached render tasks also frequently get standalone entries,
        // but those use the Eviction::Eager policy (for now). So the tradeoff there
        // is largely around reducing texture upload jank while keeping memory usage
        // at an acceptable level.
        let threshold = self.default_eviction();
        self.expire_old_entries(EntryKind::Standalone, threshold);

        self.shared_textures.array_a8_linear
            .update_profile(&mut texture_cache_profile.pages_a8_linear);
        self.shared_textures.array_a16_linear
            .update_profile(&mut texture_cache_profile.pages_a16_linear);
        self.shared_textures.array_rgba8_linear
            .update_profile(&mut texture_cache_profile.pages_rgba8_linear);
        self.shared_textures.array_rgba8_nearest
            .update_profile(&mut texture_cache_profile.pages_rgba8_nearest);
    }

    // Request an item in the texture cache. All images that will
    // be used on a frame *must* have request() called on their
    // handle, to update the last used timestamp and ensure
    // that resources are not flushed from the cache too early.
    //
    // Returns true if the image needs to be uploaded to the
    // texture cache (either never uploaded, or has been
    // evicted on a previous frame).
    pub fn request(&mut self, handle: &TextureCacheHandle, gpu_cache: &mut GpuCache) -> bool {
        match self.entries.get_opt_mut(handle) {
            // If an image is requested that is already in the cache,
            // refresh the GPU cache data associated with this item.
            Some(entry) => {
                entry.last_access = self.now;
                entry.update_gpu_cache(gpu_cache);
                false
            }
            None => true,
        }
    }

    // Returns true if the image needs to be uploaded to the
    // texture cache (either never uploaded, or has been
    // evicted on a previous frame).
    pub fn needs_upload(&self, handle: &TextureCacheHandle) -> bool {
        self.entries.get_opt(handle).is_none()
    }

    pub fn max_texture_size(&self) -> i32 {
        self.max_texture_size
    }

    #[allow(dead_code)]
    pub fn max_texture_layers(&self) -> usize {
        self.max_texture_layers
    }

    pub fn pending_updates(&mut self) -> TextureUpdateList {
        mem::replace(&mut self.pending_updates, TextureUpdateList::new())
    }

    // Update the data stored by a given texture cache handle.
    pub fn update(
        &mut self,
        handle: &mut TextureCacheHandle,
        descriptor: ImageDescriptor,
        filter: TextureFilter,
        data: Option<CachedImageData>,
        user_data: [f32; 3],
        mut dirty_rect: ImageDirtyRect,
        gpu_cache: &mut GpuCache,
        eviction_notice: Option<&EvictionNotice>,
        uv_rect_kind: UvRectKind,
        eviction: Eviction,
    ) {
        // Determine if we need to allocate texture cache memory
        // for this item. We need to reallocate if any of the following
        // is true:
        // - Never been in the cache
        // - Has been in the cache but was evicted.
        // - Exists in the cache but dimensions / format have changed.
        let realloc = match self.entries.get_opt(handle) {
            Some(entry) => {
                entry.size != descriptor.size || entry.format != descriptor.format
            }
            None => {
                // Not allocated, or was previously allocated but has been evicted.
                true
            }
        };

        if realloc {
            let params = CacheAllocParams { descriptor, filter, user_data, uv_rect_kind };
            self.allocate(&params, handle);

            // If we reallocated, we need to upload the whole item again.
            dirty_rect = DirtyRect::All;
        }

        let entry = self.entries.get_opt_mut(handle)
            .expect("BUG: handle must be valid now");

        // Install the new eviction notice for this update, if applicable.
        entry.eviction_notice = eviction_notice.cloned();
        entry.uv_rect_kind = uv_rect_kind;

        // Invalidate the contents of the resource rect in the GPU cache.
        // This ensures that the update_gpu_cache below will add
        // the new information to the GPU cache.
        gpu_cache.invalidate(&entry.uv_rect_handle);

        // Upload the resource rect and texture array layer.
        entry.update_gpu_cache(gpu_cache);

        entry.eviction = eviction;

        // Create an update command, which the render thread processes
        // to upload the new image data into the correct location
        // in GPU memory.
        if let Some(data) = data {
            let (layer_index, origin) = match entry.details {
                EntryDetails::Standalone { .. } => (0, DeviceIntPoint::zero()),
                EntryDetails::Cache {
                    layer_index,
                    origin,
                    ..
                } => (layer_index, origin),
            };

            let op = TextureCacheUpdate::new_update(
                data,
                &descriptor,
                origin,
                entry.size,
                entry.texture_id,
                layer_index as i32,
                &dirty_rect,
            );
            self.pending_updates.push_update(op);
        }
    }

    // Check if a given texture handle has a valid allocation
    // in the texture cache.
    pub fn is_allocated(&self, handle: &TextureCacheHandle) -> bool {
        self.entries.get_opt(handle).is_some()
    }

    // Retrieve the details of an item in the cache. This is used
    // during batch creation to provide the resource rect address
    // to the shaders and texture ID to the batching logic.
    // This function will assert in debug modes if the caller
    // tries to get a handle that was not requested this frame.
    pub fn get(&self, handle: &TextureCacheHandle) -> CacheItem {
        let entry = self.entries
            .get_opt(handle)
            .expect("BUG: was dropped from cache or not updated!");
        debug_assert_eq!(entry.last_access, self.now);
        let (layer_index, origin) = match entry.details {
            EntryDetails::Standalone { .. } => {
                (0, DeviceIntPoint::zero())
            }
            EntryDetails::Cache {
                layer_index,
                origin,
                ..
            } => (layer_index, origin),
        };
        CacheItem {
            uv_rect_handle: entry.uv_rect_handle,
            texture_id: TextureSource::TextureCache(entry.texture_id),
            uv_rect: DeviceIntRect::new(origin, entry.size),
            texture_layer: layer_index as i32,
        }
    }

    /// A more detailed version of get(). This allows access to the actual
    /// device rect of the cache allocation.
    ///
    /// Returns a tuple identifying the texture, the layer, and the region.
    pub fn get_cache_location(
        &self,
        handle: &TextureCacheHandle,
    ) -> (CacheTextureId, LayerIndex, DeviceIntRect) {
        let entry = self.entries
            .get_opt(handle)
            .expect("BUG: was dropped from cache or not updated!");
        debug_assert_eq!(entry.last_access, self.now);
        let (layer_index, origin) = match entry.details {
            EntryDetails::Standalone { .. } => {
                (0, DeviceIntPoint::zero())
            }
            EntryDetails::Cache {
                layer_index,
                origin,
                ..
            } => (layer_index, origin),
        };
        (entry.texture_id,
         layer_index as usize,
         DeviceIntRect::new(origin, entry.size))
    }

    pub fn mark_unused(&mut self, handle: &TextureCacheHandle) {
        if let Some(entry) = self.entries.get_opt_mut(handle) {
            // Set last accessed stamp invalid to ensure it gets cleaned up
            // next time we expire entries.
            entry.last_access = FrameStamp::INVALID;
            entry.eviction = Eviction::Auto;
        }
    }

    /// Returns the default eviction policy.
    ///
    /// These parameters come from very rough instrumentation of hits in the
    /// shared cache, with simple browsing on a few pages. In rough terms, more
    /// than 99.5% of cache hits occur for entries that were used in the previous
    /// frame. This is obviously the dominant case, but we still want good behavior
    /// in long-tail cases (i.e. a large image is scrolled off-screen and on again).
    /// If we exclude immediately-reused (first frame) entries, 70% of the remaining
    /// hits happen within the first 200 frames. So we can be relatively agressive
    /// about eviction without sacrificing much in terms of cache performance.
    /// The one wrinkle is that animation-heavy pages do tend to extend the
    /// distribution, presumably because they churn through FrameIds faster than
    /// their more-static counterparts. As such, we _also_ provide a time floor
    /// (which was not measured with the same degree of rigour).
    fn default_eviction(&self) -> EvictionThreshold {
        EvictionThresholdBuilder::new(self.now)
            .max_frames(200)
            .max_time_s(3)
            .scale_by_pressure()
            .build()
    }

    /// Shared eviction code for standalone and shared entries.
    ///
    /// See `EvictionThreshold` for more details on policy.
    fn expire_old_entries(&mut self, kind: EntryKind, threshold: EvictionThreshold) {
        // Iterate over the entries in reverse order, evicting the ones older than
        // the frame age threshold. Reverse order avoids iterator invalidation when
        // removing entries.
        for i in (0..self.handles.select(kind).len()).rev() {
            let evict = {
                let entry = self.entries.get(&self.handles.select(kind)[i]);
                match entry.eviction {
                    Eviction::Manual => false,
                    Eviction::Auto => threshold.should_evict(entry.last_access),
                    Eviction::Eager => {
                        // Texture cache entries can be evicted at the start of
                        // a frame, or at any time during the frame when a cache
                        // allocation is occurring. This means that entries tagged
                        // with eager eviction may get evicted before they have a
                        // chance to be requested on the current frame. Instead,
                        // advance the frame id of the entry by one before
                        // comparison. This ensures that an eager entry will
                        // not be evicted until it is not used for at least
                        // one complete frame.
                        let mut entry_frame_id = entry.last_access.frame_id();
                        entry_frame_id.advance();

                        entry_frame_id < self.now.frame_id()
                    }
                }
            };
            if evict {
                let handle = self.handles.select(kind).swap_remove(i);
                let entry = self.entries.free(handle);
                entry.evict();
                self.free(entry);
            }
        }
    }

    /// Expires old shared entries, if we haven't done so this frame.
    ///
    /// Returns true if any entries were expired.
    fn maybe_expire_old_shared_entries(&mut self, threshold: EvictionThreshold) -> bool {
        let old_len = self.handles.shared.len();
        if self.last_shared_cache_expiration.frame_id() < self.now.frame_id() {
            self.expire_old_entries(EntryKind::Shared, threshold);
            self.last_shared_cache_expiration = self.now;
        }
        self.handles.shared.len() != old_len
    }

    // Free a cache entry from the standalone list or shared cache.
    fn free(&mut self, entry: CacheEntry) {
        match entry.details {
            EntryDetails::Standalone { .. } => {
                // This is a standalone texture allocation. Free it directly.
                self.pending_updates.push_free(entry.texture_id);
            }
            EntryDetails::Cache {
                origin,
                layer_index,
            } => {
                // Free the block in the given region.
                let texture_array = self.shared_textures.select(entry.format, entry.filter);
                let region = &mut texture_array.regions[layer_index];

                if self.debug_flags.contains(
                    DebugFlags::TEXTURE_CACHE_DBG |
                    DebugFlags::TEXTURE_CACHE_DBG_CLEAR_EVICTED) {
                    self.pending_updates.push_debug_clear(
                        entry.texture_id,
                        origin,
                        region.slab_size.width,
                        region.slab_size.height,
                        layer_index
                    );
                }
                region.free(origin, &mut texture_array.empty_regions);
            }
        }
    }

    // Attempt to allocate a block from the shared cache.
    fn allocate_from_shared_cache(
        &mut self,
        params: &CacheAllocParams
    ) -> Option<CacheEntry> {
        // Mutably borrow the correct texture.
        let texture_array = self.shared_textures.select(
            params.descriptor.format,
            params.filter,
        );

        // Lazy initialize this texture array if required.
        if texture_array.texture_id.is_none() {
            assert!(texture_array.regions.is_empty());
            let texture_id = self.next_id;
            self.next_id.0 += 1;

            let info = TextureCacheAllocInfo {
                width: TEXTURE_REGION_DIMENSIONS,
                height: TEXTURE_REGION_DIMENSIONS,
                format: params.descriptor.format,
                filter: texture_array.filter,
                layer_count: 1,
                is_shared_cache: true,
            };
            self.pending_updates.push_alloc(texture_id, info);

            texture_array.texture_id = Some(texture_id);
            texture_array.push_region();
        }

        // Do the allocation. This can fail and return None
        // if there are no free slots or regions available.
        texture_array.alloc(params, self.now)
    }

    // Returns true if the given image descriptor *may* be
    // placed in the shared texture cache.
    pub fn is_allowed_in_shared_cache(
        &self,
        filter: TextureFilter,
        descriptor: &ImageDescriptor,
    ) -> bool {
        let mut allowed_in_shared_cache = true;

        // TODO(sotaro): For now, anything that requests RGBA8 just fails to allocate
        // in a texture page, and gets a standalone texture.
        if descriptor.format == ImageFormat::RGBA8 {
            allowed_in_shared_cache = false;
        }

        // TODO(gw): For now, anything that requests nearest filtering and isn't BGRA8
        //           just fails to allocate in a texture page, and gets a standalone
        //           texture. This is probably rare enough that it can be fixed up later.
        if filter == TextureFilter::Nearest &&
           descriptor.format != ImageFormat::BGRA8 {
            allowed_in_shared_cache = false;
        }

        // Anything larger than TEXTURE_REGION_DIMENSIONS goes in a standalone texture.
        // TODO(gw): If we find pages that suffer from batch breaks in this
        //           case, add support for storing these in a standalone
        //           texture array.
        if descriptor.size.width > TEXTURE_REGION_DIMENSIONS ||
           descriptor.size.height > TEXTURE_REGION_DIMENSIONS {
            allowed_in_shared_cache = false;
        }

        allowed_in_shared_cache
    }

    /// Allocates a new standalone cache entry.
    fn allocate_standalone_entry(
        &mut self,
        params: &CacheAllocParams,
    ) -> CacheEntry {
        let texture_id = self.next_id;
        self.next_id.0 += 1;

        // Push a command to allocate device storage of the right size / format.
        let info = TextureCacheAllocInfo {
            width: params.descriptor.size.width,
            height: params.descriptor.size.height,
            format: params.descriptor.format,
            filter: params.filter,
            layer_count: 1,
            is_shared_cache: false,
        };
        self.pending_updates.push_alloc(texture_id, info);

        return CacheEntry::new_standalone(
            texture_id,
            self.now,
            params,
        );
    }

    /// Allocates a cache entry appropriate for the given parameters.
    ///
    /// This allocates from the shared cache unless the parameters do not meet
    /// the shared cache requirements, in which case a standalone texture is
    /// used.
    fn allocate_cache_entry(
        &mut self,
        params: &CacheAllocParams,
    ) -> CacheEntry {
        assert!(params.descriptor.size.width > 0 && params.descriptor.size.height > 0);

        // If this image doesn't qualify to go in the shared (batching) cache,
        // allocate a standalone entry.
        if !self.is_allowed_in_shared_cache(params.filter, &params.descriptor) {
            return self.allocate_standalone_entry(params);
        }


        // Try allocating from the shared cache.
        if let Some(entry) = self.allocate_from_shared_cache(params) {
            return entry;
        }

        // If we failed to allocate and haven't GCed this frame, do so.
        //
        // If we hit our limit on layers in the shared cache, failing to
        // allocate will trigger standalone textures for every entry, including
        // tiny entries like glyphs. We really want to avoid this, so use a
        // maximally aggressive eviction threshold in that case (which
        // realistically should only happen on mac, where we have a tighter
        // layer limit).
        let num_regions = self.shared_textures
            .select(params.descriptor.format, params.filter).regions.len();
        let threshold = if num_regions == self.max_texture_layers {
            EvictionThresholdBuilder::new(self.now).max_frames(1).build()
        } else {
            self.default_eviction()
        };

        if self.maybe_expire_old_shared_entries(threshold) {
            if let Some(entry) = self.allocate_from_shared_cache(params) {
                return entry;
            }
        }

        let added_layer = {
            // If we've hit our layer limit, allocate standalone.
            let texture_array =
                self.shared_textures.select(params.descriptor.format, params.filter);
            // Add a layer, unless we've hit our limit.
            if num_regions < self.max_texture_layers as usize {
                let info = TextureCacheAllocInfo {
                    width: TEXTURE_REGION_DIMENSIONS,
                    height: TEXTURE_REGION_DIMENSIONS,
                    format: params.descriptor.format,
                    filter: texture_array.filter,
                    layer_count: (num_regions + 1) as i32,
                    is_shared_cache: true,
                };
                self.pending_updates.push_realloc(texture_array.texture_id.unwrap(), info);
                texture_array.push_region();
                true
            } else {
                false
            }
        };

        if added_layer {
            self.allocate_from_shared_cache(params)
                .expect("Allocation should succeed after adding a fresh layer")
        } else {
            self.allocate_standalone_entry(params)
        }
    }

    /// Allocates a cache entry for the given parameters, and updates the
    /// provided handle to point to the new entry.
    fn allocate(&mut self, params: &CacheAllocParams, handle: &mut TextureCacheHandle) {
        let new_cache_entry = self.allocate_cache_entry(params);
        let new_kind = new_cache_entry.details.kind();

        // If the handle points to a valid cache entry, we want to replace the
        // cache entry with our newly updated location. We also need to ensure
        // that the storage (region or standalone) associated with the previous
        // entry here gets freed.
        //
        // If the handle is invalid, we need to insert the data, and append the
        // result to the corresponding vector.
        //
        // This is managed with a database style upsert operation.
        match self.entries.upsert(handle, new_cache_entry) {
            UpsertResult::Updated(old_entry) => {
                if new_kind != old_entry.details.kind() {
                    // Handle the rare case than an update moves an entry from
                    // shared to standalone or vice versa. This involves a linear
                    // search, but should be rare enough not to matter.
                    let (from, to) = if new_kind == EntryKind::Standalone {
                        (&mut self.handles.shared, &mut self.handles.standalone)
                    } else {
                        (&mut self.handles.standalone, &mut self.handles.shared)
                    };
                    let idx = from.iter().position(|h| h.weak() == *handle).unwrap();
                    to.push(from.remove(idx));
                }
                self.free(old_entry);
            }
            UpsertResult::Inserted(new_handle) => {
                *handle = new_handle.weak();
                self.handles.select(new_kind).push(new_handle);
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Clone, PartialEq)]
struct SlabSize {
    width: i32,
    height: i32,
}

impl SlabSize {
    fn new(size: DeviceIntSize) -> SlabSize {
        let x_size = quantize_dimension(size.width);
        let y_size = quantize_dimension(size.height);

        assert!(x_size > 0 && x_size <= TEXTURE_REGION_DIMENSIONS);
        assert!(y_size > 0 && y_size <= TEXTURE_REGION_DIMENSIONS);

        let (width, height) = match (x_size, y_size) {
            // Special cased rectangular slab pages.
            (512, 256) => (512, 256),
            (512, 128) => (512, 128),
            (512,  64) => (512,  64),
            (256, 512) => (256, 512),
            (128, 512) => (128, 512),
            ( 64, 512) => ( 64, 512),

            // If none of those fit, use a square slab size.
            (x_size, y_size) => {
                let square_size = cmp::max(x_size, y_size);
                (square_size, square_size)
            }
        };

        SlabSize {
            width,
            height,
        }
    }

    fn invalid() -> SlabSize {
        SlabSize {
            width: 0,
            height: 0,
        }
    }
}

// The x/y location within a texture region of an allocation.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct TextureLocation(u8, u8);

impl TextureLocation {
    fn new(x: i32, y: i32) -> Self {
        debug_assert!(x >= 0 && y >= 0 && x < 0x100 && y < 0x100);
        TextureLocation(x as u8, y as u8)
    }
}

/// A region corresponds to a layer in a shared cache texture.
///
/// All allocations within a region are of the same size.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct TextureRegion {
    layer_index: usize,
    slab_size: SlabSize,
    free_slots: Vec<TextureLocation>,
    total_slot_count: usize,
}

impl TextureRegion {
    fn new(layer_index: usize) -> Self {
        TextureRegion {
            layer_index,
            slab_size: SlabSize::invalid(),
            free_slots: Vec::new(),
            total_slot_count: 0,
        }
    }

    // Initialize a region to be an allocator for a specific slab size.
    fn init(&mut self, slab_size: SlabSize, empty_regions: &mut usize) {
        debug_assert!(self.slab_size == SlabSize::invalid());
        debug_assert!(self.free_slots.is_empty());

        self.slab_size = slab_size;
        let slots_per_x_axis = TEXTURE_REGION_DIMENSIONS / self.slab_size.width;
        let slots_per_y_axis = TEXTURE_REGION_DIMENSIONS / self.slab_size.height;

        // Add each block to a freelist.
        for y in 0 .. slots_per_y_axis {
            for x in 0 .. slots_per_x_axis {
                self.free_slots.push(TextureLocation::new(x, y));
            }
        }

        self.total_slot_count = self.free_slots.len();
        *empty_regions -= 1;
    }

    // Deinit a region, allowing it to become a region with
    // a different allocator size.
    fn deinit(&mut self, empty_regions: &mut usize) {
        self.slab_size = SlabSize::invalid();
        self.free_slots.clear();
        self.total_slot_count = 0;
        *empty_regions += 1;
    }

    fn is_empty(&self) -> bool {
        self.slab_size == SlabSize::invalid()
    }

    // Attempt to allocate a fixed size block from this region.
    fn alloc(&mut self) -> Option<DeviceIntPoint> {
        debug_assert!(self.slab_size != SlabSize::invalid());

        self.free_slots.pop().map(|location| {
            DeviceIntPoint::new(
                self.slab_size.width * location.0 as i32,
                self.slab_size.height * location.1 as i32,
            )
        })
    }

    // Free a block in this region.
    fn free(&mut self, point: DeviceIntPoint, empty_regions: &mut usize) {
        let x = point.x / self.slab_size.width;
        let y = point.y / self.slab_size.height;
        self.free_slots.push(TextureLocation::new(x, y));

        // If this region is completely unused, deinit it
        // so that it can become a different slab size
        // as required.
        if self.free_slots.len() == self.total_slot_count {
            self.deinit(empty_regions);
        }
    }
}

/// A texture array contains a number of texture layers, where each layer
/// contains a region that can act as a slab allocator.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct TextureArray {
    filter: TextureFilter,
    format: ImageFormat,
    regions: Vec<TextureRegion>,
    empty_regions: usize,
    texture_id: Option<CacheTextureId>,
}

impl TextureArray {
    fn new(
        format: ImageFormat,
        filter: TextureFilter,
    ) -> Self {
        TextureArray {
            format,
            filter,
            regions: Vec::new(),
            empty_regions: 0,
            texture_id: None,
        }
    }

    /// Returns the number of GPU bytes consumed by this texture array.
    fn size_in_bytes(&self) -> usize {
        let bpp = self.format.bytes_per_pixel() as usize;
        self.regions.len() * TEXTURE_REGION_PIXELS * bpp
    }

    /// Returns the number of GPU bytes consumed by empty regions.
    fn empty_region_bytes(&self) -> usize {
        let bpp = self.format.bytes_per_pixel() as usize;
        self.empty_regions * TEXTURE_REGION_PIXELS * bpp
    }

    fn clear(&mut self, updates: &mut TextureUpdateList) {
        self.regions.clear();
        self.empty_regions = 0;
        if let Some(id) = self.texture_id.take() {
            updates.push_free(id);
        }
    }

    fn update_profile(&self, counter: &mut ResourceProfileCounter) {
        counter.set(self.regions.len(), self.size_in_bytes());
    }

    /// Adds a new empty region to the array.
    fn push_region(&mut self) {
        let index = self.regions.len();
        self.regions.push(TextureRegion::new(index));
        self.empty_regions += 1;
        assert!(self.empty_regions <= self.regions.len());
    }

    /// Allocate space in this texture array.
    fn alloc(
        &mut self,
        params: &CacheAllocParams,
        now: FrameStamp,
    ) -> Option<CacheEntry> {
        // Quantize the size of the allocation to select a region to
        // allocate from.
        let slab_size = SlabSize::new(params.descriptor.size);

        // TODO(gw): For simplicity, the initial implementation just
        //           has a single vec<> of regions. We could easily
        //           make this more efficient by storing a list of
        //           regions for each slab size specifically...

        // Keep track of the location of an empty region,
        // in case we need to select a new empty region
        // after the loop.
        let mut empty_region_index = None;
        let mut entry_details = None;

        // Run through the existing regions of this size, and see if
        // we can find a free block in any of them.
        for (i, region) in self.regions.iter_mut().enumerate() {
            if region.is_empty() {
                empty_region_index = Some(i);
            } else if region.slab_size == slab_size {
                if let Some(location) = region.alloc() {
                    entry_details = Some(EntryDetails::Cache {
                        layer_index: region.layer_index,
                        origin: location,
                    });
                    break;
                }
            }
        }

        // Find a region of the right size and try to allocate from it.
        if entry_details.is_none() {
            if let Some(empty_region_index) = empty_region_index {
                let region = &mut self.regions[empty_region_index];
                region.init(slab_size, &mut self.empty_regions);
                entry_details = region.alloc().map(|location| {
                    EntryDetails::Cache {
                        layer_index: region.layer_index,
                        origin: location,
                    }
                });
            }
        }

        entry_details.map(|details| {
            CacheEntry {
                size: params.descriptor.size,
                user_data: params.user_data,
                last_access: now,
                details,
                uv_rect_handle: GpuCacheHandle::new(),
                format: self.format,
                filter: self.filter,
                texture_id: self.texture_id.unwrap(),
                eviction_notice: None,
                uv_rect_kind: params.uv_rect_kind,
                eviction: Eviction::Auto,
            }
        })
    }
}

impl TextureCacheUpdate {
    // Constructs a TextureCacheUpdate operation to be passed to the
    // rendering thread in order to do an upload to the right
    // location in the texture cache.
    fn new_update(
        data: CachedImageData,
        descriptor: &ImageDescriptor,
        origin: DeviceIntPoint,
        size: DeviceIntSize,
        texture_id: CacheTextureId,
        layer_index: i32,
        dirty_rect: &ImageDirtyRect,
    ) -> TextureCacheUpdate {
        let source = match data {
            CachedImageData::Blob => {
                panic!("The vector image should have been rasterized.");
            }
            CachedImageData::External(ext_image) => match ext_image.image_type {
                ExternalImageType::TextureHandle(_) => {
                    panic!("External texture handle should not go through texture_cache.");
                }
                ExternalImageType::Buffer => TextureUpdateSource::External {
                    id: ext_image.id,
                    channel_index: ext_image.channel_index,
                },
            },
            CachedImageData::Raw(bytes) => {
                let finish = descriptor.offset +
                    descriptor.size.width * descriptor.format.bytes_per_pixel() +
                    (descriptor.size.height - 1) * descriptor.compute_stride();
                assert!(bytes.len() >= finish as usize);

                TextureUpdateSource::Bytes { data: bytes }
            }
        };

        let update_op = match *dirty_rect {
            DirtyRect::Partial(dirty) => {
                // the dirty rectangle doesn't have to be within the area but has to intersect it, at least
                let stride = descriptor.compute_stride();
                let offset = descriptor.offset + dirty.origin.y * stride + dirty.origin.x * descriptor.format.bytes_per_pixel();

                TextureCacheUpdate {
                    id: texture_id,
                    rect: DeviceIntRect::new(
                        DeviceIntPoint::new(origin.x + dirty.origin.x, origin.y + dirty.origin.y),
                        DeviceIntSize::new(
                            dirty.size.width.min(size.width - dirty.origin.x),
                            dirty.size.height.min(size.height - dirty.origin.y),
                        ),
                    ),
                    source,
                    stride: Some(stride),
                    offset,
                    layer_index,
                }
            }
            DirtyRect::All => {
                TextureCacheUpdate {
                    id: texture_id,
                    rect: DeviceIntRect::new(origin, size),
                    source,
                    stride: descriptor.stride,
                    offset: descriptor.offset,
                    layer_index,
                }
            }
        };

        update_op
    }
}

fn quantize_dimension(size: i32) -> i32 {
    match size {
        0 => unreachable!(),
        1...16 => 16,
        17...32 => 32,
        33...64 => 64,
        65...128 => 128,
        129...256 => 256,
        257...512 => 512,
        _ => panic!("Invalid dimensions for cache!"),
    }
}

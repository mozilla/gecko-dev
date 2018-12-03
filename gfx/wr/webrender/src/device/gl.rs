/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::super::shader_source;
use api::{ColorF, ImageFormat, MemoryReport};
use api::{DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::TextureTarget;
use api::VoidPtrToSizeFn;
#[cfg(any(feature = "debug_renderer", feature="capture"))]
use api::ImageDescriptor;
use euclid::Transform3D;
use gleam::gl;
use internal_types::{FastHashMap, LayerIndex, RenderTargetInfo};
use log::Level;
use sha2::{Digest, Sha256};
use smallvec::SmallVec;
use std::borrow::Cow;
use std::cell::{Cell, RefCell};
use std::cmp;
use std::collections::hash_map::Entry;
use std::fs::File;
use std::io::Read;
use std::marker::PhantomData;
use std::mem;
use std::os::raw::c_void;
use std::ops::Add;
use std::path::PathBuf;
use std::ptr;
use std::rc::Rc;
use std::slice;
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT, Ordering};
use std::thread;

/// Sequence number for frames, as tracked by the device layer.
#[derive(Debug, Copy, Clone, PartialEq, Ord, Eq, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GpuFrameId(usize);

/// Tracks the total number of GPU bytes allocated across all WebRender instances.
///
/// Assuming all WebRender instances run on the same thread, this doesn't need
/// to be atomic per se, but we make it atomic to satisfy the thread safety
/// invariants in the type system. We could also put the value in TLS, but that
/// would be more expensive to access.
static GPU_BYTES_ALLOCATED: AtomicUsize = ATOMIC_USIZE_INIT;

/// Returns the number of GPU bytes currently allocated.
pub fn total_gpu_bytes_allocated() -> usize {
    GPU_BYTES_ALLOCATED.load(Ordering::Relaxed)
}

/// Records an allocation in VRAM.
fn record_gpu_alloc(num_bytes: usize) {
    GPU_BYTES_ALLOCATED.fetch_add(num_bytes, Ordering::Relaxed);
}

/// Records an deallocation in VRAM.
fn record_gpu_free(num_bytes: usize) {
    let old = GPU_BYTES_ALLOCATED.fetch_sub(num_bytes, Ordering::Relaxed);
    assert!(old >= num_bytes, "Freeing {} bytes but only {} allocated", num_bytes, old);
}

impl GpuFrameId {
    pub fn new(value: usize) -> Self {
        GpuFrameId(value)
    }
}

impl Add<usize> for GpuFrameId {
    type Output = GpuFrameId;

    fn add(self, other: usize) -> GpuFrameId {
        GpuFrameId(self.0 + other)
    }
}

const SHADER_VERSION_GL: &str = "#version 150\n";
const SHADER_VERSION_GLES: &str = "#version 300 es\n";

const SHADER_KIND_VERTEX: &str = "#define WR_VERTEX_SHADER\n";
const SHADER_KIND_FRAGMENT: &str = "#define WR_FRAGMENT_SHADER\n";
const SHADER_IMPORT: &str = "#include ";

pub struct TextureSlot(pub usize);

// In some places we need to temporarily bind a texture to any slot.
const DEFAULT_TEXTURE: TextureSlot = TextureSlot(0);

#[repr(u32)]
pub enum DepthFunction {
    #[cfg(feature = "debug_renderer")]
    Less = gl::LESS,
    LessEqual = gl::LEQUAL,
}

#[repr(u32)]
#[derive(Copy, Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TextureFilter {
    Nearest,
    Linear,
    Trilinear,
}

#[derive(Debug)]
pub enum VertexAttributeKind {
    F32,
    #[cfg(feature = "debug_renderer")]
    U8Norm,
    U16Norm,
    I32,
    U16,
}

#[derive(Debug)]
pub struct VertexAttribute {
    pub name: &'static str,
    pub count: u32,
    pub kind: VertexAttributeKind,
}

#[derive(Debug)]
pub struct VertexDescriptor {
    pub vertex_attributes: &'static [VertexAttribute],
    pub instance_attributes: &'static [VertexAttribute],
}

enum FBOTarget {
    Read,
    Draw,
}

/// Method of uploading texel data from CPU to GPU.
#[derive(Debug, Clone)]
pub enum UploadMethod {
    /// Just call `glTexSubImage` directly with the CPU data pointer
    Immediate,
    /// Accumulate the changes in PBO first before transferring to a texture.
    PixelBuffer(VertexUsageHint),
}

/// Plain old data that can be used to initialize a texture.
pub unsafe trait Texel: Copy {}
unsafe impl Texel for u8 {}
unsafe impl Texel for f32 {}

/// Returns the size in bytes of a depth target with the given dimensions.
fn depth_target_size_in_bytes(dimensions: &DeviceIntSize) -> usize {
    // DEPTH24 textures generally reserve 3 bytes for depth and 1 byte
    // for stencil, so we measure them as 32 bits.
    let pixels = dimensions.width * dimensions.height;
    (pixels as usize) * 4
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ReadPixelsFormat {
    Standard(ImageFormat),
    Rgba8,
}

pub fn get_gl_target(target: TextureTarget) -> gl::GLuint {
    match target {
        TextureTarget::Default => gl::TEXTURE_2D,
        TextureTarget::Array => gl::TEXTURE_2D_ARRAY,
        TextureTarget::Rect => gl::TEXTURE_RECTANGLE,
        TextureTarget::External => gl::TEXTURE_EXTERNAL_OES,
    }
}

fn supports_extension(extensions: &[String], extension: &str) -> bool {
    extensions.iter().any(|s| s == extension)
}

fn get_shader_version(gl: &gl::Gl) -> &'static str {
    match gl.get_type() {
        gl::GlType::Gl => SHADER_VERSION_GL,
        gl::GlType::Gles => SHADER_VERSION_GLES,
    }
}

// Get a shader string by name, from the built in resources or
// an override path, if supplied.
fn get_shader_source(shader_name: &str, base_path: &Option<PathBuf>) -> Option<Cow<'static, str>> {
    if let Some(ref base) = *base_path {
        let shader_path = base.join(&format!("{}.glsl", shader_name));
        if shader_path.exists() {
            let mut source = String::new();
            File::open(&shader_path)
                .unwrap()
                .read_to_string(&mut source)
                .unwrap();
            return Some(Cow::Owned(source));
        }
    }

    shader_source::SHADERS
        .get(shader_name)
        .map(|s| Cow::Borrowed(*s))
}

// Parse a shader string for imports. Imports are recursively processed, and
// prepended to the output stream.
fn parse_shader_source<F: FnMut(&str)>(source: Cow<'static, str>, base_path: &Option<PathBuf>, output: &mut F) {
    for line in source.lines() {
        if line.starts_with(SHADER_IMPORT) {
            let imports = line[SHADER_IMPORT.len() ..].split(',');

            // For each import, get the source, and recurse.
            for import in imports {
                if let Some(include) = get_shader_source(import, base_path) {
                    parse_shader_source(include, base_path, output);
                }
            }
        } else {
            output(line);
            output("\n");
        }
    }
}

/// Creates heap-allocated strings for both vertex and fragment shaders. Public
/// to be accessible to tests.
pub fn build_shader_strings(
     gl_version_string: &str,
     features: &str,
     base_filename: &str,
     override_path: &Option<PathBuf>,
) -> (String, String) {
    let mut vs_source = String::new();
    do_build_shader_string(
        gl_version_string,
        features,
        SHADER_KIND_VERTEX,
        base_filename,
        override_path,
        |s| vs_source.push_str(s),
    );

    let mut fs_source = String::new();
    do_build_shader_string(
        gl_version_string,
        features,
        SHADER_KIND_FRAGMENT,
        base_filename,
        override_path,
        |s| fs_source.push_str(s),
    );

    (vs_source, fs_source)
}

/// Walks the given shader string and applies the output to the provided
/// callback. Assuming an override path is not used, does no heap allocation
/// and no I/O.
fn do_build_shader_string<F: FnMut(&str)>(
    gl_version_string: &str,
    features: &str,
    kind: &str,
    base_filename: &str,
    override_path: &Option<PathBuf>,
    mut output: F,
) {
    // GLSL requires that the version number comes first.
    output(gl_version_string);

    // Insert the shader name to make debugging easier.
    let name_string = format!("// {}\n", base_filename);
    output(&name_string);

    // Define a constant depending on whether we are compiling VS or FS.
    output(kind);

    // Add any defines that were passed by the caller.
    output(features);

    // Parse the main .glsl file, including any imports
    // and append them to the list of sources.
    if let Some(shared_source) = get_shader_source(base_filename, override_path) {
        parse_shader_source(shared_source, override_path, &mut output);
    }
}

pub trait FileWatcherHandler: Send {
    fn file_changed(&self, path: PathBuf);
}

impl VertexAttributeKind {
    fn size_in_bytes(&self) -> u32 {
        match *self {
            VertexAttributeKind::F32 => 4,
            #[cfg(feature = "debug_renderer")]
            VertexAttributeKind::U8Norm => 1,
            VertexAttributeKind::U16Norm => 2,
            VertexAttributeKind::I32 => 4,
            VertexAttributeKind::U16 => 2,
        }
    }
}

impl VertexAttribute {
    fn size_in_bytes(&self) -> u32 {
        self.count * self.kind.size_in_bytes()
    }

    fn bind_to_vao(
        &self,
        attr_index: gl::GLuint,
        divisor: gl::GLuint,
        stride: gl::GLint,
        offset: gl::GLuint,
        gl: &gl::Gl,
    ) {
        gl.enable_vertex_attrib_array(attr_index);
        gl.vertex_attrib_divisor(attr_index, divisor);

        match self.kind {
            VertexAttributeKind::F32 => {
                gl.vertex_attrib_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::FLOAT,
                    false,
                    stride,
                    offset,
                );
            }
            #[cfg(feature = "debug_renderer")]
            VertexAttributeKind::U8Norm => {
                gl.vertex_attrib_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::UNSIGNED_BYTE,
                    true,
                    stride,
                    offset,
                );
            }
            VertexAttributeKind::U16Norm => {
                gl.vertex_attrib_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::UNSIGNED_SHORT,
                    true,
                    stride,
                    offset,
                );
            }
            VertexAttributeKind::I32 => {
                gl.vertex_attrib_i_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::INT,
                    stride,
                    offset,
                );
            }
            VertexAttributeKind::U16 => {
                gl.vertex_attrib_i_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::UNSIGNED_SHORT,
                    stride,
                    offset,
                );
            }
        }
    }
}

impl VertexDescriptor {
    fn instance_stride(&self) -> u32 {
        self.instance_attributes
            .iter()
            .map(|attr| attr.size_in_bytes())
            .sum()
    }

    fn bind_attributes(
        attributes: &[VertexAttribute],
        start_index: usize,
        divisor: u32,
        gl: &gl::Gl,
        vbo: VBOId,
    ) {
        vbo.bind(gl);

        let stride: u32 = attributes
            .iter()
            .map(|attr| attr.size_in_bytes())
            .sum();

        let mut offset = 0;
        for (i, attr) in attributes.iter().enumerate() {
            let attr_index = (start_index + i) as gl::GLuint;
            attr.bind_to_vao(attr_index, divisor, stride as _, offset, gl);
            offset += attr.size_in_bytes();
        }
    }

    fn bind(&self, gl: &gl::Gl, main: VBOId, instance: VBOId) {
        Self::bind_attributes(self.vertex_attributes, 0, 0, gl, main);

        if !self.instance_attributes.is_empty() {
            Self::bind_attributes(
                self.instance_attributes,
                self.vertex_attributes.len(),
                1, gl, instance,
            );
        }
    }
}

impl VBOId {
    fn bind(&self, gl: &gl::Gl) {
        gl.bind_buffer(gl::ARRAY_BUFFER, self.0);
    }
}

impl IBOId {
    fn bind(&self, gl: &gl::Gl) {
        gl.bind_buffer(gl::ELEMENT_ARRAY_BUFFER, self.0);
    }
}

impl FBOId {
    fn bind(&self, gl: &gl::Gl, target: FBOTarget) {
        let target = match target {
            FBOTarget::Read => gl::READ_FRAMEBUFFER,
            FBOTarget::Draw => gl::DRAW_FRAMEBUFFER,
        };
        gl.bind_framebuffer(target, self.0);
    }
}

pub struct Stream<'a> {
    attributes: &'a [VertexAttribute],
    vbo: VBOId,
}

pub struct VBO<V> {
    id: gl::GLuint,
    target: gl::GLenum,
    allocated_count: usize,
    marker: PhantomData<V>,
}

impl<V> VBO<V> {
    pub fn allocated_count(&self) -> usize {
        self.allocated_count
    }

    pub fn stream_with<'a>(&self, attributes: &'a [VertexAttribute]) -> Stream<'a> {
        debug_assert_eq!(
            mem::size_of::<V>(),
            attributes.iter().map(|a| a.size_in_bytes() as usize).sum::<usize>()
        );
        Stream {
            attributes,
            vbo: VBOId(self.id),
        }
    }
}

impl<T> Drop for VBO<T> {
    fn drop(&mut self) {
        debug_assert!(thread::panicking() || self.id == 0);
    }
}

#[cfg_attr(feature = "replay", derive(Clone))]
pub struct ExternalTexture {
    id: gl::GLuint,
    target: gl::GLuint,
}

impl ExternalTexture {
    pub fn new(id: u32, target: TextureTarget) -> Self {
        ExternalTexture {
            id,
            target: get_gl_target(target),
        }
    }

    #[cfg(feature = "replay")]
    pub fn internal_id(&self) -> gl::GLuint {
        self.id
    }
}

bitflags! {
    #[derive(Default)]
    pub struct TextureFlags: u32 {
        /// This texture corresponds to one of the shared texture caches.
        const IS_SHARED_TEXTURE_CACHE = 1 << 0;
    }
}

/// WebRender interface to an OpenGL texture.
///
/// Because freeing a texture requires various device handles that are not
/// reachable from this struct, manual destruction via `Device` is required.
/// Our `Drop` implementation asserts that this has happened.
pub struct Texture {
    id: gl::GLuint,
    target: gl::GLuint,
    layer_count: i32,
    format: ImageFormat,
    size: DeviceIntSize,
    filter: TextureFilter,
    flags: TextureFlags,
    /// Framebuffer Objects, one for each layer of the texture, allowing this
    /// texture to be rendered to. Empty if this texture is not used as a render
    /// target.
    fbos: Vec<FBOId>,
    /// Same as the above, but with a depth buffer attached.
    ///
    /// FBOs are cheap to create but expensive to reconfigure (since doing so
    /// invalidates framebuffer completeness caching). Moreover, rendering with
    /// a depth buffer attached but the depth write+test disabled relies on the
    /// driver to optimize it out of the rendering pass, which most drivers
    /// probably do but, according to jgilbert, is best not to rely on.
    ///
    /// So we lazily generate a second list of FBOs with depth. This list is
    /// empty if this texture is not used as a render target _or_ if it is, but
    /// the depth buffer has never been requested.
    ///
    /// Note that we always fill fbos, and then lazily create fbos_with_depth
    /// when needed. We could make both lazy (i.e. render targets would have one
    /// or the other, but not both, unless they were actually used in both
    /// configurations). But that would complicate a lot of logic in this module,
    /// and FBOs are cheap enough to create.
    fbos_with_depth: Vec<FBOId>,
    last_frame_used: GpuFrameId,
}

impl Texture {
    pub fn get_dimensions(&self) -> DeviceIntSize {
        self.size
    }

    pub fn get_layer_count(&self) -> i32 {
        self.layer_count
    }

    pub fn get_format(&self) -> ImageFormat {
        self.format
    }

    #[cfg(any(feature = "debug_renderer", feature = "capture"))]
    pub fn get_filter(&self) -> TextureFilter {
        self.filter
    }

    pub fn supports_depth(&self) -> bool {
        !self.fbos_with_depth.is_empty()
    }

    pub fn used_in_frame(&self, frame_id: GpuFrameId) -> bool {
        self.last_frame_used == frame_id
    }

    /// Returns true if this texture was used within `threshold` frames of
    /// the current frame.
    pub fn used_recently(&self, current_frame_id: GpuFrameId, threshold: usize) -> bool {
        self.last_frame_used + threshold >= current_frame_id
    }

    /// Returns the flags for this texture.
    pub fn flags(&self) -> &TextureFlags {
        &self.flags
    }

    /// Returns a mutable borrow of the flags for this texture.
    pub fn flags_mut(&mut self) -> &mut TextureFlags {
        &mut self.flags
    }

    /// Returns the number of bytes (generally in GPU memory) that each layer of
    /// this texture consumes.
    pub fn layer_size_in_bytes(&self) -> usize {
        assert!(self.layer_count > 0 || self.size.width + self.size.height == 0);
        let bpp = self.format.bytes_per_pixel() as usize;
        let w = self.size.width as usize;
        let h = self.size.height as usize;
        bpp * w * h
    }

    /// Returns the number of bytes (generally in GPU memory) that this texture
    /// consumes.
    pub fn size_in_bytes(&self) -> usize {
        self.layer_size_in_bytes() * (self.layer_count as usize)
    }

    #[cfg(feature = "replay")]
    pub fn into_external(mut self) -> ExternalTexture {
        let ext = ExternalTexture {
            id: self.id,
            target: self.target,
        };
        self.id = 0; // don't complain, moved out
        ext
    }
}

impl Drop for Texture {
    fn drop(&mut self) {
        debug_assert!(thread::panicking() || self.id == 0);
    }
}

pub struct Program {
    id: gl::GLuint,
    u_transform: gl::GLint,
    u_mode: gl::GLint,
    source_info: ProgramSourceInfo,
    is_initialized: bool,
}

impl Program {
    pub fn is_initialized(&self) -> bool {
        self.is_initialized
    }
}

impl Drop for Program {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

pub struct CustomVAO {
    id: gl::GLuint,
}

impl Drop for CustomVAO {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

pub struct VAO {
    id: gl::GLuint,
    ibo_id: IBOId,
    main_vbo_id: VBOId,
    instance_vbo_id: VBOId,
    instance_stride: usize,
    owns_vertices_and_indices: bool,
}

impl Drop for VAO {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

pub struct PBO {
    id: gl::GLuint,
}

impl Drop for PBO {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct FBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct RBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct VBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
struct IBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Clone, Default)]
#[cfg_attr(feature = "serialize_program", derive(Deserialize, Serialize))]
pub struct ProgramSourceDigest([u8; 32]);

impl ::std::fmt::Display for ProgramSourceDigest {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        for byte in self.0.iter() {
            f.write_fmt(format_args!("{:02x}", byte))?;
        }
        Ok(())
    }
}

#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub struct ProgramSourceInfo {
    base_filename: &'static str,
    features: String,
    digest: ProgramSourceDigest,
}

impl ProgramSourceInfo {
    fn new(
        device: &Device,
        base_filename: &'static str,
        features: String,
    ) -> Self {
        // Compute the digest. Assuming the device has a `ProgramCache`, this
        // will always be needed, whereas the source is rarely needed. As such,
        // we compute the hash by walking the static strings in the same order
        // as we would when concatenating the source, to avoid heap-allocating
        // in the common case.

        // Construct the hasher.
        let mut hasher = Sha256::new();

        // Hash the renderer name.
        hasher.input(device.renderer_name.as_bytes());

        // Hash the vertex shader.
        device.build_shader_string(
            &features,
            SHADER_KIND_VERTEX,
            &base_filename,
            |s| hasher.input(s.as_bytes()),
        );

        // Hash the fragment shader.
        device.build_shader_string(
            &features,
            SHADER_KIND_FRAGMENT,
            base_filename,
            |s| hasher.input(s.as_bytes()),
        );

        // Finish.
        let mut digest = ProgramSourceDigest::default();
        digest.0.copy_from_slice(hasher.result().as_slice());

        ProgramSourceInfo {
            base_filename,
            features,
            digest,
        }
    }

    fn compute_source(&self, device: &Device, kind: &str) -> String {
        let mut src = String::new();
        device.build_shader_string(
            &self.features,
            kind,
            self.base_filename,
            |s| src.push_str(s),
        );
        src
    }
}

#[cfg_attr(feature = "serialize_program", derive(Deserialize, Serialize))]
pub struct ProgramBinary {
    bytes: Vec<u8>,
    format: gl::GLenum,
    source_digest: ProgramSourceDigest,
}

impl ProgramBinary {
    fn new(bytes: Vec<u8>,
           format: gl::GLenum,
           source_digest: ProgramSourceDigest) -> Self {
        ProgramBinary {
            bytes,
            format,
            source_digest,
        }
    }

    /// Returns a reference to the source digest hash.
    pub fn source_digest(&self) -> &ProgramSourceDigest {
        &self.source_digest
    }
}

/// The interfaces that an application can implement to handle ProgramCache update
pub trait ProgramCacheObserver {
    fn update_disk_cache(&self, entries: Vec<Arc<ProgramBinary>>);
    fn notify_program_binary_failed(&self, program_binary: &Arc<ProgramBinary>);
}

struct ProgramCacheEntry {
    /// The binary.
    binary: Arc<ProgramBinary>,
    /// True if the binary has been linked, i.e. used for rendering.
    linked: bool,
}

pub struct ProgramCache {
    entries: RefCell<FastHashMap<ProgramSourceDigest, ProgramCacheEntry>>,

    /// True if we've already updated the disk cache with the shaders used during startup.
    updated_disk_cache: Cell<bool>,

    /// Optional trait object that allows the client
    /// application to handle ProgramCache updating
    program_cache_handler: Option<Box<ProgramCacheObserver>>,
}

impl ProgramCache {
    pub fn new(program_cache_observer: Option<Box<ProgramCacheObserver>>) -> Rc<Self> {
        Rc::new(
            ProgramCache {
                entries: RefCell::new(FastHashMap::default()),
                updated_disk_cache: Cell::new(false),
                program_cache_handler: program_cache_observer,
            }
        )
    }

    /// Notify that we've rendered the first few frames, and that the shaders
    /// we've loaded correspond to the shaders needed during startup, and thus
    /// should be the ones cached to disk.
    fn startup_complete(&self) {
        if self.updated_disk_cache.get() {
            return;
        }

        if let Some(ref handler) = self.program_cache_handler {
            let active_shaders = self.entries.borrow().values()
                .filter(|e| e.linked).map(|e| e.binary.clone())
                .collect::<Vec<_>>();
            handler.update_disk_cache(active_shaders);
            self.updated_disk_cache.set(true);
        }
    }

    /// Load ProgramBinary to ProgramCache.
    /// The function is typically used to load ProgramBinary from disk.
    #[cfg(feature = "serialize_program")]
    pub fn load_program_binary(&self, program_binary: Arc<ProgramBinary>) {
        let digest = program_binary.source_digest.clone();
        let entry = ProgramCacheEntry {
            binary: program_binary,
            linked: false,
        };
        self.entries.borrow_mut().insert(digest, entry);
    }

    /// Returns the number of bytes allocated for shaders in the cache.
    pub fn report_memory(&self, op: VoidPtrToSizeFn) -> usize {
        self.entries.borrow().values()
            .map(|e| unsafe { op(e.binary.bytes.as_ptr() as *const c_void ) })
            .sum()
    }
}

#[derive(Debug, Copy, Clone)]
pub enum VertexUsageHint {
    Static,
    Dynamic,
    Stream,
}

impl VertexUsageHint {
    fn to_gl(&self) -> gl::GLuint {
        match *self {
            VertexUsageHint::Static => gl::STATIC_DRAW,
            VertexUsageHint::Dynamic => gl::DYNAMIC_DRAW,
            VertexUsageHint::Stream => gl::STREAM_DRAW,
        }
    }
}

#[derive(Copy, Clone, Debug)]
pub struct UniformLocation(gl::GLint);

impl UniformLocation {
    pub const INVALID: Self = UniformLocation(-1);
}

#[cfg(feature = "debug_renderer")]
pub struct Capabilities {
    pub supports_multisampling: bool,
}

#[derive(Clone, Debug)]
pub enum ShaderError {
    Compilation(String, String), // name, error message
    Link(String, String),        // name, error message
}

/// A refcounted depth target, which may be shared by multiple textures across
/// the device.
struct SharedDepthTarget {
    /// The Render Buffer Object representing the depth target.
    rbo_id: RBOId,
    /// Reference count. When this drops to zero, the RBO is deleted.
    refcount: usize,
}

#[cfg(debug_assertions)]
impl Drop for SharedDepthTarget {
    fn drop(&mut self) {
        debug_assert!(thread::panicking() || self.refcount == 0);
    }
}

/// Describes for which texture formats to use the glTexStorage*
/// family of functions.
#[derive(PartialEq, Debug)]
enum TexStorageUsage {
    Never,
    NonBGRA8,
    Always,
}

pub struct Device {
    gl: Rc<gl::Gl>,
    // device state
    bound_textures: [gl::GLuint; 16],
    bound_program: gl::GLuint,
    bound_vao: gl::GLuint,
    bound_read_fbo: FBOId,
    bound_draw_fbo: FBOId,
    program_mode_id: UniformLocation,
    default_read_fbo: FBOId,
    default_draw_fbo: FBOId,

    /// Track depth state for assertions. Note that the default FBO has depth,
    /// so this defaults to true.
    depth_available: bool,

    device_pixel_ratio: f32,
    upload_method: UploadMethod,

    // HW or API capabilities
    #[cfg(feature = "debug_renderer")]
    capabilities: Capabilities,

    bgra_format_internal: gl::GLuint,
    bgra_format_external: gl::GLuint,

    /// Map from texture dimensions to shared depth buffers for render targets.
    ///
    /// Render targets often have the same width/height, so we can save memory
    /// by sharing these across targets.
    depth_targets: FastHashMap<DeviceIntSize, SharedDepthTarget>,

    // debug
    inside_frame: bool,

    // resources
    resource_override_path: Option<PathBuf>,

    max_texture_size: i32,
    max_texture_layers: u32,
    renderer_name: String,
    cached_programs: Option<Rc<ProgramCache>>,

    // Frame counter. This is used to map between CPU
    // frames and GPU frames.
    frame_id: GpuFrameId,

    /// When to use glTexStorage*. We prefer this over glTexImage* because it
    /// guarantees that mipmaps won't be generated (which they otherwise are on
    /// some drivers, particularly ANGLE). However, it is not always supported
    /// at all, or for BGRA8 format. If it's not supported for the required
    /// format, we fall back to glTexImage*.
    texture_storage_usage: TexStorageUsage,

    /// Whether the function glCopyImageSubData is available.
    supports_copy_image_sub_data: bool,

    // GL extensions
    extensions: Vec<String>,
}

/// Contains the parameters necessary to bind a draw target.
#[derive(Clone, Copy)]
pub enum DrawTarget<'a> {
    /// Use the device's default draw target, with the provided dimensions,
    /// which are used to set the viewport.
    Default(DeviceIntSize),
    /// Use the provided texture.
    Texture {
        /// The target texture.
        texture: &'a Texture,
        /// The slice within the texture array to draw to.
        layer: LayerIndex,
        /// Whether to draw with the texture's associated depth target.
        with_depth: bool,
    },
}

impl<'a> DrawTarget<'a> {
    /// Returns true if this draw target corresponds to the default framebuffer.
    pub fn is_default(&self) -> bool {
        match *self {
            DrawTarget::Default(..) => true,
            _ => false,
        }
    }

    /// Returns the dimensions of this draw-target.
    pub fn dimensions(&self) -> DeviceIntSize {
        match *self {
            DrawTarget::Default(d) => d,
            DrawTarget::Texture { texture, .. } => texture.get_dimensions(),
        }
    }
}

/// Contains the parameters necessary to bind a texture-backed read target.
#[derive(Clone, Copy)]
pub enum ReadTarget<'a> {
    /// Use the device's default draw target.
    Default,
    /// Use the provided texture,
    Texture {
        /// The source texture.
        texture: &'a Texture,
        /// The slice within the texture array to read from.
        layer: LayerIndex,
    }
}

impl<'a> From<DrawTarget<'a>> for ReadTarget<'a> {
    fn from(t: DrawTarget<'a>) -> Self {
        match t {
            DrawTarget::Default(..) => ReadTarget::Default,
            DrawTarget::Texture { texture, layer, .. } =>
                ReadTarget::Texture { texture, layer },
        }
    }
}

impl Device {
    pub fn new(
        gl: Rc<gl::Gl>,
        resource_override_path: Option<PathBuf>,
        upload_method: UploadMethod,
        cached_programs: Option<Rc<ProgramCache>>,
    ) -> Device {
        let mut max_texture_size = [0];
        let mut max_texture_layers = [0];
        unsafe {
            gl.get_integer_v(gl::MAX_TEXTURE_SIZE, &mut max_texture_size);
            gl.get_integer_v(gl::MAX_ARRAY_TEXTURE_LAYERS, &mut max_texture_layers);
        }

        let max_texture_size = max_texture_size[0];
        let max_texture_layers = max_texture_layers[0] as u32;
        let renderer_name = gl.get_string(gl::RENDERER);

        let mut extension_count = [0];
        unsafe {
            gl.get_integer_v(gl::NUM_EXTENSIONS, &mut extension_count);
        }
        let extension_count = extension_count[0] as gl::GLuint;
        let mut extensions = Vec::new();
        for i in 0 .. extension_count {
            extensions.push(gl.get_string_i(gl::EXTENSIONS, i));
        }

        // Our common-case image data in Firefox is BGRA, so we make an effort
        // to use BGRA as the internal texture storage format to avoid the need
        // to swizzle during upload. Currently we only do this on GLES (and thus
        // for Windows, via ANGLE).
        //
        // On Mac, Apple docs [1] claim that BGRA is a more efficient internal
        // format, so we may want to consider doing that at some point, since it
        // would give us both a more efficient internal format and avoid the
        // swizzling in the common case.
        //
        // We also need our internal format types to be sized, since glTexStorage*
        // will reject non-sized internal format types.
        //
        // Unfortunately, with GL_EXT_texture_format_BGRA8888, BGRA8 is not a
        // valid internal format (for glTexImage* or glTexStorage*) unless
        // GL_EXT_texture_storage is also available [2][3], which is usually
        // not the case on GLES 3 as the latter's functionality has been
        // included by default but the former has not been updated.
        // The extension is available on ANGLE, but on Android this usually
        // means we must fall back to using unsized BGRA and glTexImage*.
        //
        // [1] https://developer.apple.com/library/archive/documentation/
        //     GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/
        //     opengl_texturedata.html#//apple_ref/doc/uid/TP40001987-CH407-SW22
        // [2] https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_format_BGRA8888.txt
        // [3] https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_storage.txt
        let supports_bgra = supports_extension(&extensions, "GL_EXT_texture_format_BGRA8888");
        let supports_texture_storage = match gl.get_type() {
            gl::GlType::Gl => supports_extension(&extensions, "GL_ARB_texture_storage"),
            gl::GlType::Gles => true,
        };


        let (bgra_format_internal, bgra_format_external, texture_storage_usage) = if supports_bgra {
            assert_eq!(gl.get_type(), gl::GlType::Gles, "gleam only detects bgra on gles");
            // To support BGRA8 with glTexStorage* we specifically need
            // GL_EXT_texture_storage and GL_EXT_texture_format_BGRA8888.
            if supports_extension(&extensions, "GL_EXT_texture_format_BGRA8888") && supports_extension(&extensions, "GL_EXT_texture_storage") {
                // We can use glTexStorage with BGRA8 as the internal format.
                (gl::BGRA8_EXT, gl::BGRA_EXT, TexStorageUsage::Always)
            } else {
                // For BGRA8 textures we must must use the unsized BGRA internal
                // format and glTexImage. If texture storage is supported we can
                // use it for other formats.
                (
                    gl::BGRA_EXT,
                    gl::BGRA_EXT,
                    if supports_texture_storage {
                        TexStorageUsage::NonBGRA8
                    } else {
                        TexStorageUsage::Never
                    },
                )
            }
        } else {
            // BGRA is not supported as an internal format, therefore we will
            // use RGBA. On non-gles we can swizzle during upload. This is not
            // allowed on gles, so we must us RGBA for the external format too.
            // Red and blue will appear reversed, but it is the best we can do.
            // Since the internal format will actually be RGBA, if texture
            // storage is supported we can use it for such textures.
            (
                gl::RGBA8,
                if gl.get_type() == gl::GlType::Gles {
                    gl::RGBA
                } else {
                    gl::BGRA
                },
                if supports_texture_storage {
                    TexStorageUsage::Always
                } else {
                    TexStorageUsage::Never
                },
            )
        };

        let supports_copy_image_sub_data = supports_extension(&extensions, "GL_EXT_copy_image") ||
            supports_extension(&extensions, "GL_ARB_copy_image");

        // Explicitly set some global states to the values we expect.
        gl.disable(gl::FRAMEBUFFER_SRGB);
        gl.disable(gl::MULTISAMPLE);
        gl.disable(gl::POLYGON_SMOOTH);

        Device {
            gl,
            resource_override_path,
            // This is initialized to 1 by default, but it is reset
            // at the beginning of each frame in `Renderer::bind_frame_data`.
            device_pixel_ratio: 1.0,
            upload_method,
            inside_frame: false,

            #[cfg(feature = "debug_renderer")]
            capabilities: Capabilities {
                supports_multisampling: false, //TODO
            },

            bgra_format_internal,
            bgra_format_external,

            depth_targets: FastHashMap::default(),

            bound_textures: [0; 16],
            bound_program: 0,
            bound_vao: 0,
            bound_read_fbo: FBOId(0),
            bound_draw_fbo: FBOId(0),
            program_mode_id: UniformLocation::INVALID,
            default_read_fbo: FBOId(0),
            default_draw_fbo: FBOId(0),

            depth_available: true,

            max_texture_size,
            max_texture_layers,
            renderer_name,
            cached_programs,
            frame_id: GpuFrameId(0),
            extensions,
            texture_storage_usage,
            supports_copy_image_sub_data
        }
    }

    pub fn gl(&self) -> &gl::Gl {
        &*self.gl
    }

    pub fn rc_gl(&self) -> &Rc<gl::Gl> {
        &self.gl
    }

    pub fn set_device_pixel_ratio(&mut self, ratio: f32) {
        self.device_pixel_ratio = ratio;
    }

    pub fn update_program_cache(&mut self, cached_programs: Rc<ProgramCache>) {
        self.cached_programs = Some(cached_programs);
    }

    /// Ensures that the maximum texture size is less than or equal to the
    /// provided value. If the provided value is less than the value supported
    /// by the driver, the latter is used.
    pub fn clamp_max_texture_size(&mut self, size: i32) {
        self.max_texture_size = self.max_texture_size.min(size);
    }

    /// Returns the limit on texture dimensions (width or height).
    pub fn max_texture_size(&self) -> i32 {
        self.max_texture_size
    }

    /// Returns the limit on texture array layers.
    pub fn max_texture_layers(&self) -> usize {
        self.max_texture_layers as usize
    }

    #[cfg(feature = "debug_renderer")]
    pub fn get_capabilities(&self) -> &Capabilities {
        &self.capabilities
    }

    pub fn reset_state(&mut self) {
        self.bound_textures = [0; 16];
        self.bound_vao = 0;
        self.bound_read_fbo = FBOId(0);
        self.bound_draw_fbo = FBOId(0);
    }

    #[cfg(debug_assertions)]
    fn print_shader_errors(source: &str, log: &str) {
        // hacky way to extract the offending lines
        if !log.starts_with("0:") {
            return;
        }
        let end_pos = match log[2..].chars().position(|c| !c.is_digit(10)) {
            Some(pos) => 2 + pos,
            None => return,
        };
        let base_line_number = match log[2 .. end_pos].parse::<usize>() {
            Ok(number) if number >= 2 => number - 2,
            _ => return,
        };
        for (line, prefix) in source.lines().skip(base_line_number).zip(&["|",">","|"]) {
            error!("{}\t{}", prefix, line);
        }
    }

    pub fn compile_shader(
        gl: &gl::Gl,
        name: &str,
        shader_type: gl::GLenum,
        source: &String,
    ) -> Result<gl::GLuint, ShaderError> {
        debug!("compile {}", name);
        let id = gl.create_shader(shader_type);
        gl.shader_source(id, &[source.as_bytes()]);
        gl.compile_shader(id);
        let log = gl.get_shader_info_log(id);
        let mut status = [0];
        unsafe {
            gl.get_shader_iv(id, gl::COMPILE_STATUS, &mut status);
        }
        if status[0] == 0 {
            error!("Failed to compile shader: {}\n{}", name, log);
            #[cfg(debug_assertions)]
            Self::print_shader_errors(source, &log);
            Err(ShaderError::Compilation(name.to_string(), log))
        } else {
            if !log.is_empty() {
                warn!("Warnings detected on shader: {}\n{}", name, log);
            }
            Ok(id)
        }
    }

    // If an assertion is hit in this function, something outside of WebRender is likely
    // messing with the GL context's global state.
    pub fn check_gl_state(&self) {
        debug_assert!(self.gl.is_enabled(gl::FRAMEBUFFER_SRGB) == 0);
        debug_assert!(self.gl.is_enabled(gl::MULTISAMPLE) == 0);
        debug_assert!(self.gl.is_enabled(gl::POLYGON_SMOOTH) == 0);
    }

    pub fn begin_frame(&mut self) -> GpuFrameId {
        debug_assert!(!self.inside_frame);
        self.inside_frame = true;

        self.check_gl_state();

        // Retrieve the currently set FBO.
        let mut default_read_fbo = [0];
        unsafe {
            self.gl.get_integer_v(gl::READ_FRAMEBUFFER_BINDING, &mut default_read_fbo);
        }
        self.default_read_fbo = FBOId(default_read_fbo[0] as gl::GLuint);
        let mut default_draw_fbo = [0];
        unsafe {
            self.gl.get_integer_v(gl::DRAW_FRAMEBUFFER_BINDING, &mut default_draw_fbo);
        }
        self.default_draw_fbo = FBOId(default_draw_fbo[0] as gl::GLuint);

        // Texture state
        for i in 0 .. self.bound_textures.len() {
            self.bound_textures[i] = 0;
            self.gl.active_texture(gl::TEXTURE0 + i as gl::GLuint);
            self.gl.bind_texture(gl::TEXTURE_2D, 0);
        }

        // Shader state
        self.bound_program = 0;
        self.program_mode_id = UniformLocation::INVALID;
        self.gl.use_program(0);

        // Vertex state
        self.bound_vao = 0;
        self.gl.bind_vertex_array(0);

        // FBO state
        self.bound_read_fbo = self.default_read_fbo;
        self.bound_draw_fbo = self.default_draw_fbo;

        // Pixel op state
        self.gl.pixel_store_i(gl::UNPACK_ALIGNMENT, 1);
        self.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);

        // Default is sampler 0, always
        self.gl.active_texture(gl::TEXTURE0);

        self.frame_id
    }

    fn bind_texture_impl(&mut self, slot: TextureSlot, id: gl::GLuint, target: gl::GLenum) {
        debug_assert!(self.inside_frame);

        if self.bound_textures[slot.0] != id {
            self.bound_textures[slot.0] = id;
            self.gl.active_texture(gl::TEXTURE0 + slot.0 as gl::GLuint);
            self.gl.bind_texture(target, id);
            self.gl.active_texture(gl::TEXTURE0);
        }
    }

    pub fn bind_texture<S>(&mut self, sampler: S, texture: &Texture)
    where
        S: Into<TextureSlot>,
    {
        self.bind_texture_impl(sampler.into(), texture.id, texture.target);
    }

    pub fn bind_external_texture<S>(&mut self, sampler: S, external_texture: &ExternalTexture)
    where
        S: Into<TextureSlot>,
    {
        self.bind_texture_impl(sampler.into(), external_texture.id, external_texture.target);
    }

    pub fn bind_read_target_impl(&mut self, fbo_id: FBOId) {
        debug_assert!(self.inside_frame);

        if self.bound_read_fbo != fbo_id {
            self.bound_read_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Read);
        }
    }

    pub fn bind_read_target(&mut self, target: ReadTarget) {
        let fbo_id = match target {
            ReadTarget::Default => self.default_read_fbo,
            ReadTarget::Texture { texture, layer } => texture.fbos[layer],
        };

        self.bind_read_target_impl(fbo_id)
    }

    fn bind_draw_target_impl(&mut self, fbo_id: FBOId) {
        debug_assert!(self.inside_frame);

        if self.bound_draw_fbo != fbo_id {
            self.bound_draw_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Draw);
        }
    }

    pub fn reset_read_target(&mut self) {
        let fbo = self.default_read_fbo;
        self.bind_read_target_impl(fbo);
    }


    pub fn reset_draw_target(&mut self) {
        let fbo = self.default_draw_fbo;
        self.bind_draw_target_impl(fbo);
        self.depth_available = true;
    }

    pub fn bind_draw_target(
        &mut self,
        target: DrawTarget,
    ) {
        let (fbo_id, dimensions, depth_available) = match target {
            DrawTarget::Default(d) => (self.default_draw_fbo, d, true),
            DrawTarget::Texture { texture, layer, with_depth } => {
                let dim = texture.get_dimensions();
                if with_depth {
                    (texture.fbos_with_depth[layer], dim, true)
                } else {
                    (texture.fbos[layer], dim, false)
                }
            }
        };

        self.depth_available = depth_available;
        self.bind_draw_target_impl(fbo_id);
        self.gl.viewport(
            0,
            0,
            dimensions.width as _,
            dimensions.height as _,
        );
    }

    pub fn create_fbo_for_external_texture(&mut self, texture_id: u32) -> FBOId {
        let fbo = FBOId(self.gl.gen_framebuffers(1)[0]);
        fbo.bind(self.gl(), FBOTarget::Draw);
        self.gl.framebuffer_texture_2d(
            gl::DRAW_FRAMEBUFFER,
            gl::COLOR_ATTACHMENT0,
            gl::TEXTURE_2D,
            texture_id,
            0,
        );
        self.bound_draw_fbo.bind(self.gl(), FBOTarget::Draw);
        fbo
    }

    pub fn delete_fbo(&mut self, fbo: FBOId) {
        self.gl.delete_framebuffers(&[fbo.0]);
    }

    pub fn bind_external_draw_target(&mut self, fbo_id: FBOId) {
        debug_assert!(self.inside_frame);

        if self.bound_draw_fbo != fbo_id {
            self.bound_draw_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Draw);
        }
    }

    /// Link a program, attaching the supplied vertex format.
    ///
    /// If `create_program()` finds a binary shader on disk, it will kick
    /// off linking immediately, which some drivers (notably ANGLE) run
    /// in parallel on background threads. As such, this function should
    /// ideally be run sometime later, to give the driver time to do that
    /// before blocking due to an API call accessing the shader.
    ///
    /// This generally means that the first run of the application will have
    /// to do a bunch of blocking work to compile the shader from source, but
    /// subsequent runs should load quickly.
    pub fn link_program(
        &mut self,
        program: &mut Program,
        descriptor: &VertexDescriptor,
    ) -> Result<(), ShaderError> {
        assert!(!program.is_initialized());
        let mut build_program = true;
        let info = &program.source_info;

        // See if we hit the binary shader cache
        if let Some(ref cached_programs) = self.cached_programs {
            if let Some(entry) = cached_programs.entries.borrow_mut().get_mut(&info.digest) {
                let mut link_status = [0];
                unsafe {
                    self.gl.get_program_iv(program.id, gl::LINK_STATUS, &mut link_status);
                }
                if link_status[0] == 0 {
                    let error_log = self.gl.get_program_info_log(program.id);
                    error!(
                      "Failed to load a program object with a program binary: {} renderer {}\n{}",
                      &info.base_filename,
                      self.renderer_name,
                      error_log
                    );
                    if let Some(ref program_cache_handler) = cached_programs.program_cache_handler {
                        program_cache_handler.notify_program_binary_failed(&entry.binary);
                    }
                } else {
                    entry.linked = true;
                    build_program = false;
                }
            }
        }

        // If not, we need to do a normal compile + link pass.
        if build_program {
            // Compile the vertex shader
            let vs_source = info.compute_source(self, SHADER_KIND_VERTEX);
            let vs_id = match Device::compile_shader(&*self.gl, &info.base_filename, gl::VERTEX_SHADER, &vs_source) {
                    Ok(vs_id) => vs_id,
                    Err(err) => return Err(err),
                };

            // Compile the fragment shader
            let fs_source = info.compute_source(self, SHADER_KIND_FRAGMENT);
            let fs_id =
                match Device::compile_shader(&*self.gl, &info.base_filename, gl::FRAGMENT_SHADER, &fs_source) {
                    Ok(fs_id) => fs_id,
                    Err(err) => {
                        self.gl.delete_shader(vs_id);
                        return Err(err);
                    }
                };

            // Attach shaders
            self.gl.attach_shader(program.id, vs_id);
            self.gl.attach_shader(program.id, fs_id);

            // Bind vertex attributes
            for (i, attr) in descriptor
                .vertex_attributes
                .iter()
                .chain(descriptor.instance_attributes.iter())
                .enumerate()
            {
                self.gl
                    .bind_attrib_location(program.id, i as gl::GLuint, attr.name);
            }

            if self.cached_programs.is_some() {
                self.gl.program_parameter_i(program.id, gl::PROGRAM_BINARY_RETRIEVABLE_HINT, gl::TRUE as gl::GLint);
            }

            // Link!
            self.gl.link_program(program.id);

            // GL recommends detaching and deleting shaders once the link
            // is complete (whether successful or not). This allows the driver
            // to free any memory associated with the parsing and compilation.
            self.gl.detach_shader(program.id, vs_id);
            self.gl.detach_shader(program.id, fs_id);
            self.gl.delete_shader(vs_id);
            self.gl.delete_shader(fs_id);

            let mut link_status = [0];
            unsafe {
                self.gl.get_program_iv(program.id, gl::LINK_STATUS, &mut link_status);
            }
            if link_status[0] == 0 {
                let error_log = self.gl.get_program_info_log(program.id);
                error!(
                    "Failed to link shader program: {}\n{}",
                    &info.base_filename,
                    error_log
                );
                self.gl.delete_program(program.id);
                return Err(ShaderError::Link(info.base_filename.to_owned(), error_log));
            }

            if let Some(ref cached_programs) = self.cached_programs {
                if !cached_programs.entries.borrow().contains_key(&info.digest) {
                    let (buffer, format) = self.gl.get_program_binary(program.id);
                    if buffer.len() > 0 {
                        let entry = ProgramCacheEntry {
                            binary: Arc::new(ProgramBinary::new(buffer, format, info.digest.clone())),
                            linked: true,
                        };
                        cached_programs.entries.borrow_mut().insert(info.digest.clone(), entry);
                    }
                }
            }
        }

        // If we get here, the link succeeded, so get the uniforms.
        program.is_initialized = true;
        program.u_transform = self.gl.get_uniform_location(program.id, "uTransform");
        program.u_mode = self.gl.get_uniform_location(program.id, "uMode");

        Ok(())
    }

    pub fn bind_program(&mut self, program: &Program) {
        debug_assert!(self.inside_frame);
        debug_assert!(program.is_initialized());

        if self.bound_program != program.id {
            self.gl.use_program(program.id);
            self.bound_program = program.id;
            self.program_mode_id = UniformLocation(program.u_mode);
        }
    }

    pub fn create_texture(
        &mut self,
        target: TextureTarget,
        format: ImageFormat,
        mut width: i32,
        mut height: i32,
        filter: TextureFilter,
        render_target: Option<RenderTargetInfo>,
        layer_count: i32,
    ) -> Texture {
        debug_assert!(self.inside_frame);

        if width > self.max_texture_size || height > self.max_texture_size {
            error!("Attempting to allocate a texture of size {}x{} above the limit, trimming", width, height);
            width = width.min(self.max_texture_size);
            height = height.min(self.max_texture_size);
        }

        // Set up the texture book-keeping.
        let mut texture = Texture {
            id: self.gl.gen_textures(1)[0],
            target: get_gl_target(target),
            size: DeviceIntSize::new(width, height),
            layer_count,
            format,
            filter,
            fbos: vec![],
            fbos_with_depth: vec![],
            last_frame_used: self.frame_id,
            flags: TextureFlags::default(),
        };
        self.bind_texture(DEFAULT_TEXTURE, &texture);
        self.set_texture_parameters(texture.target, filter);

        // Allocate storage.
        let desc = self.gl_describe_format(texture.format);
        let is_array = match texture.target {
            gl::TEXTURE_2D_ARRAY => true,
            gl::TEXTURE_2D | gl::TEXTURE_RECTANGLE | gl::TEXTURE_EXTERNAL_OES => false,
            _ => panic!("BUG: Unexpected texture target!"),
        };
        assert!(is_array || texture.layer_count == 1);

        // Firefox doesn't use mipmaps, but Servo uses them for standalone image
        // textures images larger than 512 pixels. This is the only case where
        // we set the filter to trilinear.
        let mipmap_levels =  if texture.filter == TextureFilter::Trilinear {
            let max_dimension = cmp::max(width, height);
            ((max_dimension) as f64).log2() as gl::GLint + 1
        } else {
            1
        };

        // Use glTexStorage where available, since it avoids allocating
        // unnecessary mipmap storage and generally improves performance with
        // stronger invariants.
        let use_texture_storage = match self.texture_storage_usage {
            TexStorageUsage::Always => true,
            TexStorageUsage::NonBGRA8 => texture.format != ImageFormat::BGRA8,
            TexStorageUsage::Never => false,
        };
        match (use_texture_storage, is_array) {
            (true, true) =>
                self.gl.tex_storage_3d(
                    gl::TEXTURE_2D_ARRAY,
                    mipmap_levels,
                    desc.internal,
                    texture.size.width as gl::GLint,
                    texture.size.height as gl::GLint,
                    texture.layer_count,
                ),
            (true, false) =>
                self.gl.tex_storage_2d(
                    texture.target,
                    mipmap_levels,
                    desc.internal,
                    texture.size.width as gl::GLint,
                    texture.size.height as gl::GLint,
                ),
            (false, true) =>
                self.gl.tex_image_3d(
                    gl::TEXTURE_2D_ARRAY,
                    0,
                    desc.internal as gl::GLint,
                    texture.size.width as gl::GLint,
                    texture.size.height as gl::GLint,
                    texture.layer_count,
                    0,
                    desc.external,
                    desc.pixel_type,
                    None,
                ),
            (false, false) =>
                self.gl.tex_image_2d(
                    texture.target,
                    0,
                    desc.internal as gl::GLint,
                    texture.size.width as gl::GLint,
                    texture.size.height as gl::GLint,
                    0,
                    desc.external,
                    desc.pixel_type,
                    None,
                ),
        }

        // Set up FBOs, if required.
        if let Some(rt_info) = render_target {
            self.init_fbos(&mut texture, false);
            if rt_info.has_depth {
                self.init_fbos(&mut texture, true);
            }
        }

        record_gpu_alloc(texture.size_in_bytes());

        texture
    }

    fn set_texture_parameters(&mut self, target: gl::GLuint, filter: TextureFilter) {
        let mag_filter = match filter {
            TextureFilter::Nearest => gl::NEAREST,
            TextureFilter::Linear | TextureFilter::Trilinear => gl::LINEAR,
        };

        let min_filter = match filter {
            TextureFilter::Nearest => gl::NEAREST,
            TextureFilter::Linear => gl::LINEAR,
            TextureFilter::Trilinear => gl::LINEAR_MIPMAP_LINEAR,
        };

        self.gl
            .tex_parameter_i(target, gl::TEXTURE_MAG_FILTER, mag_filter as gl::GLint);
        self.gl
            .tex_parameter_i(target, gl::TEXTURE_MIN_FILTER, min_filter as gl::GLint);

        self.gl
            .tex_parameter_i(target, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as gl::GLint);
        self.gl
            .tex_parameter_i(target, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as gl::GLint);
    }

    /// Copies the contents from one renderable texture to another.
    pub fn blit_renderable_texture(
        &mut self,
        dst: &mut Texture,
        src: &Texture,
    ) {
        debug_assert!(self.inside_frame);
        debug_assert!(dst.size.width >= src.size.width);
        debug_assert!(dst.size.height >= src.size.height);
        debug_assert!(dst.layer_count >= src.layer_count);

        if self.supports_copy_image_sub_data {
            assert_ne!(src.id, dst.id,
                    "glCopyImageSubData's behaviour is undefined if src and dst images are identical and the rectangles overlap.");
            unsafe {
                self.gl.copy_image_sub_data(src.id, src.target, 0,
                                            0, 0, 0,
                                            dst.id, dst.target, 0,
                                            0, 0, 0,
                                            src.size.width as _, src.size.height as _, src.layer_count);
            }
        } else {
            // Note that zip() truncates to the shorter of the two iterators.
            let rect = DeviceIntRect::new(DeviceIntPoint::zero(), src.get_dimensions().to_i32());
            for (read_fbo, draw_fbo) in src.fbos.iter().zip(&dst.fbos) {
                self.bind_read_target_impl(*read_fbo);
                self.bind_draw_target_impl(*draw_fbo);
                self.blit_render_target(rect, rect);
            }
            self.reset_draw_target();
            self.reset_read_target();
        }
    }

    /// Notifies the device that the contents of a render target are no longer
    /// needed.
    ///
    /// FIXME(bholley): We could/should invalidate the depth targets earlier
    /// than the color targets, i.e. immediately after each pass.
    pub fn invalidate_render_target(&mut self, texture: &Texture) {
        let (fbos, attachments) = if texture.supports_depth() {
            (&texture.fbos_with_depth,
             &[gl::COLOR_ATTACHMENT0, gl::DEPTH_ATTACHMENT] as &[gl::GLenum])
        } else {
            (&texture.fbos, &[gl::COLOR_ATTACHMENT0] as &[gl::GLenum])
        };

        let original_bound_fbo = self.bound_draw_fbo;
        for fbo_id in fbos.iter() {
            // Note: The invalidate extension may not be supported, in which
            // case this is a no-op. That's ok though, because it's just a
            // hint.
            self.bind_external_draw_target(*fbo_id);
            self.gl.invalidate_framebuffer(gl::FRAMEBUFFER, attachments);
        }
        self.bind_external_draw_target(original_bound_fbo);
    }

    /// Notifies the device that a render target is about to be reused.
    ///
    /// This method adds or removes a depth target as necessary.
    pub fn reuse_render_target<T: Texel>(
        &mut self,
        texture: &mut Texture,
        rt_info: RenderTargetInfo,
    ) {
        texture.last_frame_used = self.frame_id;

        // Add depth support if needed.
        if rt_info.has_depth && !texture.supports_depth() {
            self.init_fbos(texture, true);
        }
    }

    fn init_fbos(&mut self, texture: &mut Texture, with_depth: bool) {
        let (fbos, depth_rb) = if with_depth {
            let depth_target = self.acquire_depth_target(texture.get_dimensions());
            (&mut texture.fbos_with_depth, Some(depth_target))
        } else {
            (&mut texture.fbos, None)
        };

        // Generate the FBOs.
        assert!(fbos.is_empty());
        fbos.extend(self.gl.gen_framebuffers(texture.layer_count).into_iter().map(FBOId));

        // Bind the FBOs.
        let original_bound_fbo = self.bound_draw_fbo;
        for (fbo_index, &fbo_id) in fbos.iter().enumerate() {
            self.bind_external_draw_target(fbo_id);
            match texture.target {
                gl::TEXTURE_2D_ARRAY => {
                    self.gl.framebuffer_texture_layer(
                        gl::DRAW_FRAMEBUFFER,
                        gl::COLOR_ATTACHMENT0,
                        texture.id,
                        0,
                        fbo_index as _,
                    )
                }
                _ => {
                    assert_eq!(fbo_index, 0);
                    self.gl.framebuffer_texture_2d(
                        gl::DRAW_FRAMEBUFFER,
                        gl::COLOR_ATTACHMENT0,
                        texture.target,
                        texture.id,
                        0,
                    )
                }
            }

            if let Some(depth_rb) = depth_rb {
                self.gl.framebuffer_renderbuffer(
                    gl::DRAW_FRAMEBUFFER,
                    gl::DEPTH_ATTACHMENT,
                    gl::RENDERBUFFER,
                    depth_rb.0,
                );
            }
        }
        self.bind_external_draw_target(original_bound_fbo);
    }

    fn deinit_fbos(&mut self, fbos: &mut Vec<FBOId>) {
        if !fbos.is_empty() {
            let fbo_ids: SmallVec<[gl::GLuint; 8]> = fbos
                .drain(..)
                .map(|FBOId(fbo_id)| fbo_id)
                .collect();
            self.gl.delete_framebuffers(&fbo_ids[..]);
        }
    }

    fn acquire_depth_target(&mut self, dimensions: DeviceIntSize) -> RBOId {
        let gl = &self.gl;
        let target = self.depth_targets.entry(dimensions).or_insert_with(|| {
            let renderbuffer_ids = gl.gen_renderbuffers(1);
            let depth_rb = renderbuffer_ids[0];
            gl.bind_renderbuffer(gl::RENDERBUFFER, depth_rb);
            gl.renderbuffer_storage(
                gl::RENDERBUFFER,
                gl::DEPTH_COMPONENT24,
                dimensions.width as _,
                dimensions.height as _,
            );
            SharedDepthTarget {
                rbo_id: RBOId(depth_rb),
                refcount: 0,
            }
        });
        if target.refcount == 0 {
            record_gpu_alloc(depth_target_size_in_bytes(&dimensions));
        }
        target.refcount += 1;
        target.rbo_id
    }

    fn release_depth_target(&mut self, dimensions: DeviceIntSize) {
        let mut entry = match self.depth_targets.entry(dimensions) {
            Entry::Occupied(x) => x,
            Entry::Vacant(..) => panic!("Releasing unknown depth target"),
        };
        debug_assert!(entry.get().refcount != 0);
        entry.get_mut().refcount -= 1;
        if entry.get().refcount == 0 {
            let (dimensions, target) = entry.remove_entry();
            self.gl.delete_renderbuffers(&[target.rbo_id.0]);
            record_gpu_free(depth_target_size_in_bytes(&dimensions));
        }
    }

    pub fn blit_render_target(&mut self, src_rect: DeviceIntRect, dest_rect: DeviceIntRect) {
        debug_assert!(self.inside_frame);

        self.gl.blit_framebuffer(
            src_rect.origin.x,
            src_rect.origin.y,
            src_rect.origin.x + src_rect.size.width,
            src_rect.origin.y + src_rect.size.height,
            dest_rect.origin.x,
            dest_rect.origin.y,
            dest_rect.origin.x + dest_rect.size.width,
            dest_rect.origin.y + dest_rect.size.height,
            gl::COLOR_BUFFER_BIT,
            gl::LINEAR,
        );
    }

    /// Performs a blit while flipping vertically. Useful for blitting textures
    /// (which use origin-bottom-left) to the main framebuffer (which uses
    /// origin-top-left).
    pub fn blit_render_target_invert_y(
        &mut self,
        src_rect: DeviceIntRect,
        dest_rect: DeviceIntRect,
    ) {
        debug_assert!(self.inside_frame);
        self.gl.blit_framebuffer(
            src_rect.origin.x,
            src_rect.origin.y,
            src_rect.origin.x + src_rect.size.width,
            src_rect.origin.y + src_rect.size.height,
            dest_rect.origin.x,
            dest_rect.origin.y + dest_rect.size.height,
            dest_rect.origin.x + dest_rect.size.width,
            dest_rect.origin.y,
            gl::COLOR_BUFFER_BIT,
            gl::LINEAR,
        );
    }

    pub fn delete_texture(&mut self, mut texture: Texture) {
        debug_assert!(self.inside_frame);
        record_gpu_free(texture.size_in_bytes());
        let had_depth = texture.supports_depth();
        self.deinit_fbos(&mut texture.fbos);
        self.deinit_fbos(&mut texture.fbos_with_depth);
        if had_depth {
            self.release_depth_target(texture.get_dimensions());
        }

        self.gl.delete_textures(&[texture.id]);

        for bound_texture in &mut self.bound_textures {
            if *bound_texture == texture.id {
                *bound_texture = 0
            }
        }

        // Disarm the assert in Texture::drop().
        texture.id = 0;
    }

    #[cfg(feature = "replay")]
    pub fn delete_external_texture(&mut self, mut external: ExternalTexture) {
        self.gl.delete_textures(&[external.id]);
        external.id = 0;
    }

    pub fn delete_program(&mut self, mut program: Program) {
        self.gl.delete_program(program.id);
        program.id = 0;
    }

    /// Create a shader program and link it immediately.
    pub fn create_program_linked(
        &mut self,
        base_filename: &'static str,
        features: String,
        descriptor: &VertexDescriptor,
    ) -> Result<Program, ShaderError> {
        let mut program = self.create_program(base_filename, features)?;
        self.link_program(&mut program, descriptor)?;
        Ok(program)
    }

    /// Create a shader program. This does minimal amount of work to start
    /// loading a binary shader. If a binary shader is found, we invoke
    /// glProgramBinary, which, at least on ANGLE, will load and link the
    /// binary on a background thread. This can speed things up later when
    /// we invoke `link_program()`.
    pub fn create_program(
        &mut self,
        base_filename: &'static str,
        features: String,
    ) -> Result<Program, ShaderError> {
        debug_assert!(self.inside_frame);

        let source_info = ProgramSourceInfo::new(self, base_filename, features);

        // Create program
        let pid = self.gl.create_program();

        // Attempt to load a cached binary if possible.
        if let Some(ref cached_programs) = self.cached_programs {
            if let Some(entry) = cached_programs.entries.borrow().get(&source_info.digest) {
                self.gl.program_binary(pid, entry.binary.format, &entry.binary.bytes);
            }
        }

        // Use 0 for the uniforms as they are initialized by link_program.
        let program = Program {
            id: pid,
            u_transform: 0,
            u_mode: 0,
            source_info,
            is_initialized: false,
        };

        Ok(program)
    }

    fn build_shader_string<F: FnMut(&str)>(
        &self,
        features: &str,
        kind: &str,
        base_filename: &str,
        output: F,
    ) {
        do_build_shader_string(
            get_shader_version(&*self.gl),
            features,
            kind,
            base_filename,
            &self.resource_override_path,
            output,
        )
    }

    pub fn bind_shader_samplers<S>(&mut self, program: &Program, bindings: &[(&'static str, S)])
    where
        S: Into<TextureSlot> + Copy,
    {
        // bind_program() must be called before calling bind_shader_samplers
        assert_eq!(self.bound_program, program.id);

        for binding in bindings {
            let u_location = self.gl.get_uniform_location(program.id, binding.0);
            if u_location != -1 {
                self.bind_program(program);
                self.gl
                    .uniform_1i(u_location, binding.1.into().0 as gl::GLint);
            }
        }
    }

    #[cfg(feature = "debug_renderer")]
    pub fn get_uniform_location(&self, program: &Program, name: &str) -> UniformLocation {
        UniformLocation(self.gl.get_uniform_location(program.id, name))
    }

    pub fn set_uniforms(
        &self,
        program: &Program,
        transform: &Transform3D<f32>,
    ) {
        debug_assert!(self.inside_frame);
        self.gl
            .uniform_matrix_4fv(program.u_transform, false, &transform.to_row_major_array());
    }

    pub fn switch_mode(&self, mode: i32) {
        debug_assert!(self.inside_frame);
        self.gl.uniform_1i(self.program_mode_id.0, mode);
    }

    pub fn create_pbo(&mut self) -> PBO {
        let id = self.gl.gen_buffers(1)[0];
        PBO { id }
    }

    pub fn delete_pbo(&mut self, mut pbo: PBO) {
        self.gl.delete_buffers(&[pbo.id]);
        pbo.id = 0;
    }

    pub fn upload_texture<'a, T>(
        &'a mut self,
        texture: &'a Texture,
        pbo: &PBO,
        upload_count: usize,
    ) -> TextureUploader<'a, T> {
        debug_assert!(self.inside_frame);
        self.bind_texture(DEFAULT_TEXTURE, texture);

        let buffer = match self.upload_method {
            UploadMethod::Immediate => None,
            UploadMethod::PixelBuffer(hint) => {
                let upload_size = upload_count * mem::size_of::<T>();
                self.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, pbo.id);
                if upload_size != 0 {
                    self.gl.buffer_data_untyped(
                        gl::PIXEL_UNPACK_BUFFER,
                        upload_size as _,
                        ptr::null(),
                        hint.to_gl(),
                    );
                }
                Some(PixelBuffer::new(hint.to_gl(), upload_size))
            },
        };

        TextureUploader {
            target: UploadTarget {
                gl: &*self.gl,
                bgra_format: self.bgra_format_external,
                texture,
            },
            buffer,
            marker: PhantomData,
        }
    }

    /// Performs an immediate (non-PBO) texture upload.
    pub fn upload_texture_immediate<T: Texel>(
        &mut self,
        texture: &Texture,
        pixels: &[T]
    ) {
        self.bind_texture(DEFAULT_TEXTURE, texture);
        let desc = self.gl_describe_format(texture.format);
        match texture.target {
            gl::TEXTURE_2D | gl::TEXTURE_RECTANGLE | gl::TEXTURE_EXTERNAL_OES =>
                self.gl.tex_sub_image_2d(
                    texture.target,
                    0,
                    0,
                    0,
                    texture.size.width as gl::GLint,
                    texture.size.height as gl::GLint,
                    desc.external,
                    desc.pixel_type,
                    texels_to_u8_slice(pixels),
                ),
            gl::TEXTURE_2D_ARRAY =>
                self.gl.tex_sub_image_3d(
                    texture.target,
                    0,
                    0,
                    0,
                    0,
                    texture.size.width as gl::GLint,
                    texture.size.height as gl::GLint,
                    texture.layer_count as gl::GLint,
                    desc.external,
                    desc.pixel_type,
                    texels_to_u8_slice(pixels),
                ),
            _ => panic!("BUG: Unexpected texture target!"),
        }
    }

    #[cfg(any(feature = "debug_renderer", feature = "capture"))]
    pub fn read_pixels(&mut self, img_desc: &ImageDescriptor) -> Vec<u8> {
        let desc = self.gl_describe_format(img_desc.format);
        self.gl.read_pixels(
            0, 0,
            img_desc.size.width as i32,
            img_desc.size.height as i32,
            desc.external,
            desc.pixel_type,
        )
    }

    /// Read rectangle of pixels into the specified output slice.
    pub fn read_pixels_into(
        &mut self,
        rect: DeviceIntRect,
        format: ReadPixelsFormat,
        output: &mut [u8],
    ) {
        let (bytes_per_pixel, desc) = match format {
            ReadPixelsFormat::Standard(imf) => {
                (imf.bytes_per_pixel(), self.gl_describe_format(imf))
            }
            ReadPixelsFormat::Rgba8 => {
                (4, FormatDesc {
                    external: gl::RGBA,
                    internal: gl::RGBA8,
                    pixel_type: gl::UNSIGNED_BYTE,
                })
            }
        };
        let size_in_bytes = (bytes_per_pixel * rect.size.width * rect.size.height) as usize;
        assert_eq!(output.len(), size_in_bytes);

        self.gl.flush();
        self.gl.read_pixels_into_buffer(
            rect.origin.x as _,
            rect.origin.y as _,
            rect.size.width as _,
            rect.size.height as _,
            desc.external,
            desc.pixel_type,
            output,
        );
    }

    /// Get texels of a texture into the specified output slice.
    #[cfg(feature = "debug_renderer")]
    pub fn get_tex_image_into(
        &mut self,
        texture: &Texture,
        format: ImageFormat,
        output: &mut [u8],
    ) {
        self.bind_texture(DEFAULT_TEXTURE, texture);
        let desc = self.gl_describe_format(format);
        self.gl.get_tex_image_into_buffer(
            texture.target,
            0,
            desc.external,
            desc.pixel_type,
            output,
        );
    }

    /// Attaches the provided texture to the current Read FBO binding.
    #[cfg(any(feature = "debug_renderer", feature="capture"))]
    fn attach_read_texture_raw(
        &mut self, texture_id: gl::GLuint, target: gl::GLuint, layer_id: i32
    ) {
        match target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.framebuffer_texture_layer(
                    gl::READ_FRAMEBUFFER,
                    gl::COLOR_ATTACHMENT0,
                    texture_id,
                    0,
                    layer_id,
                )
            }
            _ => {
                assert_eq!(layer_id, 0);
                self.gl.framebuffer_texture_2d(
                    gl::READ_FRAMEBUFFER,
                    gl::COLOR_ATTACHMENT0,
                    target,
                    texture_id,
                    0,
                )
            }
        }
    }

    #[cfg(any(feature = "debug_renderer", feature="capture"))]
    pub fn attach_read_texture_external(
        &mut self, texture_id: gl::GLuint, target: TextureTarget, layer_id: i32
    ) {
        self.attach_read_texture_raw(texture_id, get_gl_target(target), layer_id)
    }

    #[cfg(any(feature = "debug_renderer", feature="capture"))]
    pub fn attach_read_texture(&mut self, texture: &Texture, layer_id: i32) {
        self.attach_read_texture_raw(texture.id, texture.target, layer_id)
    }

    fn bind_vao_impl(&mut self, id: gl::GLuint) {
        debug_assert!(self.inside_frame);

        if self.bound_vao != id {
            self.bound_vao = id;
            self.gl.bind_vertex_array(id);
        }
    }

    pub fn bind_vao(&mut self, vao: &VAO) {
        self.bind_vao_impl(vao.id)
    }

    pub fn bind_custom_vao(&mut self, vao: &CustomVAO) {
        self.bind_vao_impl(vao.id)
    }

    fn create_vao_with_vbos(
        &mut self,
        descriptor: &VertexDescriptor,
        main_vbo_id: VBOId,
        instance_vbo_id: VBOId,
        ibo_id: IBOId,
        owns_vertices_and_indices: bool,
    ) -> VAO {
        debug_assert!(self.inside_frame);

        let instance_stride = descriptor.instance_stride() as usize;
        let vao_id = self.gl.gen_vertex_arrays(1)[0];

        self.gl.bind_vertex_array(vao_id);

        descriptor.bind(self.gl(), main_vbo_id, instance_vbo_id);
        ibo_id.bind(self.gl()); // force it to be a part of VAO

        self.gl.bind_vertex_array(0);

        VAO {
            id: vao_id,
            ibo_id,
            main_vbo_id,
            instance_vbo_id,
            instance_stride,
            owns_vertices_and_indices,
        }
    }

    pub fn create_custom_vao(
        &mut self,
        streams: &[Stream],
    ) -> CustomVAO {
        debug_assert!(self.inside_frame);

        let vao_id = self.gl.gen_vertex_arrays(1)[0];
        self.gl.bind_vertex_array(vao_id);

        let mut attrib_index = 0;
        for stream in streams {
            VertexDescriptor::bind_attributes(
                stream.attributes,
                attrib_index,
                0,
                self.gl(),
                stream.vbo,
            );
            attrib_index += stream.attributes.len();
        }

        self.gl.bind_vertex_array(0);

        CustomVAO {
            id: vao_id,
        }
    }

    pub fn delete_custom_vao(&mut self, mut vao: CustomVAO) {
        self.gl.delete_vertex_arrays(&[vao.id]);
        vao.id = 0;
    }

    pub fn create_vbo<T>(&mut self) -> VBO<T> {
        let ids = self.gl.gen_buffers(1);
        VBO {
            id: ids[0],
            target: gl::ARRAY_BUFFER,
            allocated_count: 0,
            marker: PhantomData,
        }
    }

    pub fn delete_vbo<T>(&mut self, mut vbo: VBO<T>) {
        self.gl.delete_buffers(&[vbo.id]);
        vbo.id = 0;
    }

    pub fn create_vao(&mut self, descriptor: &VertexDescriptor) -> VAO {
        debug_assert!(self.inside_frame);

        let buffer_ids = self.gl.gen_buffers(3);
        let ibo_id = IBOId(buffer_ids[0]);
        let main_vbo_id = VBOId(buffer_ids[1]);
        let intance_vbo_id = VBOId(buffer_ids[2]);

        self.create_vao_with_vbos(descriptor, main_vbo_id, intance_vbo_id, ibo_id, true)
    }

    pub fn delete_vao(&mut self, mut vao: VAO) {
        self.gl.delete_vertex_arrays(&[vao.id]);
        vao.id = 0;

        if vao.owns_vertices_and_indices {
            self.gl.delete_buffers(&[vao.ibo_id.0]);
            self.gl.delete_buffers(&[vao.main_vbo_id.0]);
        }

        self.gl.delete_buffers(&[vao.instance_vbo_id.0])
    }

    pub fn allocate_vbo<V>(
        &mut self,
        vbo: &mut VBO<V>,
        count: usize,
        usage_hint: VertexUsageHint,
    ) {
        debug_assert!(self.inside_frame);
        vbo.allocated_count = count;

        self.gl.bind_buffer(vbo.target, vbo.id);
        self.gl.buffer_data_untyped(
            vbo.target,
            (count * mem::size_of::<V>()) as _,
            ptr::null(),
            usage_hint.to_gl(),
        );
    }

    pub fn fill_vbo<V>(
        &mut self,
        vbo: &VBO<V>,
        data: &[V],
        offset: usize,
    ) {
        debug_assert!(self.inside_frame);
        assert!(offset + data.len() <= vbo.allocated_count);
        let stride = mem::size_of::<V>();

        self.gl.bind_buffer(vbo.target, vbo.id);
        self.gl.buffer_sub_data_untyped(
            vbo.target,
            (offset * stride) as _,
            (data.len() * stride) as _,
            data.as_ptr() as _,
        );
    }

    fn update_vbo_data<V>(
        &mut self,
        vbo: VBOId,
        vertices: &[V],
        usage_hint: VertexUsageHint,
    ) {
        debug_assert!(self.inside_frame);

        vbo.bind(self.gl());
        gl::buffer_data(self.gl(), gl::ARRAY_BUFFER, vertices, usage_hint.to_gl());
    }

    pub fn create_vao_with_new_instances(
        &mut self,
        descriptor: &VertexDescriptor,
        base_vao: &VAO,
    ) -> VAO {
        debug_assert!(self.inside_frame);

        let buffer_ids = self.gl.gen_buffers(1);
        let intance_vbo_id = VBOId(buffer_ids[0]);

        self.create_vao_with_vbos(
            descriptor,
            base_vao.main_vbo_id,
            intance_vbo_id,
            base_vao.ibo_id,
            false,
        )
    }

    pub fn update_vao_main_vertices<V>(
        &mut self,
        vao: &VAO,
        vertices: &[V],
        usage_hint: VertexUsageHint,
    ) {
        debug_assert_eq!(self.bound_vao, vao.id);
        self.update_vbo_data(vao.main_vbo_id, vertices, usage_hint)
    }

    pub fn update_vao_instances<V>(
        &mut self,
        vao: &VAO,
        instances: &[V],
        usage_hint: VertexUsageHint,
    ) {
        debug_assert_eq!(self.bound_vao, vao.id);
        debug_assert_eq!(vao.instance_stride as usize, mem::size_of::<V>());

        self.update_vbo_data(vao.instance_vbo_id, instances, usage_hint)
    }

    pub fn update_vao_indices<I>(&mut self, vao: &VAO, indices: &[I], usage_hint: VertexUsageHint) {
        debug_assert!(self.inside_frame);
        debug_assert_eq!(self.bound_vao, vao.id);

        vao.ibo_id.bind(self.gl());
        gl::buffer_data(
            self.gl(),
            gl::ELEMENT_ARRAY_BUFFER,
            indices,
            usage_hint.to_gl(),
        );
    }

    pub fn draw_triangles_u16(&mut self, first_vertex: i32, index_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements(
            gl::TRIANGLES,
            index_count,
            gl::UNSIGNED_SHORT,
            first_vertex as u32 * 2,
        );
    }

    #[cfg(feature = "debug_renderer")]
    pub fn draw_triangles_u32(&mut self, first_vertex: i32, index_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements(
            gl::TRIANGLES,
            index_count,
            gl::UNSIGNED_INT,
            first_vertex as u32 * 4,
        );
    }

    pub fn draw_nonindexed_points(&mut self, first_vertex: i32, vertex_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_arrays(gl::POINTS, first_vertex, vertex_count);
    }

    #[cfg(feature = "debug_renderer")]
    pub fn draw_nonindexed_lines(&mut self, first_vertex: i32, vertex_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_arrays(gl::LINES, first_vertex, vertex_count);
    }

    pub fn draw_indexed_triangles_instanced_u16(&mut self, index_count: i32, instance_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements_instanced(
            gl::TRIANGLES,
            index_count,
            gl::UNSIGNED_SHORT,
            0,
            instance_count,
        );
    }

    pub fn end_frame(&mut self) {
        self.reset_draw_target();
        self.reset_read_target();

        debug_assert!(self.inside_frame);
        self.inside_frame = false;

        self.gl.bind_texture(gl::TEXTURE_2D, 0);
        self.gl.use_program(0);

        for i in 0 .. self.bound_textures.len() {
            self.gl.active_texture(gl::TEXTURE0 + i as gl::GLuint);
            self.gl.bind_texture(gl::TEXTURE_2D, 0);
        }

        self.gl.active_texture(gl::TEXTURE0);

        self.frame_id.0 += 1;

        // Declare startup complete after the first ten frames. This number is
        // basically a heuristic, which dictates how early a shader needs to be
        // used in order to be cached to disk.
        if self.frame_id.0 == 10 {
            if let Some(ref cache) = self.cached_programs {
                cache.startup_complete();
            }
        }
    }

    pub fn clear_target(
        &self,
        color: Option<[f32; 4]>,
        depth: Option<f32>,
        rect: Option<DeviceIntRect>,
    ) {
        let mut clear_bits = 0;

        if let Some(color) = color {
            self.gl.clear_color(color[0], color[1], color[2], color[3]);
            clear_bits |= gl::COLOR_BUFFER_BIT;
        }

        if let Some(depth) = depth {
            if cfg!(debug_assertions) {
                let mut mask = [0];
                unsafe {
                    self.gl.get_boolean_v(gl::DEPTH_WRITEMASK, &mut mask);
                }
                assert_ne!(mask[0], 0);
            }
            self.gl.clear_depth(depth as f64);
            clear_bits |= gl::DEPTH_BUFFER_BIT;
        }

        if clear_bits != 0 {
            match rect {
                Some(rect) => {
                    self.gl.enable(gl::SCISSOR_TEST);
                    self.gl.scissor(
                        rect.origin.x,
                        rect.origin.y,
                        rect.size.width,
                        rect.size.height,
                    );
                    self.gl.clear(clear_bits);
                    self.gl.disable(gl::SCISSOR_TEST);
                }
                None => {
                    self.gl.clear(clear_bits);
                }
            }
        }
    }

    pub fn enable_depth(&self) {
        assert!(self.depth_available, "Enabling depth test without depth target");
        self.gl.enable(gl::DEPTH_TEST);
    }

    pub fn disable_depth(&self) {
        self.gl.disable(gl::DEPTH_TEST);
    }

    pub fn set_depth_func(&self, depth_func: DepthFunction) {
        self.gl.depth_func(depth_func as gl::GLuint);
    }

    pub fn enable_depth_write(&self) {
        assert!(self.depth_available, "Enabling depth write without depth target");
        self.gl.depth_mask(true);
    }

    pub fn disable_depth_write(&self) {
        self.gl.depth_mask(false);
    }

    pub fn disable_stencil(&self) {
        self.gl.disable(gl::STENCIL_TEST);
    }

    pub fn set_scissor_rect(&self, rect: DeviceIntRect) {
        self.gl.scissor(
            rect.origin.x,
            rect.origin.y,
            rect.size.width,
            rect.size.height,
        );
    }

    pub fn enable_scissor(&self) {
        self.gl.enable(gl::SCISSOR_TEST);
    }

    pub fn disable_scissor(&self) {
        self.gl.disable(gl::SCISSOR_TEST);
    }

    pub fn set_blend(&self, enable: bool) {
        if enable {
            self.gl.enable(gl::BLEND);
        } else {
            self.gl.disable(gl::BLEND);
        }
    }

    pub fn set_blend_mode_alpha(&self) {
        self.gl.blend_func_separate(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA,
                                    gl::ONE, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn set_blend_mode_premultiplied_alpha(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn set_blend_mode_premultiplied_dest_out(&self) {
        self.gl.blend_func(gl::ZERO, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn set_blend_mode_multiply(&self) {
        self.gl
            .blend_func_separate(gl::ZERO, gl::SRC_COLOR, gl::ZERO, gl::SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_max(&self) {
        self.gl
            .blend_func_separate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);
        self.gl.blend_equation_separate(gl::MAX, gl::FUNC_ADD);
    }
    #[cfg(feature = "debug_renderer")]
    pub fn set_blend_mode_min(&self) {
        self.gl
            .blend_func_separate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);
        self.gl.blend_equation_separate(gl::MIN, gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_pass0(&self) {
        self.gl.blend_func(gl::ZERO, gl::ONE_MINUS_SRC_COLOR);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_pass1(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_with_bg_color_pass0(&self) {
        self.gl.blend_func_separate(gl::ZERO, gl::ONE_MINUS_SRC_COLOR, gl::ZERO, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_with_bg_color_pass1(&self) {
        self.gl.blend_func_separate(gl::ONE_MINUS_DST_ALPHA, gl::ONE, gl::ZERO, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_with_bg_color_pass2(&self) {
        self.gl.blend_func_separate(gl::ONE, gl::ONE, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_constant_text_color(&self, color: ColorF) {
        // color is an unpremultiplied color.
        self.gl.blend_color(color.r, color.g, color.b, 1.0);
        self.gl
            .blend_func(gl::CONSTANT_COLOR, gl::ONE_MINUS_SRC_COLOR);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_dual_source(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC1_COLOR);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_show_overdraw(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn supports_extension(&self, extension: &str) -> bool {
        supports_extension(&self.extensions, extension)
    }

    pub fn echo_driver_messages(&self) {
        for msg in self.gl.get_debug_messages() {
            let level = match msg.severity {
                gl::DEBUG_SEVERITY_HIGH => Level::Error,
                gl::DEBUG_SEVERITY_MEDIUM => Level::Warn,
                gl::DEBUG_SEVERITY_LOW => Level::Info,
                gl::DEBUG_SEVERITY_NOTIFICATION => Level::Debug,
                _ => Level::Trace,
            };
            let ty = match msg.ty {
                gl::DEBUG_TYPE_ERROR => "error",
                gl::DEBUG_TYPE_DEPRECATED_BEHAVIOR => "deprecated",
                gl::DEBUG_TYPE_UNDEFINED_BEHAVIOR => "undefined",
                gl::DEBUG_TYPE_PORTABILITY => "portability",
                gl::DEBUG_TYPE_PERFORMANCE => "perf",
                gl::DEBUG_TYPE_MARKER => "marker",
                gl::DEBUG_TYPE_PUSH_GROUP => "group push",
                gl::DEBUG_TYPE_POP_GROUP => "group pop",
                gl::DEBUG_TYPE_OTHER => "other",
                _ => "?",
            };
            log!(level, "({}) {}", ty, msg.message);
        }
    }

    fn gl_describe_format(&self, format: ImageFormat) -> FormatDesc {
        match format {
            ImageFormat::R8 => FormatDesc {
                internal: gl::R8,
                external: gl::RED,
                pixel_type: gl::UNSIGNED_BYTE,
            },
            ImageFormat::R16 => FormatDesc {
                internal: gl::R16,
                external: gl::RED,
                pixel_type: gl::UNSIGNED_SHORT,
            },
            ImageFormat::BGRA8 => {
                FormatDesc {
                    internal: self.bgra_format_internal,
                    external: self.bgra_format_external,
                    pixel_type: gl::UNSIGNED_BYTE,
                }
            },
            ImageFormat::RGBA8 => {
                FormatDesc {
                    internal: gl::RGBA8,
                    external: gl::RGBA,
                    pixel_type: gl::UNSIGNED_BYTE,
                }
            },
            ImageFormat::RGBAF32 => FormatDesc {
                internal: gl::RGBA32F,
                external: gl::RGBA,
                pixel_type: gl::FLOAT,
            },
            ImageFormat::RGBAI32 => FormatDesc {
                internal: gl::RGBA32I,
                external: gl::RGBA_INTEGER,
                pixel_type: gl::INT,
            },
            ImageFormat::RG8 => FormatDesc {
                internal: gl::RG8,
                external: gl::RG,
                pixel_type: gl::UNSIGNED_BYTE,
            },
        }
    }

    /// Generates a memory report for the resources managed by the device layer.
    pub fn report_memory(&self) -> MemoryReport {
        let mut report = MemoryReport::default();
        for dim in self.depth_targets.keys() {
            report.depth_target_textures += depth_target_size_in_bytes(dim);
        }
        report
    }
}

struct FormatDesc {
    internal: gl::GLenum,
    external: gl::GLuint,
    pixel_type: gl::GLuint,
}

struct UploadChunk {
    rect: DeviceIntRect,
    layer_index: i32,
    stride: Option<i32>,
    offset: usize,
}

struct PixelBuffer {
    usage: gl::GLenum,
    size_allocated: usize,
    size_used: usize,
    // small vector avoids heap allocation for a single chunk
    chunks: SmallVec<[UploadChunk; 1]>,
}

impl PixelBuffer {
    fn new(
        usage: gl::GLenum,
        size_allocated: usize,
    ) -> Self {
        PixelBuffer {
            usage,
            size_allocated,
            size_used: 0,
            chunks: SmallVec::new(),
        }
    }
}

struct UploadTarget<'a> {
    gl: &'a gl::Gl,
    bgra_format: gl::GLuint,
    texture: &'a Texture,
}

pub struct TextureUploader<'a, T> {
    target: UploadTarget<'a>,
    buffer: Option<PixelBuffer>,
    marker: PhantomData<T>,
}

impl<'a, T> Drop for TextureUploader<'a, T> {
    fn drop(&mut self) {
        if let Some(buffer) = self.buffer.take() {
            for chunk in buffer.chunks {
                self.target.update_impl(chunk);
            }
            self.target.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
        }
    }
}

impl<'a, T> TextureUploader<'a, T> {
    pub fn upload(
        &mut self,
        rect: DeviceIntRect,
        layer_index: i32,
        stride: Option<i32>,
        data: &[T],
    ) -> usize {
        let bytes_pp = self.target.texture.format.bytes_per_pixel();
        let upload_size = match stride {
            Some(stride) => ((rect.size.height - 1) * stride + rect.size.width * bytes_pp) as usize,
            None => (rect.size.area() * bytes_pp) as usize,
        };
        assert!(upload_size <= data.len() * mem::size_of::<T>());

        match self.buffer {
            Some(ref mut buffer) => {
                let elem_count = upload_size / mem::size_of::<T>();
                assert_eq!(elem_count * mem::size_of::<T>(), upload_size);
                let slice = &data[.. elem_count];

                if buffer.size_used + upload_size > buffer.size_allocated {
                    // flush
                    for chunk in buffer.chunks.drain() {
                        self.target.update_impl(chunk);
                    }
                    buffer.size_used = 0;
                }

                if upload_size > buffer.size_allocated {
                    gl::buffer_data(
                        self.target.gl,
                        gl::PIXEL_UNPACK_BUFFER,
                        slice,
                        buffer.usage,
                    );
                    buffer.size_allocated = upload_size;
                } else {
                    gl::buffer_sub_data(
                        self.target.gl,
                        gl::PIXEL_UNPACK_BUFFER,
                        buffer.size_used as _,
                        slice,
                    );
                }

                buffer.chunks.push(UploadChunk {
                    rect, layer_index, stride,
                    offset: buffer.size_used,
                });
                buffer.size_used += upload_size;
            }
            None => {
                self.target.update_impl(UploadChunk {
                    rect, layer_index, stride,
                    offset: data.as_ptr() as _,
                });
            }
        }

        upload_size
    }
}

impl<'a> UploadTarget<'a> {
    fn update_impl(&mut self, chunk: UploadChunk) {
        let (gl_format, bpp, data_type) = match self.texture.format {
            ImageFormat::R8 => (gl::RED, 1, gl::UNSIGNED_BYTE),
            ImageFormat::R16 => (gl::RED, 2, gl::UNSIGNED_SHORT),
            ImageFormat::BGRA8 => (self.bgra_format, 4, gl::UNSIGNED_BYTE),
            ImageFormat::RGBA8 => (gl::RGBA, 4, gl::UNSIGNED_BYTE),
            ImageFormat::RG8 => (gl::RG, 2, gl::UNSIGNED_BYTE),
            ImageFormat::RGBAF32 => (gl::RGBA, 16, gl::FLOAT),
            ImageFormat::RGBAI32 => (gl::RGBA_INTEGER, 16, gl::INT),
        };

        let row_length = match chunk.stride {
            Some(value) => value / bpp,
            None => self.texture.size.width,
        };

        if chunk.stride.is_some() {
            self.gl.pixel_store_i(
                gl::UNPACK_ROW_LENGTH,
                row_length as _,
            );
        }

        let pos = chunk.rect.origin;
        let size = chunk.rect.size;

        match self.texture.target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.tex_sub_image_3d_pbo(
                    self.texture.target,
                    0,
                    pos.x as _,
                    pos.y as _,
                    chunk.layer_index,
                    size.width as _,
                    size.height as _,
                    1,
                    gl_format,
                    data_type,
                    chunk.offset,
                );
            }
            gl::TEXTURE_2D | gl::TEXTURE_RECTANGLE | gl::TEXTURE_EXTERNAL_OES => {
                self.gl.tex_sub_image_2d_pbo(
                    self.texture.target,
                    0,
                    pos.x as _,
                    pos.y as _,
                    size.width as _,
                    size.height as _,
                    gl_format,
                    data_type,
                    chunk.offset,
                );
            }
            _ => panic!("BUG: Unexpected texture target!"),
        }

        // If using tri-linear filtering, build the mip-map chain for this texture.
        if self.texture.filter == TextureFilter::Trilinear {
            self.gl.generate_mipmap(self.texture.target);
        }

        // Reset row length to 0, otherwise the stride would apply to all texture uploads.
        if chunk.stride.is_some() {
            self.gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, 0 as _);
        }
    }
}

fn texels_to_u8_slice<T: Texel>(texels: &[T]) -> &[u8] {
    unsafe {
        slice::from_raw_parts(texels.as_ptr() as *const u8, texels.len() * mem::size_of::<T>())
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    cow_label, error::HasErrorBufferType, wgpu_string, AdapterInformation, ByteBuf,
    CommandEncoderAction, DeviceAction, ImplicitLayout, QueueWriteAction, RawString,
    TexelCopyBufferLayout, TextureAction,
};

use crate::{BufferMapResult, Message, QueueWriteDataSource, ServerMessage, SwapChainId};

use wgc::naga::front::wgsl::ImplementedLanguageExtension;
use wgc::{command::RenderBundleEncoder, id, identity::IdentityManager};
use wgt::{BufferAddress, BufferSize, DynamicOffset, IndexFormat, TextureFormat};

use wgc::id::markers;

use parking_lot::Mutex;

use nsstring::{nsACString, nsCString, nsString};

use std::fmt::Write;
use std::{borrow::Cow, ptr};

use self::render_pass::{FfiRenderPassColorAttachment, RenderPassDepthStencilAttachment};

pub mod render_pass;

// we can't call `from_raw_parts` unconditionally because the caller
// may not even have a valid pointer (e.g. NULL) if the `length` is zero.
fn make_slice<'a, T>(pointer: *const T, length: usize) -> &'a [T] {
    if length == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(pointer, length) }
    }
}

#[repr(C)]
pub struct ConstantEntry {
    key: RawString,
    value: f64,
}

#[repr(C)]
pub struct ProgrammableStageDescriptor {
    module: id::ShaderModuleId,
    entry_point: RawString,
    constants: *const ConstantEntry,
    constants_length: usize,
}

impl ProgrammableStageDescriptor {
    fn to_wgpu(&self) -> wgc::pipeline::ProgrammableStageDescriptor {
        let constants = make_slice(self.constants, self.constants_length)
            .iter()
            .map(|ce| {
                (
                    unsafe { std::ffi::CStr::from_ptr(ce.key) }
                        .to_str()
                        .unwrap()
                        .to_string(),
                    ce.value,
                )
            })
            .collect();
        wgc::pipeline::ProgrammableStageDescriptor {
            module: self.module,
            entry_point: cow_label(&self.entry_point),
            constants,
            zero_initialize_workgroup_memory: true,
        }
    }
}

#[repr(C)]
pub struct ComputePipelineDescriptor<'a> {
    label: Option<&'a nsACString>,
    layout: Option<id::PipelineLayoutId>,
    stage: ProgrammableStageDescriptor,
}

#[repr(C)]
pub struct VertexBufferLayout {
    array_stride: wgt::BufferAddress,
    step_mode: wgt::VertexStepMode,
    attributes: *const wgt::VertexAttribute,
    attributes_length: usize,
}

#[repr(C)]
pub struct VertexState {
    stage: ProgrammableStageDescriptor,
    buffers: *const VertexBufferLayout,
    buffers_length: usize,
}

impl VertexState {
    fn to_wgpu(&self) -> wgc::pipeline::VertexState {
        let buffer_layouts = make_slice(self.buffers, self.buffers_length)
            .iter()
            .map(|vb| wgc::pipeline::VertexBufferLayout {
                array_stride: vb.array_stride,
                step_mode: vb.step_mode,
                attributes: Cow::Borrowed(make_slice(vb.attributes, vb.attributes_length)),
            })
            .collect();
        wgc::pipeline::VertexState {
            stage: self.stage.to_wgpu(),
            buffers: Cow::Owned(buffer_layouts),
        }
    }
}

#[repr(C)]
pub struct ColorTargetState<'a> {
    format: wgt::TextureFormat,
    blend: Option<&'a wgt::BlendState>,
    write_mask: wgt::ColorWrites,
}

#[repr(C)]
pub struct FragmentState<'a> {
    stage: ProgrammableStageDescriptor,
    targets: *const ColorTargetState<'a>,
    targets_length: usize,
}

impl FragmentState<'_> {
    fn to_wgpu(&self) -> wgc::pipeline::FragmentState {
        let color_targets = make_slice(self.targets, self.targets_length)
            .iter()
            .map(|ct| {
                Some(wgt::ColorTargetState {
                    format: ct.format,
                    blend: ct.blend.cloned(),
                    write_mask: ct.write_mask,
                })
            })
            .collect();
        wgc::pipeline::FragmentState {
            stage: self.stage.to_wgpu(),
            targets: Cow::Owned(color_targets),
        }
    }
}

#[repr(C)]
pub struct PrimitiveState<'a> {
    topology: wgt::PrimitiveTopology,
    strip_index_format: Option<&'a wgt::IndexFormat>,
    front_face: wgt::FrontFace,
    cull_mode: Option<&'a wgt::Face>,
    polygon_mode: wgt::PolygonMode,
    unclipped_depth: bool,
}

impl PrimitiveState<'_> {
    fn to_wgpu(&self) -> wgt::PrimitiveState {
        wgt::PrimitiveState {
            topology: self.topology,
            strip_index_format: self.strip_index_format.cloned(),
            front_face: self.front_face.clone(),
            cull_mode: self.cull_mode.cloned(),
            polygon_mode: self.polygon_mode,
            unclipped_depth: self.unclipped_depth,
            conservative: false,
        }
    }
}

#[repr(C)]
pub struct RenderPipelineDescriptor<'a> {
    label: Option<&'a nsACString>,
    layout: Option<id::PipelineLayoutId>,
    vertex: &'a VertexState,
    primitive: PrimitiveState<'a>,
    fragment: Option<&'a FragmentState<'a>>,
    depth_stencil: Option<&'a wgt::DepthStencilState>,
    multisample: wgt::MultisampleState,
}

#[repr(C)]
pub enum RawTextureSampleType {
    Float,
    UnfilterableFloat,
    Uint,
    Sint,
    Depth,
}

#[repr(C)]
pub enum RawBindingType {
    UniformBuffer,
    StorageBuffer,
    ReadonlyStorageBuffer,
    Sampler,
    SampledTexture,
    ReadonlyStorageTexture,
    WriteonlyStorageTexture,
    ReadWriteStorageTexture,
}

#[repr(C)]
pub struct BindGroupLayoutEntry<'a> {
    binding: u32,
    visibility: wgt::ShaderStages,
    ty: RawBindingType,
    has_dynamic_offset: bool,
    min_binding_size: Option<wgt::BufferSize>,
    view_dimension: Option<&'a wgt::TextureViewDimension>,
    texture_sample_type: Option<&'a RawTextureSampleType>,
    multisampled: bool,
    storage_texture_format: Option<&'a wgt::TextureFormat>,
    sampler_filter: bool,
    sampler_compare: bool,
}

#[repr(C)]
pub struct BindGroupLayoutDescriptor<'a> {
    label: Option<&'a nsACString>,
    entries: *const BindGroupLayoutEntry<'a>,
    entries_length: usize,
}

#[repr(C)]
#[derive(Debug)]
pub struct BindGroupEntry {
    binding: u32,
    buffer: Option<id::BufferId>,
    offset: wgt::BufferAddress,
    size: Option<wgt::BufferSize>,
    sampler: Option<id::SamplerId>,
    texture_view: Option<id::TextureViewId>,
}

#[repr(C)]
pub struct BindGroupDescriptor<'a> {
    label: Option<&'a nsACString>,
    layout: id::BindGroupLayoutId,
    entries: *const BindGroupEntry,
    entries_length: usize,
}

#[repr(C)]
pub struct PipelineLayoutDescriptor<'a> {
    label: Option<&'a nsACString>,
    bind_group_layouts: *const id::BindGroupLayoutId,
    bind_group_layouts_length: usize,
}

#[repr(C)]
pub struct SamplerDescriptor<'a> {
    label: Option<&'a nsACString>,
    address_modes: [wgt::AddressMode; 3],
    mag_filter: wgt::FilterMode,
    min_filter: wgt::FilterMode,
    mipmap_filter: wgt::FilterMode,
    lod_min_clamp: f32,
    lod_max_clamp: f32,
    compare: Option<&'a wgt::CompareFunction>,
    max_anisotropy: u16,
}

#[repr(C)]
pub struct TextureViewDescriptor<'a> {
    label: Option<&'a nsACString>,
    format: Option<&'a wgt::TextureFormat>,
    dimension: Option<&'a wgt::TextureViewDimension>,
    aspect: wgt::TextureAspect,
    base_mip_level: u32,
    mip_level_count: Option<&'a u32>,
    base_array_layer: u32,
    array_layer_count: Option<&'a u32>,
}

#[repr(C)]
pub struct RenderBundleEncoderDescriptor<'a> {
    label: Option<&'a nsACString>,
    color_formats: *const wgt::TextureFormat,
    color_formats_length: usize,
    depth_stencil_format: Option<&'a wgt::TextureFormat>,
    depth_read_only: bool,
    stencil_read_only: bool,
    sample_count: u32,
}

#[derive(Debug)]
struct IdentityHub {
    adapters: IdentityManager<markers::Adapter>,
    devices: IdentityManager<markers::Device>,
    queues: IdentityManager<markers::Queue>,
    buffers: IdentityManager<markers::Buffer>,
    command_buffers: IdentityManager<markers::CommandBuffer>,
    render_bundles: IdentityManager<markers::RenderBundle>,
    bind_group_layouts: IdentityManager<markers::BindGroupLayout>,
    pipeline_layouts: IdentityManager<markers::PipelineLayout>,
    bind_groups: IdentityManager<markers::BindGroup>,
    shader_modules: IdentityManager<markers::ShaderModule>,
    compute_pipelines: IdentityManager<markers::ComputePipeline>,
    render_pipelines: IdentityManager<markers::RenderPipeline>,
    textures: IdentityManager<markers::Texture>,
    texture_views: IdentityManager<markers::TextureView>,
    samplers: IdentityManager<markers::Sampler>,
    query_sets: IdentityManager<markers::QuerySet>,
}

impl Default for IdentityHub {
    fn default() -> Self {
        IdentityHub {
            adapters: IdentityManager::new(),
            devices: IdentityManager::new(),
            queues: IdentityManager::new(),
            buffers: IdentityManager::new(),
            command_buffers: IdentityManager::new(),
            render_bundles: IdentityManager::new(),
            bind_group_layouts: IdentityManager::new(),
            pipeline_layouts: IdentityManager::new(),
            bind_groups: IdentityManager::new(),
            shader_modules: IdentityManager::new(),
            compute_pipelines: IdentityManager::new(),
            render_pipelines: IdentityManager::new(),
            textures: IdentityManager::new(),
            texture_views: IdentityManager::new(),
            samplers: IdentityManager::new(),
            query_sets: IdentityManager::new(),
        }
    }
}

impl ImplicitLayout<'_> {
    fn new(identities: &IdentityHub) -> Self {
        ImplicitLayout {
            pipeline: identities.pipeline_layouts.process(),
            bind_groups: Cow::Owned(
                (0..8) // hal::MAX_BIND_GROUPS
                    .map(|_| identities.bind_group_layouts.process())
                    .collect(),
            ),
        }
    }
}

/// Opaque pointer to `mozilla::webgpu::WebGPUChild`.
#[derive(Debug, Clone, Copy)]
#[repr(transparent)]
pub struct WebGPUChildPtr(*mut core::ffi::c_void);

#[derive(Debug)]
pub struct Client {
    owner: WebGPUChildPtr,
    message_queue: Mutex<MessageQueue>,
    identities: Mutex<IdentityHub>,
}

impl Client {
    fn queue_message(&self, message: &Message) {
        let mut message_queue = self.message_queue.lock();
        message_queue.push(self.owner, message);
    }
    fn get_serialized_messages(&self) -> (u32, Vec<u8>) {
        let mut message_queue = self.message_queue.lock();
        message_queue.flush()
    }
}

#[derive(Debug)]
struct MessageQueue {
    on_message_queued: extern "C" fn(WebGPUChildPtr),

    serialized_messages: std::io::Cursor<Vec<u8>>,
    nr_of_queued_messages: u32,
}

impl MessageQueue {
    fn new(on_message_queued: extern "C" fn(WebGPUChildPtr)) -> Self {
        Self {
            on_message_queued,
            serialized_messages: std::io::Cursor::new(Vec::new()),
            nr_of_queued_messages: 0,
        }
    }

    fn push(&mut self, child: WebGPUChildPtr, message: &Message) {
        use bincode::Options;
        let options = bincode::DefaultOptions::new()
            .with_fixint_encoding()
            .allow_trailing_bytes();
        let mut serializer = bincode::Serializer::new(&mut self.serialized_messages, options);

        use serde::Serialize;
        message.serialize(&mut serializer).unwrap();

        self.nr_of_queued_messages = self.nr_of_queued_messages.checked_add(1).unwrap();
        (self.on_message_queued)(child);
    }

    fn flush(&mut self) -> (u32, Vec<u8>) {
        let nr_of_messages = self.nr_of_queued_messages;
        self.nr_of_queued_messages = 0;
        (
            nr_of_messages,
            core::mem::take(&mut self.serialized_messages).into_inner(),
        )
    }
}

#[no_mangle]
pub extern "C" fn wgpu_client_get_queued_messages(
    client: &Client,
    serialized_messages_bb: &mut ByteBuf,
) -> u32 {
    let (nr_of_messages, serialized_messages) = client.get_serialized_messages();
    *serialized_messages_bb = ByteBuf::from_vec(serialized_messages);
    nr_of_messages
}

#[no_mangle]
pub extern "C" fn wgpu_client_new(
    owner: WebGPUChildPtr,
    on_message_queued: extern "C" fn(WebGPUChildPtr),
) -> *mut Client {
    log::info!("Initializing WGPU client");
    let client = Client {
        owner,
        message_queue: Mutex::new(MessageQueue::new(on_message_queued)),
        identities: Mutex::new(IdentityHub::default()),
    };
    Box::into_raw(Box::new(client))
}

/// # Safety
///
/// This function is unsafe because improper use may lead to memory
/// problems. For example, a double-free may occur if the function is called
/// twice on the same raw pointer.
#[no_mangle]
pub unsafe extern "C" fn wgpu_client_delete(client: *mut Client) {
    log::info!("Terminating WGPU client");
    let _client = Box::from_raw(client);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_make_adapter_id(client: &Client) -> id::AdapterId {
    client.identities.lock().adapters.process()
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_adapter_id(client: &Client, id: id::AdapterId) {
    client.identities.lock().adapters.free(id)
}

#[no_mangle]
pub extern "C" fn wgpu_client_fill_default_limits(limits: &mut wgt::Limits) {
    *limits = wgt::Limits::default();
}

/// Writes the single `WGSLLanguageFeature` associated with `index`, appending its identifier to the
/// provided `buffer`. If `index` does not correspond to a valid feature index, then do nothing.
///
/// This function enables an FFI consumer to extract all implemented features in a loop, like so:
///
/// ```rust
/// let mut buffer = nsstring::nsCString::new();
/// for index in 0usize.. {
///     buffer.truncate();
///     wgpu_client_instance_get_wgsl_language_feature(&mut buffer, index);
///     if buffer.is_empty() {
///         break;
///     }
///     // Handle the identifier in `buffer`…
/// }
/// ```
#[no_mangle]
pub extern "C" fn wgpu_client_instance_get_wgsl_language_feature(
    buffer: &mut nsstring::nsCString,
    index: usize,
) {
    match ImplementedLanguageExtension::all().get(index) {
        Some(some) => buffer.write_str(some.to_ident()).unwrap(),
        None => (),
    }
}

#[repr(C)]
pub struct FfiDeviceDescriptor<'a> {
    pub label: Option<&'a nsACString>,
    pub required_features: wgt::FeaturesWebGPU,
    pub required_limits: wgt::Limits,
}

#[no_mangle]
pub extern "C" fn wgpu_client_request_device(
    client: &Client,
    adapter_id: id::AdapterId,
    device_id: id::DeviceId,
    queue_id: id::QueueId,
    desc: &FfiDeviceDescriptor,
) {
    let label = wgpu_string(desc.label);
    let required_features =
        wgt::Features::from_internal_flags(wgt::FeaturesWGPU::empty(), desc.required_features);
    let desc = wgt::DeviceDescriptor {
        label,
        required_features,
        required_limits: desc.required_limits.clone(),
        memory_hints: wgt::MemoryHints::MemoryUsage,
        // The content process is untrusted, so this value is ignored
        // by the GPU process. The GPU process overwrites this with
        // the result of consulting the `WGPU_TRACE` environment
        // variable itself in `wgpu_server_adapter_request_device`.
        trace: wgt::Trace::Off,
    };
    let message = Message::RequestDevice {
        adapter_id,
        device_id,
        queue_id,
        desc,
    };
    client.queue_message(&message);
}

#[repr(C)]
pub struct DeviceQueueId {
    device: id::DeviceId,
    queue: id::QueueId,
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_device_queue_id(client: &Client) -> DeviceQueueId {
    let identities = client.identities.lock();
    let device = identities.devices.process();
    let queue = identities.queues.process();
    DeviceQueueId { device, queue }
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_buffer_id(client: &Client) -> id::BufferId {
    client.identities.lock().buffers.process()
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_buffer_id(client: &Client, id: id::BufferId) {
    client.identities.lock().buffers.free(id)
}

#[rustfmt::skip]
mod drop {
    use super::*;

    #[no_mangle] pub extern "C" fn wgpu_client_destroy_buffer(client: &Client, id: id::BufferId) { client.queue_message(&Message::DestroyBuffer(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_destroy_texture(client: &Client, id: id::TextureId) { client.queue_message(&Message::DestroyTexture(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_destroy_device(client: &Client, id: id::DeviceId) { client.queue_message(&Message::DestroyDevice(id)); }

    #[no_mangle] pub extern "C" fn wgpu_client_drop_adapter(client: &Client, id: id::AdapterId) { client.queue_message(&Message::DropAdapter(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_device(client: &Client, id: id::DeviceId) { client.queue_message(&Message::DropDevice(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_queue(client: &Client, id: id::QueueId) { client.queue_message(&Message::DropQueue(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_buffer(client: &Client, id: id::BufferId) { client.queue_message(&Message::DropBuffer(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_command_buffer(client: &Client, id: id::CommandBufferId) { client.queue_message(&Message::DropCommandBuffer(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_render_bundle(client: &Client, id: id::RenderBundleId) { client.queue_message(&Message::DropRenderBundle(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_bind_group_layout(client: &Client, id: id::BindGroupLayoutId) { client.queue_message(&Message::DropBindGroupLayout(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_pipeline_layout(client: &Client, id: id::PipelineLayoutId) { client.queue_message(&Message::DropPipelineLayout(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_bind_group(client: &Client, id: id::BindGroupId) { client.queue_message(&Message::DropBindGroup(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_shader_module(client: &Client, id: id::ShaderModuleId) { client.queue_message(&Message::DropShaderModule(id)); }

    #[no_mangle] pub extern "C" fn wgpu_client_drop_texture(client: &Client, id: id::TextureId) { client.queue_message(&Message::DropTexture(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_texture_view(client: &Client, id: id::TextureViewId) { client.queue_message(&Message::DropTextureView(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_sampler(client: &Client, id: id::SamplerId) { client.queue_message(&Message::DropSampler(id)); }
    #[no_mangle] pub extern "C" fn wgpu_client_drop_query_set(client: &Client, id: id::QuerySetId) { client.queue_message(&Message::DropQuerySet(id)); }

    #[no_mangle] pub extern "C" fn wgpu_client_drop_command_encoder(client: &Client, id: id::CommandEncoderId) { client.queue_message(&Message::DropCommandEncoder(id)); }
}

#[repr(C)]
pub struct FfiShaderModuleCompilationMessage {
    pub line_number: u64,
    pub line_pos: u64,
    pub utf16_offset: u64,
    pub utf16_length: u64,
    pub message: nsString,
}

#[no_mangle]
pub extern "C" fn wgpu_client_drop_compute_pipeline(
    client: &Client,
    id: id::ComputePipelineId,
    implicit_pipeline_layout_id: Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids_ptr: *const id::BindGroupLayoutId,
    implicit_bind_group_layout_ids_len: usize,
) {
    let implicit_layout =
        implicit_pipeline_layout_id.map(|implicit_pipeline_layout_id| ImplicitLayout {
            pipeline: implicit_pipeline_layout_id,
            bind_groups: Cow::Borrowed(unsafe {
                std::slice::from_raw_parts(
                    implicit_bind_group_layout_ids_ptr,
                    implicit_bind_group_layout_ids_len,
                )
            }),
        });
    client.queue_message(&Message::DropComputePipeline(id, implicit_layout));
}
#[no_mangle]
pub extern "C" fn wgpu_client_drop_render_pipeline(
    client: &Client,
    id: id::RenderPipelineId,
    implicit_pipeline_layout_id: Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids_ptr: *const id::BindGroupLayoutId,
    implicit_bind_group_layout_ids_len: usize,
) {
    let implicit_layout =
        implicit_pipeline_layout_id.map(|implicit_pipeline_layout_id| ImplicitLayout {
            pipeline: implicit_pipeline_layout_id,
            bind_groups: Cow::Borrowed(unsafe {
                std::slice::from_raw_parts(
                    implicit_bind_group_layout_ids_ptr,
                    implicit_bind_group_layout_ids_len,
                )
            }),
        });
    client.queue_message(&Message::DropRenderPipeline(id, implicit_layout));
}

#[no_mangle]
pub extern "C" fn wgpu_client_receive_server_message(
    client: &Client,
    byte_buf: &ByteBuf,
    resolve_request_adapter_promise: extern "C" fn(
        child: WebGPUChildPtr,
        adapter_info: *const AdapterInformation<nsString>,
    ),
    resolve_request_device_promise: extern "C" fn(child: WebGPUChildPtr, error: Option<&nsCString>),
    resolve_pop_error_scope_promise: extern "C" fn(
        child: WebGPUChildPtr,
        ty: u8,
        message: &nsCString,
    ),
    resolve_create_pipeline_promise: extern "C" fn(
        child: WebGPUChildPtr,
        is_render_pipeline: bool,
        is_validation_error: bool,
        error: Option<&nsCString>,
    ),
    resolve_create_shader_module_promise: extern "C" fn(
        child: WebGPUChildPtr,
        messages_ptr: *const FfiShaderModuleCompilationMessage,
        messages_len: usize,
    ),
    resolve_buffer_map_promise: extern "C" fn(
        child: WebGPUChildPtr,
        buffer_id: id::BufferId,
        is_writable: bool,
        offset: u64,
        size: u64,
        error: Option<&nsCString>,
    ),
    resolve_on_submitted_work_done_promise: extern "C" fn(child: WebGPUChildPtr),
) {
    let message: ServerMessage = bincode::deserialize(unsafe { byte_buf.as_slice() }).unwrap();
    match message {
        ServerMessage::RequestAdapterResponse(adapter_id, adapter_information) => {
            if let Some(AdapterInformation {
                backend,
                device_type,
                device,
                driver_info,
                driver,
                features,
                id,
                limits,
                name,
                vendor,
                support_use_external_texture_in_swap_chain,
            }) = adapter_information
            {
                let nss = |s: &str| {
                    let mut ns_string = nsString::new();
                    ns_string.assign_str(s);
                    ns_string
                };
                let adapter_info = AdapterInformation {
                    backend,
                    device_type,
                    device,
                    driver_info: nss(&driver_info),
                    driver: nss(&driver),
                    features,
                    id,
                    limits,
                    name: nss(&name),
                    vendor,
                    support_use_external_texture_in_swap_chain,
                };
                resolve_request_adapter_promise(client.owner, &adapter_info);
            } else {
                resolve_request_adapter_promise(client.owner, core::ptr::null());
                client.identities.lock().adapters.free(adapter_id)
            }
        }
        ServerMessage::RequestDeviceResponse(device_id, queue_id, error) => {
            if let Some(error) = error {
                let error = nsCString::from(error);
                resolve_request_device_promise(client.owner, Some(&error));
                let identities = client.identities.lock();
                identities.devices.free(device_id);
                identities.queues.free(queue_id);
            } else {
                resolve_request_device_promise(client.owner, None);
            }
        }
        ServerMessage::PopErrorScopeResponse(ty, message) => {
            let message = nsCString::from(message.as_ref());
            resolve_pop_error_scope_promise(client.owner, ty, &message);
        }
        ServerMessage::CreateRenderPipelineResponse {
            pipeline_id,
            implicit_ids,
            error,
        } => {
            let is_render_pipeline = true;
            if let Some(error) = error {
                let ns_error = nsCString::from(error.error);
                resolve_create_pipeline_promise(
                    client.owner,
                    is_render_pipeline,
                    error.is_validation_error,
                    Some(&ns_error),
                );

                let identities = client.identities.lock();
                identities.render_pipelines.free(pipeline_id);
                if let Some(implicit_ids) = implicit_ids {
                    identities.pipeline_layouts.free(implicit_ids.pipeline);
                    for bgl_id in implicit_ids.bind_groups.as_ref() {
                        identities.bind_group_layouts.free(*bgl_id);
                    }
                }
            } else {
                resolve_create_pipeline_promise(client.owner, is_render_pipeline, false, None);
            }
        }
        ServerMessage::CreateComputePipelineResponse {
            pipeline_id,
            implicit_ids,
            error,
        } => {
            let is_render_pipeline = false;
            if let Some(error) = error {
                let ns_error = nsCString::from(error.error);
                resolve_create_pipeline_promise(
                    client.owner,
                    is_render_pipeline,
                    error.is_validation_error,
                    Some(&ns_error),
                );

                let identities = client.identities.lock();
                identities.compute_pipelines.free(pipeline_id);
                if let Some(implicit_ids) = implicit_ids {
                    identities.pipeline_layouts.free(implicit_ids.pipeline);
                    for bgl_id in implicit_ids.bind_groups.as_ref() {
                        identities.bind_group_layouts.free(*bgl_id);
                    }
                }
            } else {
                resolve_create_pipeline_promise(client.owner, is_render_pipeline, false, None);
            }
        }
        ServerMessage::CreateShaderModuleResponse(compilation_messages) => {
            let ffi_compilation_messages: Vec<_> = compilation_messages
                .iter()
                .map(|m| FfiShaderModuleCompilationMessage {
                    line_number: m.line_number,
                    line_pos: m.line_pos,
                    utf16_offset: m.utf16_offset,
                    utf16_length: m.utf16_length,
                    message: nsString::from(&m.message),
                })
                .collect();
            resolve_create_shader_module_promise(
                client.owner,
                ffi_compilation_messages.as_ptr(),
                ffi_compilation_messages.len(),
            )
        }
        ServerMessage::BufferMapResponse(buffer_id, buffer_map_result) => {
            match buffer_map_result {
                BufferMapResult::Success {
                    is_writable,
                    offset,
                    size,
                } => {
                    resolve_buffer_map_promise(
                        client.owner,
                        buffer_id,
                        is_writable,
                        offset,
                        size,
                        None,
                    );
                }
                BufferMapResult::Error(error) => {
                    let ns_error = nsCString::from(error.as_ref());
                    resolve_buffer_map_promise(
                        client.owner,
                        buffer_id,
                        false,
                        0,
                        0,
                        Some(&ns_error),
                    );
                }
            };
        }
        ServerMessage::QueueOnSubmittedWorkDoneResponse => {
            resolve_on_submitted_work_done_promise(client.owner);
        }
    }
}

#[no_mangle]
pub extern "C" fn wgpu_client_request_adapter(
    client: &Client,
    adapter_id: id::AdapterId,
    power_preference: wgt::PowerPreference,
    force_fallback_adapter: bool,
) {
    let message = Message::RequestAdapter {
        adapter_id,
        power_preference,
        force_fallback_adapter,
    };
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_pop_error_scope(client: &Client, device_id: id::DeviceId) {
    let message = Message::Device(device_id, DeviceAction::PopErrorScope);
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_shader_module(
    client: &Client,
    device_id: id::DeviceId,
    shader_module_id: id::ShaderModuleId,
    label: Option<&nsACString>,
    code: &nsACString,
) {
    let label = wgpu_string(label);
    let action =
        DeviceAction::CreateShaderModule(shader_module_id, label, Cow::Owned(code.to_string()));
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_on_submitted_work_done(client: &Client, queue_id: id::QueueId) {
    let message = Message::QueueOnSubmittedWorkDone(queue_id);
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_swap_chain(
    client: &Client,
    device_id: id::DeviceId,
    queue_id: id::QueueId,
    width: i32,
    height: i32,
    format: crate::SurfaceFormat,
    buffer_ids: *const id::BufferId,
    buffer_ids_length: usize,
    remote_texture_owner_id: crate::RemoteTextureOwnerId,
    use_external_texture_in_swap_chain: bool,
) {
    let buffer_ids = unsafe { core::slice::from_raw_parts(buffer_ids, buffer_ids_length) };
    let message = Message::CreateSwapChain {
        device_id,
        queue_id,
        width,
        height,
        format,
        buffer_ids: Cow::Borrowed(buffer_ids),
        remote_texture_owner_id,
        use_external_texture_in_swap_chain,
    };
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_swap_chain_present(
    client: &Client,
    texture_id: id::TextureId,
    command_encoder_id: id::CommandEncoderId,
    remote_texture_id: crate::RemoteTextureId,
    remote_texture_owner_id: crate::RemoteTextureOwnerId,
) {
    let message = Message::SwapChainPresent {
        texture_id,
        command_encoder_id,
        remote_texture_id,
        remote_texture_owner_id,
    };
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_swap_chain_drop(
    client: &Client,
    remote_texture_owner_id: crate::RemoteTextureOwnerId,
    txn_type: crate::RemoteTextureTxnType,
    txn_id: crate::RemoteTextureTxnId,
) {
    let message = Message::SwapChainDrop {
        remote_texture_owner_id,
        txn_type,
        txn_id,
    };
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_queue_submit(
    client: &Client,
    device_id: id::DeviceId,
    queue_id: id::QueueId,
    command_buffers: *const id::CommandBufferId,
    command_buffers_length: usize,
    textures: *const id::TextureId,
    textures_length: usize,
) {
    let command_buffers =
        unsafe { core::slice::from_raw_parts(command_buffers, command_buffers_length) };
    let textures = unsafe { core::slice::from_raw_parts(textures, textures_length) };
    let message = Message::QueueSubmit(
        device_id,
        queue_id,
        Cow::Borrowed(command_buffers),
        Cow::Borrowed(textures),
    );
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_buffer_map(
    client: &Client,
    device_id: id::DeviceId,
    buffer_id: id::BufferId,
    mode: u32,
    offset: u64,
    size: u64,
) {
    let message = Message::BufferMap {
        device_id,
        buffer_id,
        mode,
        offset,
        size,
    };
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_buffer_unmap(
    client: &Client,
    device_id: id::DeviceId,
    buffer_id: id::BufferId,
    flush: bool,
) {
    let message = Message::BufferUnmap(device_id, buffer_id, flush);
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_push_error_scope(
    client: &Client,
    device_id: id::DeviceId,
    filter: u8,
) {
    let action = DeviceAction::PushErrorScope(filter);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_buffer(
    client: &Client,
    device_id: id::DeviceId,
    buffer_id: id::BufferId,
    desc: &wgt::BufferDescriptor<Option<&nsACString>>,
    shmem_handle_index: usize,
) {
    let label = wgpu_string(desc.label);
    let desc = desc.map_label(|_| label);
    let action = DeviceAction::CreateBuffer {
        buffer_id,
        desc,
        shmem_handle_index,
    };
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_texture(
    client: &Client,
    device_id: id::DeviceId,
    desc: &wgt::TextureDescriptor<Option<&nsACString>, crate::FfiSlice<TextureFormat>>,
    swap_chain_id: Option<&SwapChainId>,
) -> id::TextureId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().textures.process();

    let view_formats = unsafe { desc.view_formats.as_slice() }.to_vec();

    let action = DeviceAction::CreateTexture(
        id,
        desc.map_label_and_view_formats(|_| label, |_| view_formats),
        swap_chain_id.copied(),
    );
    let message = Message::Device(device_id, action);
    client.queue_message(&message);

    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_texture_id(client: &Client, id: id::TextureId) {
    client.identities.lock().textures.free(id)
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_texture_view(
    client: &Client,
    device_id: id::DeviceId,
    texture_id: id::TextureId,
    desc: &TextureViewDescriptor,
) -> id::TextureViewId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().texture_views.process();

    let wgpu_desc = wgc::resource::TextureViewDescriptor {
        label,
        format: desc.format.cloned(),
        dimension: desc.dimension.cloned(),
        range: wgt::ImageSubresourceRange {
            aspect: desc.aspect,
            base_mip_level: desc.base_mip_level,
            mip_level_count: desc.mip_level_count.map(|ptr| *ptr),
            base_array_layer: desc.base_array_layer,
            array_layer_count: desc.array_layer_count.map(|ptr| *ptr),
        },
        usage: None,
    };

    let action = TextureAction::CreateView(id, wgpu_desc);
    let message = Message::Texture(device_id, texture_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_texture_view_id(client: &Client, id: id::TextureViewId) {
    client.identities.lock().texture_views.free(id)
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_sampler(
    client: &Client,
    device_id: id::DeviceId,
    desc: &SamplerDescriptor,
) -> id::SamplerId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().samplers.process();

    let wgpu_desc = wgc::resource::SamplerDescriptor {
        label,
        address_modes: desc.address_modes,
        mag_filter: desc.mag_filter,
        min_filter: desc.min_filter,
        mipmap_filter: desc.mipmap_filter,
        lod_min_clamp: desc.lod_min_clamp,
        lod_max_clamp: desc.lod_max_clamp,
        compare: desc.compare.cloned(),
        anisotropy_clamp: desc.max_anisotropy,
        border_color: None,
    };
    let action = DeviceAction::CreateSampler(id, wgpu_desc);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_sampler_id(client: &Client, id: id::SamplerId) {
    client.identities.lock().samplers.free(id)
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_encoder_id(client: &Client) -> id::CommandEncoderId {
    client
        .identities
        .lock()
        .command_buffers
        .process()
        .into_command_encoder_id()
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_command_encoder_id(client: &Client, id: id::CommandEncoderId) {
    client
        .identities
        .lock()
        .command_buffers
        .free(id.into_command_buffer_id())
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_command_encoder(
    client: &Client,
    device_id: id::DeviceId,
    desc: &wgt::CommandEncoderDescriptor<Option<&nsACString>>,
) -> id::CommandEncoderId {
    let label = wgpu_string(desc.label);

    let id = client
        .identities
        .lock()
        .command_buffers
        .process()
        .into_command_encoder_id();

    let action = DeviceAction::CreateCommandEncoder(id, desc.map_label(|_| label));
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_device_create_render_bundle_encoder(
    client: &Client,
    device_id: id::DeviceId,
    desc: &RenderBundleEncoderDescriptor,
) -> *mut wgc::command::RenderBundleEncoder {
    let label = wgpu_string(desc.label);

    let color_formats: Vec<_> = make_slice(desc.color_formats, desc.color_formats_length)
        .iter()
        .map(|format| Some(format.clone()))
        .collect();
    let descriptor = wgc::command::RenderBundleEncoderDescriptor {
        label,
        color_formats: Cow::Owned(color_formats),
        depth_stencil: desc
            .depth_stencil_format
            .map(|&format| wgt::RenderBundleDepthStencil {
                format,
                depth_read_only: desc.depth_read_only,
                stencil_read_only: desc.stencil_read_only,
            }),
        sample_count: desc.sample_count,
        multiview: None,
    };
    match wgc::command::RenderBundleEncoder::new(&descriptor, device_id, None) {
        Ok(encoder) => Box::into_raw(Box::new(encoder)),
        Err(e) => {
            let message = format!("Error in `Device::create_render_bundle_encoder`: {}", e);
            let action = DeviceAction::Error {
                message,
                r#type: e.error_type(),
            };
            let message = Message::Device(device_id, action);
            client.queue_message(&message);
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_bundle_encoder_destroy(
    pass: *mut wgc::command::RenderBundleEncoder,
) {
    // The RB encoder is just a boxed Rust struct, it doesn't have any API primitives
    // associated with it right now, but in the future it will.
    let _ = Box::from_raw(pass);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_bundle(
    client: &Client,
    device_id: id::DeviceId,
    encoder: *mut wgc::command::RenderBundleEncoder,
    desc: &wgt::RenderBundleDescriptor<Option<&nsACString>>,
) -> id::RenderBundleId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().render_bundles.process();

    let action =
        DeviceAction::CreateRenderBundle(id, *Box::from_raw(encoder), desc.map_label(|_| label));
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_bundle_error(
    client: &Client,
    device_id: id::DeviceId,
    label: Option<&nsACString>,
) -> id::RenderBundleId {
    let label = wgpu_string(label);

    let id = client.identities.lock().render_bundles.process();

    let action = DeviceAction::CreateRenderBundleError(id, label);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_render_bundle_id(client: &Client, id: id::RenderBundleId) {
    client.identities.lock().render_bundles.free(id)
}

#[repr(C)]
pub struct RawQuerySetDescriptor<'a> {
    label: Option<&'a nsACString>,
    ty: RawQueryType,
    count: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub enum RawQueryType {
    Occlusion,
    Timestamp,
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_query_set(
    client: &Client,
    device_id: id::DeviceId,
    desc: &RawQuerySetDescriptor,
) -> wgc::id::QuerySetId {
    let &RawQuerySetDescriptor { label, ty, count } = desc;

    let label = wgpu_string(label);
    let ty = match ty {
        RawQueryType::Occlusion => wgt::QueryType::Occlusion,
        RawQueryType::Timestamp => wgt::QueryType::Timestamp,
    };

    let desc = wgc::resource::QuerySetDescriptor { label, ty, count };

    let id = client.identities.lock().query_sets.process();

    let action = DeviceAction::CreateQuerySet(id, desc);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);

    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_query_set_id(client: &Client, id: id::QuerySetId) {
    client.identities.lock().query_sets.free(id)
}

#[repr(C)]
pub struct ComputePassDescriptor<'a> {
    pub label: Option<&'a nsACString>,
    pub timestamp_writes: Option<&'a PassTimestampWrites<'a>>,
}

#[repr(C)]
pub struct PassTimestampWrites<'a> {
    pub query_set: id::QuerySetId,
    pub beginning_of_pass_write_index: Option<&'a u32>,
    pub end_of_pass_write_index: Option<&'a u32>,
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_begin_compute_pass(
    desc: &ComputePassDescriptor,
) -> *mut crate::command::RecordedComputePass {
    let &ComputePassDescriptor {
        label,
        timestamp_writes,
    } = desc;

    let label = wgpu_string(label);

    let timestamp_writes = timestamp_writes.map(|tsw| {
        let &PassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        } = tsw;
        let beginning_of_pass_write_index = beginning_of_pass_write_index.cloned();
        let end_of_pass_write_index = end_of_pass_write_index.cloned();
        wgc::command::PassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        }
    });

    let pass = crate::command::RecordedComputePass::new(&wgc::command::ComputePassDescriptor {
        label,
        timestamp_writes,
    });
    Box::into_raw(Box::new(pass))
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_compute_pass_finish(
    client: &Client,
    device_id: id::DeviceId,
    encoder_id: id::CommandEncoderId,
    pass: *mut crate::command::RecordedComputePass,
) {
    let pass = *Box::from_raw(pass);
    let message = Message::ReplayComputePass(device_id, encoder_id, pass);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_compute_pass_destroy(pass: *mut crate::command::RecordedComputePass) {
    let _ = Box::from_raw(pass);
}

#[repr(C)]
pub struct RenderPassDescriptor<'a> {
    pub label: Option<&'a nsACString>,
    pub color_attachments: *const FfiRenderPassColorAttachment,
    pub color_attachments_length: usize,
    pub depth_stencil_attachment: Option<&'a RenderPassDepthStencilAttachment>,
    pub timestamp_writes: Option<&'a PassTimestampWrites<'a>>,
    pub occlusion_query_set: Option<wgc::id::QuerySetId>,
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_begin_render_pass(
    desc: &RenderPassDescriptor,
) -> *mut crate::command::RecordedRenderPass {
    let &RenderPassDescriptor {
        label,
        color_attachments,
        color_attachments_length,
        depth_stencil_attachment,
        timestamp_writes,
        occlusion_query_set,
    } = desc;

    let label = wgpu_string(label).map(|l| l.to_string());

    let timestamp_writes = timestamp_writes.map(|tsw| {
        let &PassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        } = tsw;
        let beginning_of_pass_write_index = beginning_of_pass_write_index.cloned();
        let end_of_pass_write_index = end_of_pass_write_index.cloned();
        wgc::command::PassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        }
    });

    let color_attachments: Vec<_> = make_slice(color_attachments, color_attachments_length)
        .iter()
        .map(|format| Some(format.clone().to_wgpu()))
        .collect();
    let depth_stencil_attachment = depth_stencil_attachment.cloned().map(|dsa| dsa.to_wgpu());
    let pass = crate::command::RecordedRenderPass::new(
        label,
        color_attachments,
        depth_stencil_attachment,
        timestamp_writes,
        occlusion_query_set,
    );
    Box::into_raw(Box::new(pass))
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_pass_finish(
    client: &Client,
    device_id: id::DeviceId,
    encoder_id: id::CommandEncoderId,
    pass: *mut crate::command::RecordedRenderPass,
) {
    let pass = *Box::from_raw(pass);
    let message = Message::ReplayRenderPass(device_id, encoder_id, pass);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_pass_destroy(pass: *mut crate::command::RecordedRenderPass) {
    let _ = Box::from_raw(pass);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_bind_group_layout(
    client: &Client,
    device_id: id::DeviceId,
    desc: &BindGroupLayoutDescriptor,
) -> id::BindGroupLayoutId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().bind_group_layouts.process();

    let mut entries = Vec::with_capacity(desc.entries_length);
    for entry in make_slice(desc.entries, desc.entries_length) {
        entries.push(wgt::BindGroupLayoutEntry {
            binding: entry.binding,
            visibility: entry.visibility,
            count: None,
            ty: match entry.ty {
                RawBindingType::UniformBuffer => wgt::BindingType::Buffer {
                    ty: wgt::BufferBindingType::Uniform,
                    has_dynamic_offset: entry.has_dynamic_offset,
                    min_binding_size: entry.min_binding_size,
                },
                RawBindingType::StorageBuffer => wgt::BindingType::Buffer {
                    ty: wgt::BufferBindingType::Storage { read_only: false },
                    has_dynamic_offset: entry.has_dynamic_offset,
                    min_binding_size: entry.min_binding_size,
                },
                RawBindingType::ReadonlyStorageBuffer => wgt::BindingType::Buffer {
                    ty: wgt::BufferBindingType::Storage { read_only: true },
                    has_dynamic_offset: entry.has_dynamic_offset,
                    min_binding_size: entry.min_binding_size,
                },
                RawBindingType::Sampler => wgt::BindingType::Sampler(if entry.sampler_compare {
                    wgt::SamplerBindingType::Comparison
                } else if entry.sampler_filter {
                    wgt::SamplerBindingType::Filtering
                } else {
                    wgt::SamplerBindingType::NonFiltering
                }),
                RawBindingType::SampledTexture => wgt::BindingType::Texture {
                    //TODO: the spec has a bug here
                    view_dimension: *entry
                        .view_dimension
                        .unwrap_or(&wgt::TextureViewDimension::D2),
                    sample_type: match entry.texture_sample_type {
                        None | Some(RawTextureSampleType::Float) => {
                            wgt::TextureSampleType::Float { filterable: true }
                        }
                        Some(RawTextureSampleType::UnfilterableFloat) => {
                            wgt::TextureSampleType::Float { filterable: false }
                        }
                        Some(RawTextureSampleType::Uint) => wgt::TextureSampleType::Uint,
                        Some(RawTextureSampleType::Sint) => wgt::TextureSampleType::Sint,
                        Some(RawTextureSampleType::Depth) => wgt::TextureSampleType::Depth,
                    },
                    multisampled: entry.multisampled,
                },
                RawBindingType::ReadonlyStorageTexture => wgt::BindingType::StorageTexture {
                    access: wgt::StorageTextureAccess::ReadOnly,
                    view_dimension: *entry.view_dimension.unwrap(),
                    format: *entry.storage_texture_format.unwrap(),
                },
                RawBindingType::WriteonlyStorageTexture => wgt::BindingType::StorageTexture {
                    access: wgt::StorageTextureAccess::WriteOnly,
                    view_dimension: *entry.view_dimension.unwrap(),
                    format: *entry.storage_texture_format.unwrap(),
                },
                RawBindingType::ReadWriteStorageTexture => wgt::BindingType::StorageTexture {
                    access: wgt::StorageTextureAccess::ReadWrite,
                    view_dimension: *entry.view_dimension.unwrap(),
                    format: *entry.storage_texture_format.unwrap(),
                },
            },
        });
    }
    let wgpu_desc = wgc::binding_model::BindGroupLayoutDescriptor {
        label,
        entries: Cow::Owned(entries),
    };

    let action = DeviceAction::CreateBindGroupLayout(id, wgpu_desc);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_bind_group_layout_id(
    client: &Client,
    id: id::BindGroupLayoutId,
) {
    client.identities.lock().bind_group_layouts.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_render_pipeline_get_bind_group_layout(
    client: &Client,
    device_id: id::DeviceId,
    pipeline_id: id::RenderPipelineId,
    index: u32,
) -> id::BindGroupLayoutId {
    let bgl_id = client.identities.lock().bind_group_layouts.process();

    let action = DeviceAction::RenderPipelineGetBindGroupLayout(pipeline_id, index, bgl_id);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);

    bgl_id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_compute_pipeline_get_bind_group_layout(
    client: &Client,
    device_id: id::DeviceId,
    pipeline_id: id::ComputePipelineId,
    index: u32,
) -> id::BindGroupLayoutId {
    let bgl_id = client.identities.lock().bind_group_layouts.process();

    let action = DeviceAction::ComputePipelineGetBindGroupLayout(pipeline_id, index, bgl_id);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);

    bgl_id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_pipeline_layout(
    client: &Client,
    device_id: id::DeviceId,
    desc: &PipelineLayoutDescriptor,
) -> id::PipelineLayoutId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().pipeline_layouts.process();

    let wgpu_desc = wgc::binding_model::PipelineLayoutDescriptor {
        label,
        bind_group_layouts: Cow::Borrowed(make_slice(
            desc.bind_group_layouts,
            desc.bind_group_layouts_length,
        )),
        push_constant_ranges: Cow::Borrowed(&[]),
    };

    let action = DeviceAction::CreatePipelineLayout(id, wgpu_desc);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_pipeline_layout_id(client: &Client, id: id::PipelineLayoutId) {
    client.identities.lock().pipeline_layouts.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_bind_group(
    client: &Client,
    device_id: id::DeviceId,
    desc: &BindGroupDescriptor,
) -> id::BindGroupId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().bind_groups.process();

    let mut entries = Vec::with_capacity(desc.entries_length);
    for entry in make_slice(desc.entries, desc.entries_length) {
        entries.push(wgc::binding_model::BindGroupEntry {
            binding: entry.binding,
            resource: if let Some(id) = entry.buffer {
                wgc::binding_model::BindingResource::Buffer(wgc::binding_model::BufferBinding {
                    buffer: id,
                    offset: entry.offset,
                    size: entry.size,
                })
            } else if let Some(id) = entry.sampler {
                wgc::binding_model::BindingResource::Sampler(id)
            } else if let Some(id) = entry.texture_view {
                wgc::binding_model::BindingResource::TextureView(id)
            } else {
                panic!("Unexpected binding entry {:?}", entry);
            },
        });
    }
    let wgpu_desc = wgc::binding_model::BindGroupDescriptor {
        label,
        layout: desc.layout,
        entries: Cow::Owned(entries),
    };

    let action = DeviceAction::CreateBindGroup(id, wgpu_desc);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_bind_group_id(client: &Client, id: id::BindGroupId) {
    client.identities.lock().bind_groups.free(id)
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_shader_module_id(client: &Client) -> id::ShaderModuleId {
    client.identities.lock().shader_modules.process()
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_shader_module_id(client: &Client, id: id::ShaderModuleId) {
    client.identities.lock().shader_modules.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_compute_pipeline(
    client: &Client,
    device_id: id::DeviceId,
    desc: &ComputePipelineDescriptor,
    implicit_pipeline_layout_id: *mut Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids: *mut Option<id::BindGroupLayoutId>,
    is_async: bool,
) -> id::ComputePipelineId {
    let label = wgpu_string(desc.label);

    let identities = client.identities.lock();
    let id = identities.compute_pipelines.process();

    let wgpu_desc = wgc::pipeline::ComputePipelineDescriptor {
        label,
        layout: desc.layout,
        stage: desc.stage.to_wgpu(),
        cache: None,
    };

    let implicit = match desc.layout {
        Some(_) => None,
        None => {
            let implicit = ImplicitLayout::new(&identities);
            ptr::write(implicit_pipeline_layout_id, Some(implicit.pipeline));
            for (i, bgl_id) in implicit.bind_groups.iter().enumerate() {
                *implicit_bind_group_layout_ids.add(i) = Some(*bgl_id);
            }
            Some(implicit)
        }
    };

    let action = DeviceAction::CreateComputePipeline(id, wgpu_desc, implicit, is_async);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_compute_pipeline_id(client: &Client, id: id::ComputePipelineId) {
    client.identities.lock().compute_pipelines.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_pipeline(
    client: &Client,
    device_id: id::DeviceId,
    desc: &RenderPipelineDescriptor,
    implicit_pipeline_layout_id: *mut Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids: *mut Option<id::BindGroupLayoutId>,
    is_async: bool,
) -> id::RenderPipelineId {
    let label = wgpu_string(desc.label);

    let identities = client.identities.lock();
    let id = identities.render_pipelines.process();

    let wgpu_desc = wgc::pipeline::RenderPipelineDescriptor {
        label,
        layout: desc.layout,
        vertex: desc.vertex.to_wgpu(),
        fragment: desc.fragment.map(FragmentState::to_wgpu),
        primitive: desc.primitive.to_wgpu(),
        depth_stencil: desc.depth_stencil.cloned(),
        multisample: desc.multisample.clone(),
        multiview: None,
        cache: None,
    };

    let implicit = match desc.layout {
        Some(_) => None,
        None => {
            let implicit = ImplicitLayout::new(&identities);
            ptr::write(implicit_pipeline_layout_id, Some(implicit.pipeline));
            for (i, bgl_id) in implicit.bind_groups.iter().enumerate() {
                *implicit_bind_group_layout_ids.add(i) = Some(*bgl_id);
            }
            Some(implicit)
        }
    };

    let action = DeviceAction::CreateRenderPipeline(id, wgpu_desc, implicit, is_async);
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_render_pipeline_id(client: &Client, id: id::RenderPipelineId) {
    client.identities.lock().render_pipelines.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_buffer_to_buffer(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    src: id::BufferId,
    src_offset: wgt::BufferAddress,
    dst: id::BufferId,
    dst_offset: wgt::BufferAddress,
    size: wgt::BufferAddress,
) {
    // In Javascript, `size === undefined` means "copy from src_offset to end of
    // buffer". The `size` argument to this function uses a value of
    // `wgt::BufferAddress::MAX` to encode that case. (Valid copy
    // sizes must be multiples of four, so in the case that the application
    // really asked to copy BufferAddress::MAX bytes,
    // CommandEncoder::CopyBufferToBuffer decrements it by four, which
    // will still fail for mis-alignment.)
    let size = (size != wgt::BufferAddress::MAX).then_some(size);
    let action = CommandEncoderAction::CopyBufferToBuffer {
        src,
        src_offset,
        dst,
        dst_offset,
        size,
    };
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_texture_to_buffer(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    src: wgc::command::TexelCopyTextureInfo,
    dst_buffer: wgc::id::BufferId,
    dst_layout: &TexelCopyBufferLayout,
    size: wgt::Extent3d,
) {
    let action = CommandEncoderAction::CopyTextureToBuffer {
        src,
        dst: wgc::command::TexelCopyBufferInfo {
            buffer: dst_buffer,
            layout: dst_layout.into_wgt(),
        },
        size,
    };
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_buffer_to_texture(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    src_buffer: wgc::id::BufferId,
    src_layout: &TexelCopyBufferLayout,
    dst: wgc::command::TexelCopyTextureInfo,
    size: wgt::Extent3d,
) {
    let action = CommandEncoderAction::CopyBufferToTexture {
        src: wgc::command::TexelCopyBufferInfo {
            buffer: src_buffer,
            layout: src_layout.into_wgt(),
        },
        dst,
        size,
    };
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_texture_to_texture(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    src: wgc::command::TexelCopyTextureInfo,
    dst: wgc::command::TexelCopyTextureInfo,
    size: wgt::Extent3d,
) {
    let action = CommandEncoderAction::CopyTextureToTexture { src, dst, size };
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_clear_buffer(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    dst: wgc::id::BufferId,
    offset: u64,
    size: Option<&u64>,
) {
    let action = CommandEncoderAction::ClearBuffer {
        dst,
        offset,
        size: size.cloned(),
    };
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub extern "C" fn wgpu_command_encoder_push_debug_group(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    marker: &nsACString,
) {
    let string = marker.to_string();
    let action = CommandEncoderAction::PushDebugGroup(string);
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_pop_debug_group(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
) {
    let action = CommandEncoderAction::PopDebugGroup;
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_insert_debug_marker(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    marker: &nsACString,
) {
    let string = marker.to_string();
    let action = CommandEncoderAction::InsertDebugMarker(string);
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_resolve_query_set(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    query_set_id: id::QuerySetId,
    start_query: u32,
    query_count: u32,
    destination: id::BufferId,
    destination_offset: wgt::BufferAddress,
) {
    let action = CommandEncoderAction::ResolveQuerySet {
        query_set_id,
        start_query,
        query_count,
        destination,
        destination_offset,
    };
    let message = Message::CommandEncoder(device_id, command_encoder_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_report_validation_error(
    client: &Client,
    device_id: id::DeviceId,
    message: *const core::ffi::c_char,
) {
    let action = DeviceAction::Error {
        message: core::ffi::CStr::from_ptr(message)
            .to_str()
            .unwrap()
            .to_string(),
        r#type: crate::error::ErrorBufferType::Validation,
    };
    let message = Message::Device(device_id, action);
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_finish(
    client: &Client,
    device_id: id::DeviceId,
    command_encoder_id: id::CommandEncoderId,
    desc: &wgt::CommandBufferDescriptor<Option<&nsACString>>,
) {
    let label = wgpu_string(desc.label);
    let message =
        Message::CommandEncoderFinish(device_id, command_encoder_id, desc.map_label(|_| label));
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_queue_write_buffer_inline(
    client: &Client,
    device_id: id::DeviceId,
    queue_id: id::QueueId,
    dst: id::BufferId,
    offset: wgt::BufferAddress,
    data_buffer_index: usize,
) {
    let data_source = QueueWriteDataSource::DataBuffer(data_buffer_index);

    let action = QueueWriteAction::Buffer { dst, offset };
    let message = Message::QueueWrite {
        device_id,
        queue_id,
        data_source,
        action,
    };
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_queue_write_buffer_via_shmem(
    client: &Client,
    device_id: id::DeviceId,
    queue_id: id::QueueId,
    dst: id::BufferId,
    offset: wgt::BufferAddress,
    shmem_handle_index: usize,
) {
    let data_source = QueueWriteDataSource::Shmem(shmem_handle_index);

    let action = QueueWriteAction::Buffer { dst, offset };
    let message = Message::QueueWrite {
        device_id,
        queue_id,
        data_source,
        action,
    };
    client.queue_message(&message);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_queue_write_texture_via_shmem(
    client: &Client,
    device_id: id::DeviceId,
    queue_id: id::QueueId,
    dst: wgt::TexelCopyTextureInfo<id::TextureId>,
    layout: TexelCopyBufferLayout,
    size: wgt::Extent3d,
    shmem_handle_index: usize,
) {
    let data_source = QueueWriteDataSource::Shmem(shmem_handle_index);

    let layout = layout.into_wgt();
    let action = QueueWriteAction::Texture { dst, layout, size };
    let message = Message::QueueWrite {
        device_id,
        queue_id,
        data_source,
        action,
    };
    client.queue_message(&message);
}

#[repr(C)]
pub struct TextureFormatBlockInfo {
    copy_size: u32,
    width: u32,
    height: u32,
}

/// Obtain the block size and dimensions for a single aspect.
///
/// Populates `info` and returns true on success. Returns false if `format` has
/// multiple aspects and `aspect` is `All`.
#[no_mangle]
pub extern "C" fn wgpu_texture_format_get_block_info(
    format: wgt::TextureFormat,
    aspect: wgt::TextureAspect,
    info: &mut TextureFormatBlockInfo,
) -> bool {
    let (width, height) = format.block_dimensions();
    let (copy_size, ret) = match format.block_copy_size(Some(aspect)) {
        Some(size) => (size, true),
        None => (0, false),
    };
    *info = TextureFormatBlockInfo {
        width,
        height,
        copy_size,
    };
    ret
}

#[no_mangle]
pub extern "C" fn wgpu_client_use_external_texture_in_swapChain(
    format: wgt::TextureFormat,
) -> bool {
    let supported = match format {
        wgt::TextureFormat::Bgra8Unorm => true,
        _ => false,
    };

    supported
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_bundle_set_bind_group(
    bundle: &mut RenderBundleEncoder,
    index: u32,
    bind_group_id: Option<id::BindGroupId>,
    offsets: *const DynamicOffset,
    offset_length: usize,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_set_bind_group(
        bundle,
        index,
        bind_group_id,
        offsets,
        offset_length,
    )
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_set_pipeline(
    bundle: &mut RenderBundleEncoder,
    pipeline_id: id::RenderPipelineId,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_set_pipeline(bundle, pipeline_id)
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_set_vertex_buffer(
    bundle: &mut RenderBundleEncoder,
    slot: u32,
    buffer_id: id::BufferId,
    offset: BufferAddress,
    size: Option<&BufferSize>,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_set_vertex_buffer(
        bundle,
        slot,
        buffer_id,
        offset,
        size.copied(),
    )
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_set_index_buffer(
    encoder: &mut RenderBundleEncoder,
    buffer: id::BufferId,
    index_format: IndexFormat,
    offset: BufferAddress,
    size: Option<&BufferSize>,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_set_index_buffer(
        encoder,
        buffer,
        index_format,
        offset,
        size.copied(),
    )
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_bundle_set_push_constants(
    pass: &mut RenderBundleEncoder,
    stages: wgt::ShaderStages,
    offset: u32,
    size_bytes: u32,
    data: *const u8,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_set_push_constants(
        pass, stages, offset, size_bytes, data,
    )
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_draw(
    bundle: &mut RenderBundleEncoder,
    vertex_count: u32,
    instance_count: u32,
    first_vertex: u32,
    first_instance: u32,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_draw(
        bundle,
        vertex_count,
        instance_count,
        first_vertex,
        first_instance,
    )
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_draw_indexed(
    bundle: &mut RenderBundleEncoder,
    index_count: u32,
    instance_count: u32,
    first_index: u32,
    base_vertex: i32,
    first_instance: u32,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_draw_indexed(
        bundle,
        index_count,
        instance_count,
        first_index,
        base_vertex,
        first_instance,
    )
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_draw_indirect(
    bundle: &mut RenderBundleEncoder,
    buffer_id: id::BufferId,
    offset: BufferAddress,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_draw_indirect(bundle, buffer_id, offset)
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_draw_indexed_indirect(
    bundle: &mut RenderBundleEncoder,
    buffer_id: id::BufferId,
    offset: BufferAddress,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_draw_indexed_indirect(bundle, buffer_id, offset)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_bundle_push_debug_group(
    _bundle: &mut RenderBundleEncoder,
    _label: RawString,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_push_debug_group(_bundle, _label)
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_pop_debug_group(_bundle: &mut RenderBundleEncoder) {
    wgc::command::bundle_ffi::wgpu_render_bundle_pop_debug_group(_bundle)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_bundle_insert_debug_marker(
    _bundle: &mut RenderBundleEncoder,
    _label: RawString,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_insert_debug_marker(_bundle, _label)
}

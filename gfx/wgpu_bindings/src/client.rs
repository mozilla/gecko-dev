/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    cow_label, error::HasErrorBufferType, wgpu_string, AdapterInformation, ByteBuf,
    CommandEncoderAction, DeviceAction, DropAction, ImplicitLayout, QueueWriteAction, RawString,
    TexelCopyBufferLayout, TextureAction,
};

use crate::SwapChainId;

use wgc::{command::RenderBundleEncoder, id, identity::IdentityManager};
use wgt::{BufferAddress, BufferSize, DynamicOffset, IndexFormat, TextureFormat};

use wgc::id::markers;

use parking_lot::Mutex;

use nsstring::{nsACString, nsString};

use std::{borrow::Cow, ptr};

use self::render_pass::RenderPassDepthStencilAttachment;

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

fn make_byte_buf<T: serde::Serialize>(data: &T) -> ByteBuf {
    let vec = bincode::serialize(data).unwrap();
    ByteBuf::from_vec(vec)
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
            constants: Cow::Owned(constants),
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

#[derive(Debug)]
pub struct Client {
    identities: Mutex<IdentityHub>,
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_drop_action(client: &mut Client, byte_buf: &ByteBuf) {
    let mut cursor = std::io::Cursor::new(byte_buf.as_slice());
    let identities = client.identities.lock();
    while let Ok(action) = bincode::deserialize_from(&mut cursor) {
        match action {
            DropAction::Adapter(id) => identities.adapters.free(id),
            DropAction::Device(id) => identities.devices.free(id),
            DropAction::ShaderModule(id) => identities.shader_modules.free(id),
            DropAction::PipelineLayout(id) => identities.pipeline_layouts.free(id),
            DropAction::BindGroupLayout(id) => identities.bind_group_layouts.free(id),
            DropAction::BindGroup(id) => identities.bind_groups.free(id),
            DropAction::CommandBuffer(id) => identities.command_buffers.free(id),
            DropAction::RenderBundle(id) => identities.render_bundles.free(id),
            DropAction::RenderPipeline(id) => identities.render_pipelines.free(id),
            DropAction::ComputePipeline(id) => identities.compute_pipelines.free(id),
            DropAction::Buffer(id) => identities.buffers.free(id),
            DropAction::Texture(id) => identities.textures.free(id),
            DropAction::TextureView(id) => identities.texture_views.free(id),
            DropAction::Sampler(id) => identities.samplers.free(id),
        }
    }
}

#[no_mangle]
pub extern "C" fn wgpu_client_kill_device_id(client: &Client, id: id::DeviceId) {
    client.identities.lock().devices.free(id)
}

#[repr(C)]
#[derive(Debug)]
pub struct Infrastructure {
    pub client: *mut Client,
    pub error: *const u8,
}

#[no_mangle]
pub extern "C" fn wgpu_client_new() -> Infrastructure {
    log::info!("Initializing WGPU client");
    let client = Box::new(Client {
        identities: Mutex::new(IdentityHub::default()),
    });
    Infrastructure {
        client: Box::into_raw(client),
        error: ptr::null(),
    }
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
pub extern "C" fn wgpu_client_fill_default_limits(limits: &mut wgt::Limits) {
    *limits = wgt::Limits::default();
}

#[no_mangle]
pub extern "C" fn wgpu_client_adapter_extract_info(
    byte_buf: &ByteBuf,
    info: &mut AdapterInformation<nsString>,
) {
    let AdapterInformation {
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
    } = bincode::deserialize::<AdapterInformation<String>>(unsafe { byte_buf.as_slice() }).unwrap();

    let nss = |s: &str| {
        let mut ns_string = nsString::new();
        ns_string.assign_str(s);
        ns_string
    };
    *info = AdapterInformation {
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
}

#[repr(C)]
pub struct FfiDeviceDescriptor<'a> {
    pub label: Option<&'a nsACString>,
    pub required_features: wgt::Features,
    pub required_limits: wgt::Limits,
}

#[no_mangle]
pub extern "C" fn wgpu_client_serialize_device_descriptor(
    desc: &FfiDeviceDescriptor,
    bb: &mut ByteBuf,
) {
    let label = wgpu_string(desc.label);
    let desc = wgt::DeviceDescriptor {
        label,
        required_features: desc.required_features.clone(),
        required_limits: desc.required_limits.clone(),
        memory_hints: wgt::MemoryHints::MemoryUsage,
    };
    *bb = make_byte_buf(&desc);
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

#[no_mangle]
pub extern "C" fn wgpu_client_create_texture(
    client: &Client,
    desc: &wgt::TextureDescriptor<Option<&nsACString>, crate::FfiSlice<TextureFormat>>,
    swap_chain_id: Option<&SwapChainId>,
    bb: &mut ByteBuf,
) -> id::TextureId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().textures.process();

    let view_formats = unsafe { desc.view_formats.as_slice() }.to_vec();

    let action = DeviceAction::CreateTexture(
        id,
        desc.map_label_and_view_formats(|_| label, |_| view_formats),
        swap_chain_id.copied(),
    );
    *bb = make_byte_buf(&action);

    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_texture_id(client: &Client, id: id::TextureId) {
    client.identities.lock().textures.free(id)
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_texture_view(
    client: &Client,
    desc: &TextureViewDescriptor,
    bb: &mut ByteBuf,
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
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_texture_view_id(client: &Client, id: id::TextureViewId) {
    client.identities.lock().texture_views.free(id)
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_sampler(
    client: &Client,
    desc: &SamplerDescriptor,
    bb: &mut ByteBuf,
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
    *bb = make_byte_buf(&action);
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
    desc: &wgt::CommandEncoderDescriptor<Option<&nsACString>>,
    bb: &mut ByteBuf,
) -> id::CommandEncoderId {
    let label = wgpu_string(desc.label);

    let id = client
        .identities
        .lock()
        .command_buffers
        .process()
        .into_command_encoder_id();

    let action = DeviceAction::CreateCommandEncoder(id, desc.map_label(|_| label));
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_device_create_render_bundle_encoder(
    device_id: id::DeviceId,
    desc: &RenderBundleEncoderDescriptor,
    bb: &mut ByteBuf,
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
            *bb = make_byte_buf(&action);
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
    encoder: *mut wgc::command::RenderBundleEncoder,
    desc: &wgt::RenderBundleDescriptor<Option<&nsACString>>,
    bb: &mut ByteBuf,
) -> id::RenderBundleId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().render_bundles.process();

    let action =
        DeviceAction::CreateRenderBundle(id, *Box::from_raw(encoder), desc.map_label(|_| label));
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_bundle_error(
    client: &Client,
    label: Option<&nsACString>,
    bb: &mut ByteBuf,
) -> id::RenderBundleId {
    let label = wgpu_string(label);

    let id = client.identities.lock().render_bundles.process();

    let action = DeviceAction::CreateRenderBundleError(id, label);
    *bb = make_byte_buf(&action);
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
    desc: &RawQuerySetDescriptor,
    bb: &mut ByteBuf,
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
    *bb = make_byte_buf(&action);

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
    let timestamp_writes = timestamp_writes.as_ref();

    let pass = crate::command::RecordedComputePass::new(&wgc::command::ComputePassDescriptor {
        label,
        timestamp_writes,
    });
    Box::into_raw(Box::new(pass))
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_compute_pass_finish(
    pass: *mut crate::command::RecordedComputePass,
    output: &mut ByteBuf,
) {
    let command = Box::from_raw(pass);
    *output = make_byte_buf(&command);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_compute_pass_destroy(pass: *mut crate::command::RecordedComputePass) {
    let _ = Box::from_raw(pass);
}

#[repr(C)]
pub struct RenderPassDescriptor<'a> {
    pub label: Option<&'a nsACString>,
    pub color_attachments: *const wgc::command::RenderPassColorAttachment,
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

    let timestamp_writes = timestamp_writes.as_ref();

    let color_attachments: Vec<_> = make_slice(color_attachments, color_attachments_length)
        .iter()
        .map(|format| Some(format.clone()))
        .collect();
    let depth_stencil_attachment = depth_stencil_attachment.cloned().map(|dsa| dsa.to_wgpu());
    let pass = crate::command::RecordedRenderPass::new(&wgc::command::RenderPassDescriptor {
        label,
        color_attachments: Cow::Owned(color_attachments),
        depth_stencil_attachment: depth_stencil_attachment.as_ref(),
        timestamp_writes,
        occlusion_query_set,
    });
    Box::into_raw(Box::new(pass))
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_pass_finish(
    pass: *mut crate::command::RecordedRenderPass,
    output: &mut ByteBuf,
) {
    let command = Box::from_raw(pass);
    *output = make_byte_buf(&command);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_pass_destroy(pass: *mut crate::command::RecordedRenderPass) {
    let _ = Box::from_raw(pass);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_bind_group_layout(
    client: &Client,
    desc: &BindGroupLayoutDescriptor,
    bb: &mut ByteBuf,
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
    *bb = make_byte_buf(&action);
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
    pipeline_id: id::RenderPipelineId,
    index: u32,
    bb: &mut ByteBuf,
) -> id::BindGroupLayoutId {
    let bgl_id = client.identities.lock().bind_group_layouts.process();

    let action = DeviceAction::RenderPipelineGetBindGroupLayout(pipeline_id, index, bgl_id);
    *bb = make_byte_buf(&action);

    bgl_id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_compute_pipeline_get_bind_group_layout(
    client: &Client,
    pipeline_id: id::ComputePipelineId,
    index: u32,
    bb: &mut ByteBuf,
) -> id::BindGroupLayoutId {
    let bgl_id = client.identities.lock().bind_group_layouts.process();

    let action = DeviceAction::ComputePipelineGetBindGroupLayout(pipeline_id, index, bgl_id);
    *bb = make_byte_buf(&action);

    bgl_id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_pipeline_layout(
    client: &Client,
    desc: &PipelineLayoutDescriptor,
    bb: &mut ByteBuf,
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
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_pipeline_layout_id(client: &Client, id: id::PipelineLayoutId) {
    client.identities.lock().pipeline_layouts.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_bind_group(
    client: &Client,
    desc: &BindGroupDescriptor,
    bb: &mut ByteBuf,
) -> id::BindGroupId {
    let label = wgpu_string(desc.label);

    let id = client.identities.lock().bind_groups.process();

    let mut entries = Vec::with_capacity(desc.entries_length);
    for entry in make_slice(desc.entries, desc.entries_length) {
        entries.push(wgc::binding_model::BindGroupEntry {
            binding: entry.binding,
            resource: if let Some(id) = entry.buffer {
                wgc::binding_model::BindingResource::Buffer(wgc::binding_model::BufferBinding {
                    buffer_id: id,
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
    *bb = make_byte_buf(&action);
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
    desc: &ComputePipelineDescriptor,
    bb: &mut ByteBuf,
    implicit_pipeline_layout_id: *mut Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids: *mut Option<id::BindGroupLayoutId>,
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

    let action = DeviceAction::CreateComputePipeline(id, wgpu_desc, implicit);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_compute_pipeline_id(client: &Client, id: id::ComputePipelineId) {
    client.identities.lock().compute_pipelines.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_pipeline(
    client: &Client,
    desc: &RenderPipelineDescriptor,
    bb: &mut ByteBuf,
    implicit_pipeline_layout_id: *mut Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids: *mut Option<id::BindGroupLayoutId>,
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

    let action = DeviceAction::CreateRenderPipeline(id, wgpu_desc, implicit);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_free_render_pipeline_id(client: &Client, id: id::RenderPipelineId) {
    client.identities.lock().render_pipelines.free(id)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_buffer_to_buffer(
    src: id::BufferId,
    src_offset: wgt::BufferAddress,
    dst: id::BufferId,
    dst_offset: wgt::BufferAddress,
    size: wgt::BufferAddress,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyBufferToBuffer {
        src,
        src_offset,
        dst,
        dst_offset,
        size,
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_texture_to_buffer(
    src: wgc::command::TexelCopyTextureInfo,
    dst_buffer: wgc::id::BufferId,
    dst_layout: &TexelCopyBufferLayout,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyTextureToBuffer {
        src,
        dst: wgc::command::TexelCopyBufferInfo {
            buffer: dst_buffer,
            layout: dst_layout.into_wgt(),
        },
        size,
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_buffer_to_texture(
    src_buffer: wgc::id::BufferId,
    src_layout: &TexelCopyBufferLayout,
    dst: wgc::command::TexelCopyTextureInfo,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyBufferToTexture {
        src: wgc::command::TexelCopyBufferInfo {
            buffer: src_buffer,
            layout: src_layout.into_wgt(),
        },
        dst,
        size,
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_texture_to_texture(
    src: wgc::command::TexelCopyTextureInfo,
    dst: wgc::command::TexelCopyTextureInfo,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyTextureToTexture { src, dst, size };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_clear_buffer(
    dst: wgc::id::BufferId,
    offset: u64,
    size: Option<&u64>,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::ClearBuffer {
        dst,
        offset,
        size: size.cloned(),
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub extern "C" fn wgpu_command_encoder_push_debug_group(marker: &nsACString, bb: &mut ByteBuf) {
    let string = marker.to_string();
    let action = CommandEncoderAction::PushDebugGroup(string);
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_pop_debug_group(bb: &mut ByteBuf) {
    let action = CommandEncoderAction::PopDebugGroup;
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_insert_debug_marker(
    marker: &nsACString,
    bb: &mut ByteBuf,
) {
    let string = marker.to_string();
    let action = CommandEncoderAction::InsertDebugMarker(string);
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_resolve_query_set(
    query_set_id: id::QuerySetId,
    start_query: u32,
    query_count: u32,
    destination: id::BufferId,
    destination_offset: wgt::BufferAddress,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::ResolveQuerySet {
        query_set_id,
        start_query,
        query_count,
        destination,
        destination_offset,
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_queue_write_buffer(
    dst: id::BufferId,
    offset: wgt::BufferAddress,
    bb: &mut ByteBuf,
) {
    let action = QueueWriteAction::Buffer { dst, offset };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_queue_write_texture(
    dst: wgt::TexelCopyTextureInfo<id::TextureId>,
    layout: TexelCopyBufferLayout,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let layout = layout.into_wgt();
    let action = QueueWriteAction::Texture { dst, layout, size };
    *bb = make_byte_buf(&action);
}

/// Returns the block size or zero if the format has multiple aspects (for example depth+stencil).
#[no_mangle]
pub extern "C" fn wgpu_texture_format_block_size_single_aspect(format: wgt::TextureFormat) -> u32 {
    format.block_copy_size(None).unwrap_or(0)
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
    size: Option<BufferSize>,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_set_vertex_buffer(
        bundle, slot, buffer_id, offset, size,
    )
}

#[no_mangle]
pub extern "C" fn wgpu_render_bundle_set_index_buffer(
    encoder: &mut RenderBundleEncoder,
    buffer: id::BufferId,
    index_format: IndexFormat,
    offset: BufferAddress,
    size: Option<BufferSize>,
) {
    wgc::command::bundle_ffi::wgpu_render_bundle_set_index_buffer(
        encoder,
        buffer,
        index_format,
        offset,
        size,
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

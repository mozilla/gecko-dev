#![deny(clippy::unimplemented, clippy::unwrap_used, clippy::ok_expect)]
use std::{backtrace::Backtrace, sync::Arc};

use log::debug;

use crate::{
    allocator::{self, AllocatorReport, MemoryBlockReport},
    AllocationError, AllocationSizes, AllocatorDebugSettings, MemoryLocation, Result,
};

fn memory_location_to_metal(location: MemoryLocation) -> metal::MTLResourceOptions {
    match location {
        MemoryLocation::GpuOnly => metal::MTLResourceOptions::StorageModePrivate,
        MemoryLocation::CpuToGpu | MemoryLocation::GpuToCpu | MemoryLocation::Unknown => {
            metal::MTLResourceOptions::StorageModeShared
        }
    }
}

#[derive(Debug)]
pub struct Allocation {
    chunk_id: Option<std::num::NonZeroU64>,
    offset: u64,
    size: u64,
    memory_block_index: usize,
    memory_type_index: usize,
    heap: Arc<metal::Heap>,
    name: Option<Box<str>>,
}

impl Allocation {
    pub fn heap(&self) -> &metal::Heap {
        self.heap.as_ref()
    }

    pub fn make_buffer(&self) -> Option<metal::Buffer> {
        let resource =
            self.heap
                .new_buffer_with_offset(self.size, self.heap.resource_options(), self.offset);
        if let Some(resource) = &resource {
            if let Some(name) = &self.name {
                resource.set_label(name);
            }
        }
        resource
    }

    pub fn make_texture(&self, desc: &metal::TextureDescriptor) -> Option<metal::Texture> {
        let resource = self.heap.new_texture_with_offset(desc, self.offset);
        if let Some(resource) = &resource {
            if let Some(name) = &self.name {
                resource.set_label(name);
            }
        }
        resource
    }

    pub fn make_acceleration_structure(&self) -> Option<metal::AccelerationStructure> {
        let resource = self
            .heap
            .new_acceleration_structure_with_size_offset(self.size, self.offset);
        if let Some(resource) = &resource {
            if let Some(name) = &self.name {
                resource.set_label(name);
            }
        }
        resource
    }

    fn is_null(&self) -> bool {
        self.chunk_id.is_none()
    }
}

#[derive(Clone, Debug)]
pub struct AllocationCreateDesc<'a> {
    /// Name of the allocation, for tracking and debugging purposes
    pub name: &'a str,
    /// Location where the memory allocation should be stored
    pub location: MemoryLocation,
    pub size: u64,
    pub alignment: u64,
}

impl<'a> AllocationCreateDesc<'a> {
    pub fn buffer(
        device: &metal::Device,
        name: &'a str,
        length: u64,
        location: MemoryLocation,
    ) -> Self {
        let size_and_align =
            device.heap_buffer_size_and_align(length, memory_location_to_metal(location));
        Self {
            name,
            location,
            size: size_and_align.size,
            alignment: size_and_align.align,
        }
    }

    pub fn texture(device: &metal::Device, name: &'a str, desc: &metal::TextureDescriptor) -> Self {
        let size_and_align = device.heap_texture_size_and_align(desc);
        Self {
            name,
            location: match desc.storage_mode() {
                metal::MTLStorageMode::Shared
                | metal::MTLStorageMode::Managed
                | metal::MTLStorageMode::Memoryless => MemoryLocation::Unknown,
                metal::MTLStorageMode::Private => MemoryLocation::GpuOnly,
            },
            size: size_and_align.size,
            alignment: size_and_align.align,
        }
    }

    pub fn acceleration_structure_with_size(
        device: &metal::Device,
        name: &'a str,
        size: u64,
        location: MemoryLocation,
    ) -> Self {
        let size_and_align = device.heap_acceleration_structure_size_and_align_with_size(size);
        Self {
            name,
            location,
            size: size_and_align.size,
            alignment: size_and_align.align,
        }
    }
}

pub struct Allocator {
    device: Arc<metal::Device>,
    debug_settings: AllocatorDebugSettings,
    memory_types: Vec<MemoryType>,
    allocation_sizes: AllocationSizes,
}

#[derive(Debug)]
pub struct AllocatorCreateDesc {
    pub device: Arc<metal::Device>,
    pub debug_settings: AllocatorDebugSettings,
    pub allocation_sizes: AllocationSizes,
}

#[derive(Debug)]
pub struct CommittedAllocationStatistics {
    pub num_allocations: usize,
    pub total_size: u64,
}

#[derive(Debug)]
struct MemoryBlock {
    heap: Arc<metal::Heap>,
    size: u64,
    sub_allocator: Box<dyn allocator::SubAllocator>,
}

impl MemoryBlock {
    fn new(
        device: &Arc<metal::Device>,
        size: u64,
        heap_descriptor: &metal::HeapDescriptor,
        dedicated: bool,
        memory_location: MemoryLocation,
    ) -> Result<Self> {
        heap_descriptor.set_size(size);

        let heap = Arc::new(device.new_heap(heap_descriptor));
        heap.set_label(&format!("MemoryBlock {memory_location:?}"));

        let sub_allocator: Box<dyn allocator::SubAllocator> = if dedicated {
            Box::new(allocator::DedicatedBlockAllocator::new(size))
        } else {
            Box::new(allocator::FreeListAllocator::new(size))
        };

        Ok(Self {
            heap,
            size,
            sub_allocator,
        })
    }
}

#[derive(Debug)]
struct MemoryType {
    memory_blocks: Vec<Option<MemoryBlock>>,
    _committed_allocations: CommittedAllocationStatistics,
    memory_location: MemoryLocation,
    heap_properties: metal::HeapDescriptor,
    memory_type_index: usize,
    active_general_blocks: usize,
}

impl MemoryType {
    fn allocate(
        &mut self,
        device: &Arc<metal::Device>,
        desc: &AllocationCreateDesc<'_>,
        backtrace: Arc<Backtrace>,
        allocation_sizes: &AllocationSizes,
    ) -> Result<Allocation> {
        let allocation_type = allocator::AllocationType::Linear;

        let memblock_size = if self.heap_properties.storage_mode() == metal::MTLStorageMode::Private
        {
            allocation_sizes.device_memblock_size
        } else {
            allocation_sizes.host_memblock_size
        };

        let size = desc.size;
        let alignment = desc.alignment;

        // Create a dedicated block for large memory allocations
        if size > memblock_size {
            let mem_block = MemoryBlock::new(
                device,
                size,
                &self.heap_properties,
                true,
                self.memory_location,
            )?;

            let block_index = self.memory_blocks.iter().position(|block| block.is_none());
            let block_index = match block_index {
                Some(i) => {
                    self.memory_blocks[i].replace(mem_block);
                    i
                }
                None => {
                    self.memory_blocks.push(Some(mem_block));
                    self.memory_blocks.len() - 1
                }
            };

            let mem_block = self.memory_blocks[block_index]
                .as_mut()
                .ok_or_else(|| AllocationError::Internal("Memory block must be Some".into()))?;

            let (offset, chunk_id) = mem_block.sub_allocator.allocate(
                size,
                alignment,
                allocation_type,
                1,
                desc.name,
                backtrace,
            )?;

            return Ok(Allocation {
                chunk_id: Some(chunk_id),
                size,
                offset,
                memory_block_index: block_index,
                memory_type_index: self.memory_type_index,
                heap: mem_block.heap.clone(),
                name: Some(desc.name.into()),
            });
        }

        let mut empty_block_index = None;
        for (mem_block_i, mem_block) in self.memory_blocks.iter_mut().enumerate().rev() {
            if let Some(mem_block) = mem_block {
                let allocation = mem_block.sub_allocator.allocate(
                    size,
                    alignment,
                    allocation_type,
                    1,
                    desc.name,
                    backtrace.clone(),
                );

                match allocation {
                    Ok((offset, chunk_id)) => {
                        return Ok(Allocation {
                            chunk_id: Some(chunk_id),
                            offset,
                            size,
                            memory_block_index: mem_block_i,
                            memory_type_index: self.memory_type_index,
                            heap: mem_block.heap.clone(),
                            name: Some(desc.name.into()),
                        });
                    }
                    Err(AllocationError::OutOfMemory) => {} // Block is full, continue search.
                    Err(err) => return Err(err),            // Unhandled error, return.
                }
            } else if empty_block_index.is_none() {
                empty_block_index = Some(mem_block_i);
            }
        }

        let new_memory_block = MemoryBlock::new(
            device,
            memblock_size,
            &self.heap_properties,
            false,
            self.memory_location,
        )?;

        let new_block_index = if let Some(block_index) = empty_block_index {
            self.memory_blocks[block_index] = Some(new_memory_block);
            block_index
        } else {
            self.memory_blocks.push(Some(new_memory_block));
            self.memory_blocks.len() - 1
        };

        self.active_general_blocks += 1;

        let mem_block = self.memory_blocks[new_block_index]
            .as_mut()
            .ok_or_else(|| AllocationError::Internal("Memory block must be Some".into()))?;
        let allocation = mem_block.sub_allocator.allocate(
            size,
            alignment,
            allocation_type,
            1,
            desc.name,
            backtrace,
        );
        let (offset, chunk_id) = match allocation {
            Err(AllocationError::OutOfMemory) => Err(AllocationError::Internal(
                "Allocation that must succeed failed. This is a bug in the allocator.".into(),
            )),
            a => a,
        }?;

        Ok(Allocation {
            chunk_id: Some(chunk_id),
            offset,
            size,
            memory_block_index: new_block_index,
            memory_type_index: self.memory_type_index,
            heap: mem_block.heap.clone(),
            name: Some(desc.name.into()),
        })
    }

    fn free(&mut self, allocation: &Allocation) -> Result<()> {
        let block_idx = allocation.memory_block_index;

        let mem_block = self.memory_blocks[block_idx]
            .as_mut()
            .ok_or_else(|| AllocationError::Internal("Memory block must be Some.".into()))?;

        mem_block.sub_allocator.free(allocation.chunk_id)?;

        if mem_block.sub_allocator.is_empty() {
            if mem_block.sub_allocator.supports_general_allocations() {
                if self.active_general_blocks > 1 {
                    let block = self.memory_blocks[block_idx].take();
                    if block.is_none() {
                        return Err(AllocationError::Internal(
                            "Memory block must be Some.".into(),
                        ));
                    }
                    // Note that `block` will be destroyed on `drop` here

                    self.active_general_blocks -= 1;
                }
            } else {
                let block = self.memory_blocks[block_idx].take();
                if block.is_none() {
                    return Err(AllocationError::Internal(
                        "Memory block must be Some.".into(),
                    ));
                }
                // Note that `block` will be destroyed on `drop` here
            }
        }

        Ok(())
    }
}

impl Allocator {
    pub fn new(desc: &AllocatorCreateDesc) -> Result<Self> {
        let heap_types = [
            (MemoryLocation::GpuOnly, {
                let heap_desc = metal::HeapDescriptor::new();
                heap_desc.set_cpu_cache_mode(metal::MTLCPUCacheMode::DefaultCache);
                heap_desc.set_storage_mode(metal::MTLStorageMode::Private);
                heap_desc.set_heap_type(metal::MTLHeapType::Placement);
                heap_desc
            }),
            (MemoryLocation::CpuToGpu, {
                let heap_desc = metal::HeapDescriptor::new();
                heap_desc.set_cpu_cache_mode(metal::MTLCPUCacheMode::WriteCombined);
                heap_desc.set_storage_mode(metal::MTLStorageMode::Shared);
                heap_desc.set_heap_type(metal::MTLHeapType::Placement);
                heap_desc
            }),
            (MemoryLocation::GpuToCpu, {
                let heap_desc = metal::HeapDescriptor::new();
                heap_desc.set_cpu_cache_mode(metal::MTLCPUCacheMode::DefaultCache);
                heap_desc.set_storage_mode(metal::MTLStorageMode::Shared);
                heap_desc.set_heap_type(metal::MTLHeapType::Placement);
                heap_desc
            }),
        ];

        let memory_types = heap_types
            .into_iter()
            .enumerate()
            .map(|(i, (memory_location, heap_descriptor))| MemoryType {
                memory_blocks: vec![],
                _committed_allocations: CommittedAllocationStatistics {
                    num_allocations: 0,
                    total_size: 0,
                },
                memory_location,
                heap_properties: heap_descriptor,
                memory_type_index: i,
                active_general_blocks: 0,
            })
            .collect();

        Ok(Self {
            device: desc.device.clone(),
            debug_settings: desc.debug_settings,
            memory_types,
            allocation_sizes: desc.allocation_sizes,
        })
    }

    pub fn allocate(&mut self, desc: &AllocationCreateDesc<'_>) -> Result<Allocation> {
        let size = desc.size;
        let alignment = desc.alignment;

        let backtrace = Arc::new(if self.debug_settings.store_stack_traces {
            Backtrace::force_capture()
        } else {
            Backtrace::disabled()
        });

        if self.debug_settings.log_allocations {
            debug!(
                "Allocating `{}` of {} bytes with an alignment of {}.",
                &desc.name, size, alignment
            );
            if self.debug_settings.log_stack_traces {
                let backtrace = Backtrace::force_capture();
                debug!("Allocation stack trace: {}", backtrace);
            }
        }

        if size == 0 || !alignment.is_power_of_two() {
            return Err(AllocationError::InvalidAllocationCreateDesc);
        }

        // Find memory type
        let memory_type = self
            .memory_types
            .iter_mut()
            .find(|memory_type| {
                // Is location compatible
                desc.location == MemoryLocation::Unknown
                    || desc.location == memory_type.memory_location
            })
            .ok_or(AllocationError::NoCompatibleMemoryTypeFound)?;

        memory_type.allocate(&self.device, desc, backtrace, &self.allocation_sizes)
    }

    pub fn free(&mut self, allocation: &Allocation) -> Result<()> {
        if self.debug_settings.log_frees {
            let name = allocation.name.as_deref().unwrap_or("<null>");
            debug!("Freeing `{}`.", name);
            if self.debug_settings.log_stack_traces {
                let backtrace = Backtrace::force_capture();
                debug!("Free stack trace: {}", backtrace);
            }
        }

        if allocation.is_null() {
            return Ok(());
        }
        self.memory_types[allocation.memory_type_index].free(allocation)?;
        Ok(())
    }

    pub fn get_heaps(&self) -> Vec<&metal::HeapRef> {
        // Get all memory blocks
        let mut heaps: Vec<&metal::HeapRef> = Vec::new();
        for memory_type in &self.memory_types {
            for block in memory_type.memory_blocks.iter().flatten() {
                heaps.push(block.heap.as_ref());
            }
        }
        heaps
    }

    pub fn generate_report(&self) -> AllocatorReport {
        let mut allocations = vec![];
        let mut blocks = vec![];
        let mut total_reserved_bytes = 0;

        for memory_type in &self.memory_types {
            for block in memory_type.memory_blocks.iter().flatten() {
                total_reserved_bytes += block.size;
                let first_allocation = allocations.len();
                allocations.extend(block.sub_allocator.report_allocations());
                blocks.push(MemoryBlockReport {
                    size: block.size,
                    allocations: first_allocation..allocations.len(),
                });
            }
        }

        let total_allocated_bytes = allocations.iter().map(|report| report.size).sum();

        AllocatorReport {
            allocations,
            blocks,
            total_allocated_bytes,
            total_reserved_bytes,
        }
    }
}

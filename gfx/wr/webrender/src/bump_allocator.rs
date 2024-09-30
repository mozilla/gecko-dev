/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 use std::{
    alloc::Layout,
    ptr::{self, NonNull}, sync::{Arc, Mutex},
};

use allocator_api2::alloc::{AllocError, Allocator, Global};

const CHUNK_ALIGNMENT: usize = 32;
const DEFAULT_CHUNK_SIZE: usize = 128 * 1024;

/// A simple bump allocator, sub-allocating from fixed size chunks that are provided
/// by a parent allocator.
///
/// If an allocation is larger than the chunk size, a chunk sufficiently large to contain
/// the allocation is added.
pub struct BumpAllocator {
    /// The chunk we are currently allocating from.
    current_chunk: NonNull<Chunk>,
    /// For debugging.
    allocation_count: i32,

    chunk_pool: Arc<ChunkPool>,

    stats: Stats,
}

impl BumpAllocator {
    pub fn new(chunk_pool: Arc<ChunkPool>) -> Self {
        let mut stats = Stats::default();

        let first_chunk = chunk_pool.allocate_chunk(DEFAULT_CHUNK_SIZE).unwrap();
        stats.chunks = 1;
        stats.reserved_bytes += DEFAULT_CHUNK_SIZE;

        BumpAllocator {
            current_chunk: first_chunk,
            chunk_pool,
            allocation_count: 0,
            stats,
        }
    }

    pub fn get_stats(&mut self) -> Stats {
        self.stats.chunk_utilization = self.stats.chunks as f32 - 1.0 + Chunk::utilization(self.current_chunk);
        self.stats
    }

    pub fn reset_stats(&mut self) {
        let chunks = self.stats.chunks;
        let reserved_bytes = self.stats.reserved_bytes;
        self.stats = Stats::default();
        self.stats.chunks = chunks;
        self.stats.reserved_bytes = reserved_bytes;
    }

    pub fn allocate_item(&mut self, layout: Layout) -> Result<NonNull<[u8]>, AllocError> {
        self.stats.allocations += 1;
        self.stats.allocated_bytes += layout.size();

        if let Ok(alloc) = Chunk::allocate_item(self.current_chunk, layout) {
            self.allocation_count += 1;
            return Ok(alloc);
        }

        self.alloc_chunk(layout.size())?;

        match Chunk::allocate_item(self.current_chunk, layout) {
            Ok(alloc) => {
                self.allocation_count += 1;
                    return Ok(alloc);
            }
            Err(_) => {
                return Err(AllocError);
            }
        }
    }

    pub fn deallocate_item(&mut self, ptr: NonNull<u8>, layout: Layout) {
        self.stats.deallocations += 1;

        if Chunk::contains_item(self.current_chunk, ptr) {
            unsafe { Chunk::deallocate_item(self.current_chunk, ptr, layout); }
        }

        self.allocation_count -= 1;
        debug_assert!(self.allocation_count >= 0);
    }

    pub unsafe fn grow_item(&mut self, ptr: NonNull<u8>, old_layout: Layout, new_layout: Layout) -> Result<NonNull<[u8]>, AllocError> {
        debug_assert!(
            new_layout.size() >= old_layout.size(),
            "`new_layout.size()` must be greater than or equal to `old_layout.size()`"
        );

        self.stats.reallocations += 1;

        if Chunk::contains_item(self.current_chunk, ptr) {
            if let Ok(alloc) = Chunk::grow_item(self.current_chunk, ptr, old_layout, new_layout) {
                self.stats.in_place_reallocations += 1;
                return Ok(alloc);
            }
        }

        let new_alloc = if let Ok(alloc) = Chunk::allocate_item(self.current_chunk, new_layout) {
            alloc
        } else {
            self.alloc_chunk(new_layout.size())?;
            Chunk::allocate_item(self.current_chunk, new_layout).map_err(|_| AllocError)?
        };

        self.stats.reallocated_bytes += old_layout.size();

        unsafe {
            ptr::copy_nonoverlapping(ptr.as_ptr(), new_alloc.as_ptr().cast(), old_layout.size());
        }

        Ok(new_alloc)
    }

    pub unsafe fn shrink_item(&mut self, ptr: NonNull<u8>, old_layout: Layout, new_layout: Layout) -> Result<NonNull<[u8]>, AllocError> {
        debug_assert!(
            new_layout.size() <= old_layout.size(),
            "`new_layout.size()` must be smaller than or equal to `old_layout.size()`"
        );

        if Chunk::contains_item(self.current_chunk, ptr) {
            return unsafe { Ok(Chunk::shrink_item(self.current_chunk, ptr, old_layout, new_layout)) };
        }

        // Can't actually shrink, so return the full range of the previous allocation.
        Ok(NonNull::slice_from_raw_parts(ptr, old_layout.size()))
    }

    fn alloc_chunk(&mut self, item_size: usize) -> Result<(), AllocError> {
        let chunk_size = DEFAULT_CHUNK_SIZE.max(align(item_size, CHUNK_ALIGNMENT) + CHUNK_ALIGNMENT);
        self.stats.reserved_bytes += chunk_size;

        let chunk = self.chunk_pool.allocate_chunk(chunk_size)?;

        unsafe {
            (*chunk.as_ptr()).previous = Some(self.current_chunk);
        }
        self.current_chunk = chunk;

        self.stats.chunks += 1;

        Ok(())
    }
}

impl Drop for BumpAllocator {
    fn drop(&mut self) {
        assert!(self.allocation_count == 0);

        unsafe {
            self.chunk_pool.recycle_chunks(self.current_chunk);
        }
    }
}

/// A Contiguous buffer of memory holding multiple sub-allocaions.
pub struct Chunk {
    previous: Option<NonNull<Chunk>>,
    /// Offset of the next allocation
    cursor: *mut u8,
    /// Points to the first byte after the chunk's buffer.
    chunk_end: *mut u8,
    /// Size of the chunk
    size: usize,
}

impl Chunk {
    pub fn allocate_item(this: NonNull<Chunk>, layout: Layout) -> Result<NonNull<[u8]>, ()> {
        // Common wisdom would be to always bump address downward (https://fitzgeraldnick.com/2019/11/01/always-bump-downwards.html).
        // However, bump allocation does not show up in profiles with the current workloads
        // so we can keep things simple for now.
        debug_assert!(CHUNK_ALIGNMENT % layout.align() == 0);
        debug_assert!(layout.align() > 0);
        debug_assert!(layout.align().is_power_of_two());

        let size = align(layout.size(), CHUNK_ALIGNMENT);

        unsafe {
            let cursor = (*this.as_ptr()).cursor;
            let end = (*this.as_ptr()).chunk_end;
            let available_size = end.offset_from(cursor);

            if size as isize > available_size {
                return Err(());
            }

            let next = cursor.add(size);

            (*this.as_ptr()).cursor = next;

            let cursor = NonNull::new(cursor).unwrap();
            let suballocation: NonNull<[u8]> = NonNull::slice_from_raw_parts(cursor, size);

            Ok(suballocation)
        }
    }

    pub unsafe fn deallocate_item(this: NonNull<Chunk>, item: NonNull<u8>, layout: Layout) {
        debug_assert!(Chunk::contains_item(this, item));

        unsafe {
            let size = align(layout.size(), CHUNK_ALIGNMENT);
            let item_end = item.as_ptr().add(size);

            // If the item is the last allocation, then move the cursor back
            // to reuse its memory.
            if item_end == (*this.as_ptr()).cursor {
                (*this.as_ptr()).cursor = item.as_ptr();
            }

            // Otherwise, deallocation is a no-op
        }
    }

    pub unsafe fn grow_item(this: NonNull<Chunk>, item: NonNull<u8>, old_layout: Layout, new_layout: Layout) -> Result<NonNull<[u8]>, ()> {
        debug_assert!(Chunk::contains_item(this, item));

        let old_size = align(old_layout.size(), CHUNK_ALIGNMENT);
        let new_size = align(new_layout.size(), CHUNK_ALIGNMENT);
        let old_item_end = item.as_ptr().add(old_size);

        if old_item_end != (*this.as_ptr()).cursor {
            return Err(());
        }

        // The item is the last allocation. we can attempt to just move
        // the cursor if the new size fits.

        let chunk_end = (*this.as_ptr()).chunk_end;
        let available_size = chunk_end.offset_from(item.as_ptr());

        if new_size as isize > available_size {
            // Does not fit.
            return Err(());
        }

        let new_item_end = item.as_ptr().add(new_size);
        (*this.as_ptr()).cursor = new_item_end;

        Ok(NonNull::slice_from_raw_parts(item, new_size))
    }

    pub unsafe fn shrink_item(this: NonNull<Chunk>, item: NonNull<u8>, old_layout: Layout, new_layout: Layout) -> NonNull<[u8]> {
        debug_assert!(Chunk::contains_item(this, item));

        let old_size = align(old_layout.size(), CHUNK_ALIGNMENT);
        let new_size = align(new_layout.size(), CHUNK_ALIGNMENT);
        let old_item_end = item.as_ptr().add(old_size);

        // The item is the last allocation. we can attempt to just move
        // the cursor if the new size fits.

        if old_item_end == (*this.as_ptr()).cursor {
            let new_item_end = item.as_ptr().add(new_size);
            (*this.as_ptr()).cursor = new_item_end;
        }

        NonNull::slice_from_raw_parts(item, new_size)
    }

    pub fn contains_item(this: NonNull<Chunk>, item: NonNull<u8>) -> bool {
        unsafe {
            let start: *mut u8 = this.cast::<u8>().as_ptr().add(CHUNK_ALIGNMENT);
            let end: *mut u8 = (*this.as_ptr()).chunk_end;
            let item = item.as_ptr();

            start <= item && item < end
        }
    }

    fn available_size(this: NonNull<Chunk>) -> usize {
        unsafe {
            let this = this.as_ptr();
            (*this).chunk_end.offset_from((*this).cursor) as usize
        }
    }

    fn utilization(this: NonNull<Chunk>) -> f32 {
        let size = unsafe { (*this.as_ptr()).size } as f32;
        (size - Chunk::available_size(this) as f32) / size
    }
}

fn align(val: usize, alignment: usize) -> usize {
    let rem = val % alignment;
    if rem == 0 {
        return val;
    }

    val.checked_add(alignment).unwrap() - rem
}

#[derive(Copy, Clone, Debug, Default)]
pub struct Stats {
    pub chunks: u32,
    pub chunk_utilization: f32,
    pub allocations: u32,
    pub deallocations: u32,
    pub reallocations: u32,
    pub in_place_reallocations: u32,

    pub reallocated_bytes: usize,
    pub allocated_bytes: usize,
    pub reserved_bytes: usize,
}

/// A simple pool for allocating and recycling memory chunks of a fixed size,
/// protected by a mutex.
///
/// Chunks in the pool are stored as a linked list using a pointer to the next
/// element at the beginning of the chunk.
pub struct ChunkPool {
    inner: Mutex<ChunkPoolInner>,
}

struct ChunkPoolInner {
    first: Option<NonNull<RecycledChunk>>,
    count: i32,
}

/// Header at the beginning of recycled memory chunk.
struct RecycledChunk {
    next: Option<NonNull<RecycledChunk>>,
}

impl ChunkPool {
    pub fn new() -> Self {
        ChunkPool {
            inner: Mutex::new(ChunkPoolInner {
                first: None,
                count: 0,
            }),
        }
    }

    /// Pop a chunk from the pool or allocate a new one.
    ///
    /// If the requested size is not equal to the default chunk size,
    /// a new chunk is allocated.
    pub fn allocate_chunk(&self, size: usize) -> Result<NonNull<Chunk>, AllocError> {
        let chunk: Option<NonNull<RecycledChunk>> = if size == DEFAULT_CHUNK_SIZE {
            // Try to reuse a chunk.
            let mut inner = self.inner.lock().unwrap();
            let mut chunk = inner.first.take();
            inner.first = chunk.as_mut().and_then(|chunk| unsafe { chunk.as_mut().next.take() });

            if chunk.is_some() {
                inner.count -= 1;
                debug_assert!(inner.count >= 0);
            }

            chunk
        } else {
            // Always allocate a new chunk if it is not the standard size.
            None
        };

        let chunk: NonNull<Chunk> = match chunk {
            Some(chunk) => chunk.cast(),
            None => {
                // Allocate a new one.
                let layout = match Layout::from_size_align(size, CHUNK_ALIGNMENT) {
                    Ok(layout) => layout,
                    Err(_) => {
                        return Err(AllocError);
                    }
                };

                let alloc = Global.allocate(layout)?;

                alloc.cast()
            }
        };

        let chunk_start: *mut u8 = chunk.cast().as_ptr();

        unsafe {
            let chunk_end = chunk_start.add(size);
            let cursor = chunk_start.add(CHUNK_ALIGNMENT);
            ptr::write(
                chunk.as_ptr(),
                Chunk {
                    previous: None,
                    chunk_end,
                    cursor,
                    size,
                },
            );
        }

        Ok(chunk)
    }

    /// Put the provided list of chunks into the pool.
    ///
    /// Chunks with size different from the default chunk size are deallocated
    /// immediately.
    ///
    /// # Safety
    ///
    /// Ownership of the provided chunks is transfered to the pool, nothing
    /// else can access them after this function runs.
    unsafe fn recycle_chunks(&self, chunk: NonNull<Chunk>) {
        let mut inner = self.inner.lock().unwrap();
        let mut iter = Some(chunk);
        // Go through the provided linked list of chunks, and insert each
        // of them at the beginning of our linked list of recycled chunks.
        while let Some(mut chunk) = iter {
            // Advance the iterator.
            iter = unsafe { chunk.as_mut().previous.take() };

            unsafe {
                // Don't recycle chunks with a non-standard size.
                let size = chunk.as_ref().size;
                if size != DEFAULT_CHUNK_SIZE {
                    let layout = Layout::from_size_align(size, CHUNK_ALIGNMENT).unwrap();
                    Global.deallocate(chunk.cast(), layout);
                    continue;
                }
            }

            // Turn the chunk into a recycled chunk.
            let recycled: NonNull<RecycledChunk> = chunk.cast();

            // Insert into the recycled list.
            unsafe {
                ptr::write(recycled.as_ptr(), RecycledChunk {
                    next: inner.first,
                });
            }
            inner.first = Some(recycled);

            inner.count += 1;
        }
    }

    /// Deallocate chunks until the pool contains at most `target` items, or
    /// `count` chunks have been deallocated.
    ///
    /// Returns `true` if the target number of chunks in the pool was reached,
    /// `false` if this method stopped before reaching the target.
    ///
    /// Purging chunks can be expensive so it is preferable to perform this
    /// operation outside of the critical path. Specifying a lower `count`
    /// allows the caller to split the work and spread it over time.
    #[inline(never)]
    pub fn purge_chunks(&self, target: u32, mut count: u32) -> bool {
        let mut inner = self.inner.lock().unwrap();
        assert!(inner.count >= 0);

        while inner.count as u32 > target {
            unsafe {
                // First can't be None because inner.count > 0.
                let chunk = inner.first.unwrap();

                // Pop chunk off the list.
                inner.first = chunk.as_ref().next;

                // Deallocate chunk.
                let layout = Layout::from_size_align(
                    DEFAULT_CHUNK_SIZE,
                    CHUNK_ALIGNMENT
                ).unwrap();
                Global.deallocate(chunk.cast(), layout);
            }

            inner.count -= 1;
            count -= 1;

            if count == 0 {
                return false;
            }
        }

        return true;
    }

    /// Deallocate all of the chunks.
    pub fn purge_all_chunks(&self) {
        self.purge_chunks(0, u32::MAX);
    }
}

impl Drop for ChunkPool {
    fn drop(&mut self) {
        self.purge_all_chunks();
    }
}

unsafe impl Send for ChunkPoolInner {}

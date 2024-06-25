/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_ETAGERE_H
#define MOZILLA_GFX_ETAGERE_H

#include <stddef.h>

namespace Etagere {

constexpr static const uint32_t FLAGS_VERTICAL_SHELVES = 1;

/// A shelf-packing dynamic texture atlas allocator tracking each allocation
/// individually and with support for coalescing empty shelves.
struct AtlasAllocator;

/// Options to tweak the behavior of the atlas allocator.
struct AllocatorOptions {
  int32_t width_alignment;
  int32_t height_alignment;
  int32_t num_columns;
  uint32_t flags;
};

/// 1 means OK, 0 means error.
using Status = uint32_t;

struct Rectangle {
  int32_t min_x;
  int32_t min_y;
  int32_t max_x;
  int32_t max_y;
};

using AllocationId = uint32_t;

constexpr static const uint32_t INVALID_ALLOCATION_ID = ~0U;

struct Allocation {
  Rectangle rectangle;
  AllocationId id;
};

extern "C" {

AtlasAllocator* etagere_atlas_allocator_new(int32_t width, int32_t height);

AtlasAllocator* etagere_atlas_allocator_with_options(
    int32_t width, int32_t height, const AllocatorOptions* options);

void etagere_atlas_allocator_delete(AtlasAllocator* allocator);

Status etagere_atlas_allocator_allocate(AtlasAllocator* allocator,
                                        int32_t width, int32_t height,
                                        Allocation* allocation);

void etagere_atlas_allocator_deallocate(AtlasAllocator* allocator,
                                        AllocationId id);

void etagere_atlas_allocator_clear(AtlasAllocator* allocator);

int32_t etagere_atlas_allocator_allocated_space(
    const AtlasAllocator* allocator);

int32_t etagere_atlas_allocator_free_space(const AtlasAllocator* allocator);

Rectangle etagere_atlas_allocator_get(const AtlasAllocator* allocator,
                                      AllocationId id);

Status etagere_atlas_allocator_dump_svg(const AtlasAllocator* allocator,
                                        const char* file_name);

}  // extern "C"

}  // namespace Etagere

#endif  // MOZILLA_GFX_ETAGERE_H

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Heap_h
#define gc_Heap_h

#include "mozilla/Attributes.h"
#include "mozilla/PodOperations.h"

#include <stddef.h>
#include <stdint.h>

#include "jspubtd.h"
#include "jstypes.h"
#include "jsutil.h"

#include "ds/BitArray.h"
#include "js/HeapAPI.h"

struct JSCompartment;

struct JSRuntime;

namespace JS {
namespace shadow {
class Runtime;
}
}

namespace js {

class FreeOp;

namespace gc {

struct Arena;
struct ArenaHeader;
struct Chunk;

/*
 * This flag allows an allocation site to request a specific heap based upon the
 * estimated lifetime or lifetime requirements of objects allocated from that
 * site.
 */
enum InitialHeap {
    DefaultHeap,
    TenuredHeap
};

/* The GC allocation kinds. */
enum AllocKind {
    FINALIZE_OBJECT0,
    FINALIZE_OBJECT0_BACKGROUND,
    FINALIZE_OBJECT2,
    FINALIZE_OBJECT2_BACKGROUND,
    FINALIZE_OBJECT4,
    FINALIZE_OBJECT4_BACKGROUND,
    FINALIZE_OBJECT8,
    FINALIZE_OBJECT8_BACKGROUND,
    FINALIZE_OBJECT12,
    FINALIZE_OBJECT12_BACKGROUND,
    FINALIZE_OBJECT16,
    FINALIZE_OBJECT16_BACKGROUND,
    FINALIZE_OBJECT_LAST = FINALIZE_OBJECT16_BACKGROUND,
    FINALIZE_SCRIPT,
    FINALIZE_LAZY_SCRIPT,
    FINALIZE_SHAPE,
    FINALIZE_BASE_SHAPE,
    FINALIZE_TYPE_OBJECT,
    FINALIZE_SHORT_STRING,
    FINALIZE_STRING,
    FINALIZE_EXTERNAL_STRING,
    FINALIZE_JITCODE,
    FINALIZE_LAST = FINALIZE_JITCODE
};

static const unsigned FINALIZE_LIMIT = FINALIZE_LAST + 1;
static const unsigned FINALIZE_OBJECT_LIMIT = FINALIZE_OBJECT_LAST + 1;

/*
 * This must be an upper bound, but we do not need the least upper bound, so
 * we just exclude non-background objects.
 */
static const size_t MAX_BACKGROUND_FINALIZE_KINDS = FINALIZE_LIMIT - FINALIZE_OBJECT_LIMIT / 2;

/*
 * A GC cell is the base class for all GC things.
 */
struct Cell
{
  public:
    inline ArenaHeader *arenaHeader() const;
    inline AllocKind tenuredGetAllocKind() const;
    MOZ_ALWAYS_INLINE bool isMarked(uint32_t color = BLACK) const;
    MOZ_ALWAYS_INLINE bool markIfUnmarked(uint32_t color = BLACK) const;
    MOZ_ALWAYS_INLINE void unmark(uint32_t color) const;

    inline JSRuntime *runtimeFromMainThread() const;
    inline JS::shadow::Runtime *shadowRuntimeFromMainThread() const;
    inline JS::Zone *tenuredZone() const;
    inline JS::Zone *tenuredZoneFromAnyThread() const;
    inline bool tenuredIsInsideZone(JS::Zone *zone) const;

    // Note: Unrestricted access to the runtime of a GC thing from an arbitrary
    // thread can easily lead to races. Use this method very carefully.
    inline JSRuntime *runtimeFromAnyThread() const;
    inline JS::shadow::Runtime *shadowRuntimeFromAnyThread() const;

#ifdef DEBUG
    inline bool isAligned() const;
    inline bool isTenured() const;
#endif

  protected:
    inline uintptr_t address() const;
    inline Chunk *chunk() const;
};

/*
 * The mark bitmap has one bit per each GC cell. For multi-cell GC things this
 * wastes space but allows to avoid expensive devisions by thing's size when
 * accessing the bitmap. In addition this allows to use some bits for colored
 * marking during the cycle GC.
 */
const size_t ArenaCellCount = size_t(1) << (ArenaShift - CellShift);
const size_t ArenaBitmapBits = ArenaCellCount;
const size_t ArenaBitmapBytes = ArenaBitmapBits / 8;
const size_t ArenaBitmapWords = ArenaBitmapBits / JS_BITS_PER_WORD;

/*
 * A FreeSpan represents a contiguous sequence of free cells in an Arena.
 * |first| is the address of the first free cell in the span. |last| is the
 * address of the last free cell in the span. This last cell holds a FreeSpan
 * data structure for the next span unless this is the last span on the list
 * of spans in the arena. For this last span |last| points to the last byte of
 * the last thing in the arena and no linkage is stored there, so
 * |last| == arenaStart + ArenaSize - 1. If the space at the arena end is
 * fully used this last span is empty and |first| == |last + 1|.
 *
 * Thus |first| < |last| implies that we have either the last span with at least
 * one element or that the span is not the last and contains at least 2
 * elements. In both cases to allocate a thing from this span we need simply
 * to increment |first| by the allocation size.
 *
 * |first| == |last| implies that we have a one element span that records the
 * next span. So to allocate from it we need to update the span list head
 * with a copy of the span stored at |last| address so the following
 * allocations will use that span.
 *
 * |first| > |last| implies that we have an empty last span and the arena is
 * fully used.
 *
 * Also only for the last span (|last| & 1)! = 0 as all allocation sizes are
 * multiples of CellSize.
 */
struct FreeSpan
{
    uintptr_t   first;
    uintptr_t   last;

  public:
    FreeSpan() {}

    FreeSpan(uintptr_t first, uintptr_t last)
      : first(first), last(last) {
        checkSpan();
    }

    /*
     * To minimize the size of the arena header the first span is encoded
     * there as offsets from the arena start.
     */
    static size_t encodeOffsets(size_t firstOffset, size_t lastOffset) {
        static_assert(ArenaShift < 16, "Check that we can pack offsets into uint16_t.");
        JS_ASSERT(firstOffset <= ArenaSize);
        JS_ASSERT(lastOffset < ArenaSize);
        JS_ASSERT(firstOffset <= ((lastOffset + 1) & ~size_t(1)));
        return firstOffset | (lastOffset << 16);
    }

    /*
     * Encoded offsets for a full arena when its first span is the last one
     * and empty.
     */
    static const size_t FullArenaOffsets = ArenaSize | ((ArenaSize - 1) << 16);

    static FreeSpan decodeOffsets(uintptr_t arenaAddr, size_t offsets) {
        JS_ASSERT(!(arenaAddr & ArenaMask));

        size_t firstOffset = offsets & 0xFFFF;
        size_t lastOffset = offsets >> 16;
        JS_ASSERT(firstOffset <= ArenaSize);
        JS_ASSERT(lastOffset < ArenaSize);

        /*
         * We must not use | when calculating first as firstOffset is
         * ArenaMask + 1 for the empty span.
         */
        return FreeSpan(arenaAddr + firstOffset, arenaAddr | lastOffset);
    }

    void initAsEmpty(uintptr_t arenaAddr = 0) {
        JS_ASSERT(!(arenaAddr & ArenaMask));
        first = arenaAddr + ArenaSize;
        last = arenaAddr | (ArenaSize  - 1);
        JS_ASSERT(isEmpty());
    }

    bool isEmpty() const {
        checkSpan();
        return first > last;
    }

    bool hasNext() const {
        checkSpan();
        return !(last & uintptr_t(1));
    }

    const FreeSpan *nextSpan() const {
        JS_ASSERT(hasNext());
        return reinterpret_cast<FreeSpan *>(last);
    }

    FreeSpan *nextSpanUnchecked(size_t thingSize) const {
#ifdef DEBUG
        uintptr_t lastOffset = last & ArenaMask;
        JS_ASSERT(!(lastOffset & 1));
        JS_ASSERT((ArenaSize - lastOffset) % thingSize == 0);
#endif
        return reinterpret_cast<FreeSpan *>(last);
    }

    uintptr_t arenaAddressUnchecked() const {
        return last & ~ArenaMask;
    }

    uintptr_t arenaAddress() const {
        checkSpan();
        return arenaAddressUnchecked();
    }

    ArenaHeader *arenaHeader() const {
        return reinterpret_cast<ArenaHeader *>(arenaAddress());
    }

    bool isSameNonEmptySpan(const FreeSpan *another) const {
        JS_ASSERT(!isEmpty());
        JS_ASSERT(!another->isEmpty());
        return first == another->first && last == another->last;
    }

    bool isWithinArena(uintptr_t arenaAddr) const {
        JS_ASSERT(!(arenaAddr & ArenaMask));

        /* Return true for the last empty span as well. */
        return arenaAddress() == arenaAddr;
    }

    size_t encodeAsOffsets() const {
        /*
         * We must use first - arenaAddress(), not first & ArenaMask as
         * first == ArenaMask + 1 for an empty span.
         */
        uintptr_t arenaAddr = arenaAddress();
        return encodeOffsets(first - arenaAddr, last & ArenaMask);
    }

    /* See comments before FreeSpan for details. */
    MOZ_ALWAYS_INLINE void *allocate(size_t thingSize) {
        JS_ASSERT(thingSize % CellSize == 0);
        checkSpan();
        uintptr_t thing = first;
        if (thing < last) {
            /* Bump-allocate from the current span. */
            first = thing + thingSize;
        } else if (MOZ_LIKELY(thing == last)) {
            /*
             * Move to the next span. We use MOZ_LIKELY as without PGO
             * compilers mis-predict == here as unlikely to succeed.
             */
            *this = *reinterpret_cast<FreeSpan *>(thing);
        } else {
            return nullptr;
        }
        checkSpan();
        return reinterpret_cast<void *>(thing);
    }

    /* A version of allocate when we know that the span is not empty. */
    MOZ_ALWAYS_INLINE void *infallibleAllocate(size_t thingSize) {
        JS_ASSERT(thingSize % CellSize == 0);
        checkSpan();
        uintptr_t thing = first;
        if (thing < last) {
            first = thing + thingSize;
        } else {
            JS_ASSERT(thing == last);
            *this = *reinterpret_cast<FreeSpan *>(thing);
        }
        checkSpan();
        return reinterpret_cast<void *>(thing);
    }

    /*
     * Allocate from a newly allocated arena. We do not move the free list
     * from the arena. Rather we set the arena up as fully used during the
     * initialization so to allocate we simply return the first thing in the
     * arena and set the free list to point to the second.
     */
    MOZ_ALWAYS_INLINE void *allocateFromNewArena(uintptr_t arenaAddr, size_t firstThingOffset,
                                                size_t thingSize) {
        JS_ASSERT(!(arenaAddr & ArenaMask));
        uintptr_t thing = arenaAddr | firstThingOffset;
        first = thing + thingSize;
        last = arenaAddr | ArenaMask;
        checkSpan();
        return reinterpret_cast<void *>(thing);
    }

    void checkSpan() const {
#ifdef DEBUG
        /* We do not allow spans at the end of the address space. */
        JS_ASSERT(last != uintptr_t(-1));
        JS_ASSERT(first);
        JS_ASSERT(last);
        JS_ASSERT(first - 1 <= last);
        uintptr_t arenaAddr = arenaAddressUnchecked();
        if (last & 1) {
            /* The span is the last. */
            JS_ASSERT((last & ArenaMask) == ArenaMask);

            if (first - 1 == last) {
                /* The span is last and empty. The above start != 0 check
                 * implies that we are not at the end of the address space.
                 */
                return;
            }
            size_t spanLength = last - first + 1;
            JS_ASSERT(spanLength % CellSize == 0);

            /* Start and end must belong to the same arena. */
            JS_ASSERT((first & ~ArenaMask) == arenaAddr);
            return;
        }

        /* The span is not the last and we have more spans to follow. */
        JS_ASSERT(first <= last);
        size_t spanLengthWithoutOneThing = last - first;
        JS_ASSERT(spanLengthWithoutOneThing % CellSize == 0);

        JS_ASSERT((first & ~ArenaMask) == arenaAddr);

        /*
         * If there is not enough space before the arena end to allocate one
         * more thing, then the span must be marked as the last one to avoid
         * storing useless empty span reference.
         */
        size_t beforeTail = ArenaSize - (last & ArenaMask);
        JS_ASSERT(beforeTail >= sizeof(FreeSpan) + CellSize);

        FreeSpan *next = reinterpret_cast<FreeSpan *>(last);

        /*
         * The GC things on the list of free spans come from one arena
         * and the spans are linked in ascending address order with
         * at least one non-free thing between spans.
         */
        JS_ASSERT(last < next->first);
        JS_ASSERT(arenaAddr == next->arenaAddressUnchecked());

        if (next->first > next->last) {
            /*
             * The next span is the empty span that terminates the list for
             * arenas that do not have any free things at the end.
             */
            JS_ASSERT(next->first - 1 == next->last);
            JS_ASSERT(arenaAddr + ArenaSize == next->first);
        }
#endif
    }

};

/* Every arena has a header. */
struct ArenaHeader : public JS::shadow::ArenaHeader
{
    friend struct FreeLists;

    /*
     * ArenaHeader::next has two purposes: when unallocated, it points to the
     * next available Arena's header. When allocated, it points to the next
     * arena of the same size class and compartment.
     */
    ArenaHeader     *next;

  private:
    /*
     * The first span of free things in the arena. We encode it as the start
     * and end offsets within the arena, not as FreeSpan structure, to
     * minimize the header size.
     */
    size_t          firstFreeSpanOffsets;

    /*
     * One of AllocKind constants or FINALIZE_LIMIT when the arena does not
     * contain any GC things and is on the list of empty arenas in the GC
     * chunk. The latter allows to quickly check if the arena is allocated
     * during the conservative GC scanning without searching the arena in the
     * list.
     *
     * We use 8 bits for the allocKind so the compiler can use byte-level memory
     * instructions to access it.
     */
    size_t       allocKind          : 8;

    /*
     * When collecting we sometimes need to keep an auxillary list of arenas,
     * for which we use the following fields.  This happens for several reasons:
     *
     * When recursive marking uses too much stack the marking is delayed and the
     * corresponding arenas are put into a stack. To distinguish the bottom of
     * the stack from the arenas not present in the stack we use the
     * markOverflow flag to tag arenas on the stack.
     *
     * Delayed marking is also used for arenas that we allocate into during an
     * incremental GC. In this case, we intend to mark all the objects in the
     * arena, and it's faster to do this marking in bulk.
     *
     * When sweeping we keep track of which arenas have been allocated since the
     * end of the mark phase.  This allows us to tell whether a pointer to an
     * unmarked object is yet to be finalized or has already been reallocated.
     * We set the allocatedDuringIncremental flag for this and clear it at the
     * end of the sweep phase.
     *
     * To minimize the ArenaHeader size we record the next linkage as
     * arenaAddress() >> ArenaShift and pack it with the allocKind field and the
     * flags.
     */
  public:
    size_t       hasDelayedMarking  : 1;
    size_t       allocatedDuringIncremental : 1;
    size_t       markOverflow : 1;
    size_t       auxNextLink : JS_BITS_PER_WORD - 8 - 1 - 1 - 1;
    static_assert(ArenaShift >= 8 + 1 + 1 + 1,
                  "ArenaHeader::auxNextLink packing assumes that ArenaShift has enough bits to "
                  "cover allocKind and hasDelayedMarking.");

    inline uintptr_t address() const;
    inline Chunk *chunk() const;

    bool allocated() const {
        JS_ASSERT(allocKind <= size_t(FINALIZE_LIMIT));
        return allocKind < size_t(FINALIZE_LIMIT);
    }

    void init(JS::Zone *zoneArg, AllocKind kind) {
        JS_ASSERT(!allocated());
        JS_ASSERT(!markOverflow);
        JS_ASSERT(!allocatedDuringIncremental);
        JS_ASSERT(!hasDelayedMarking);
        zone = zoneArg;

        static_assert(FINALIZE_LIMIT <= 255, "We must be able to fit the allockind into uint8_t.");
        allocKind = size_t(kind);

        /* See comments in FreeSpan::allocateFromNewArena. */
        firstFreeSpanOffsets = FreeSpan::FullArenaOffsets;
    }

    void setAsNotAllocated() {
        allocKind = size_t(FINALIZE_LIMIT);
        markOverflow = 0;
        allocatedDuringIncremental = 0;
        hasDelayedMarking = 0;
        auxNextLink = 0;
    }

    inline uintptr_t arenaAddress() const;
    inline Arena *getArena();

    AllocKind getAllocKind() const {
        JS_ASSERT(allocated());
        return AllocKind(allocKind);
    }

    inline size_t getThingSize() const;

    bool hasFreeThings() const {
        return firstFreeSpanOffsets != FreeSpan::FullArenaOffsets;
    }

    inline bool isEmpty() const;

    void setAsFullyUsed() {
        firstFreeSpanOffsets = FreeSpan::FullArenaOffsets;
    }

    inline FreeSpan getFirstFreeSpan() const;
    inline void setFirstFreeSpan(const FreeSpan *span);

#ifdef DEBUG
    void checkSynchronizedWithFreeList() const;
#endif

    inline ArenaHeader *getNextDelayedMarking() const;
    inline void setNextDelayedMarking(ArenaHeader *aheader);
    inline void unsetDelayedMarking();

    inline ArenaHeader *getNextAllocDuringSweep() const;
    inline void setNextAllocDuringSweep(ArenaHeader *aheader);
    inline void unsetAllocDuringSweep();
};

struct Arena
{
    /*
     * Layout of an arena:
     * An arena is 4K in size and 4K-aligned. It starts with the ArenaHeader
     * descriptor followed by some pad bytes. The remainder of the arena is
     * filled with the array of T things. The pad bytes ensure that the thing
     * array ends exactly at the end of the arena.
     *
     * +-------------+-----+----+----+-----+----+
     * | ArenaHeader | pad | T0 | T1 | ... | Tn |
     * +-------------+-----+----+----+-----+----+
     *
     * <----------------------------------------> = ArenaSize bytes
     * <-------------------> = first thing offset
     */
    ArenaHeader aheader;
    uint8_t     data[ArenaSize - sizeof(ArenaHeader)];

  private:
    static JS_FRIEND_DATA(const uint32_t) ThingSizes[];
    static JS_FRIEND_DATA(const uint32_t) FirstThingOffsets[];

  public:
    static void staticAsserts();

    static size_t thingSize(AllocKind kind) {
        return ThingSizes[kind];
    }

    static size_t firstThingOffset(AllocKind kind) {
        return FirstThingOffsets[kind];
    }

    static size_t thingsPerArena(size_t thingSize) {
        JS_ASSERT(thingSize % CellSize == 0);

        /* We should be able to fit FreeSpan in any GC thing. */
        JS_ASSERT(thingSize >= sizeof(FreeSpan));

        return (ArenaSize - sizeof(ArenaHeader)) / thingSize;
    }

    static size_t thingsSpan(size_t thingSize) {
        return thingsPerArena(thingSize) * thingSize;
    }

    static bool isAligned(uintptr_t thing, size_t thingSize) {
        /* Things ends at the arena end. */
        uintptr_t tailOffset = (ArenaSize - thing) & ArenaMask;
        return tailOffset % thingSize == 0;
    }

    uintptr_t address() const {
        return aheader.address();
    }

    uintptr_t thingsStart(AllocKind thingKind) {
        return address() | firstThingOffset(thingKind);
    }

    uintptr_t thingsEnd() {
        return address() + ArenaSize;
    }

    template <typename T>
    bool finalize(FreeOp *fop, AllocKind thingKind, size_t thingSize);
};

static_assert(sizeof(Arena) == ArenaSize, "The hardcoded arena size must match the struct size.");

inline size_t
ArenaHeader::getThingSize() const
{
    JS_ASSERT(allocated());
    return Arena::thingSize(getAllocKind());
}

/*
 * The tail of the chunk info is shared between all chunks in the system, both
 * nursery and tenured. This structure is locatable from any GC pointer by
 * aligning to 1MiB.
 */
struct ChunkTrailer
{
    JSRuntime       *runtime;
};

/* The chunk header (located at the end of the chunk to preserve arena alignment). */
struct ChunkInfo
{
    Chunk           *next;
    Chunk           **prevp;

    /* Free arenas are linked together with aheader.next. */
    ArenaHeader     *freeArenasHead;

#if JS_BITS_PER_WORD == 32
    /*
     * Calculating sizes and offsets is simpler if sizeof(ChunkInfo) is
     * architecture-independent.
     */
    char            padding[16];
#endif

    /*
     * Decommitted arenas are tracked by a bitmap in the chunk header. We use
     * this offset to start our search iteration close to a decommitted arena
     * that we can allocate.
     */
    uint32_t        lastDecommittedArenaOffset;

    /* Number of free arenas, either committed or decommitted. */
    uint32_t        numArenasFree;

    /* Number of free, committed arenas. */
    uint32_t        numArenasFreeCommitted;

    /* Number of GC cycles this chunk has survived. */
    uint32_t        age;

    /* Information shared by all Chunk types. */
    ChunkTrailer    trailer;
};

/*
 * Calculating ArenasPerChunk:
 *
 * In order to figure out how many Arenas will fit in a chunk, we need to know
 * how much extra space is available after we allocate the header data. This
 * is a problem because the header size depends on the number of arenas in the
 * chunk. The two dependent fields are bitmap and decommittedArenas.
 *
 * For the mark bitmap, we know that each arena will use a fixed number of full
 * bytes: ArenaBitmapBytes. The full size of the header data is this number
 * multiplied by the eventual number of arenas we have in the header. We,
 * conceptually, distribute this header data among the individual arenas and do
 * not include it in the header. This way we do not have to worry about its
 * variable size: it gets attached to the variable number we are computing.
 *
 * For the decommitted arena bitmap, we only have 1 bit per arena, so this
 * technique will not work. Instead, we observe that we do not have enough
 * header info to fill 8 full arenas: it is currently 4 on 64bit, less on
 * 32bit. Thus, with current numbers, we need 64 bytes for decommittedArenas.
 * This will not become 63 bytes unless we double the data required in the
 * header. Therefore, we just compute the number of bytes required to track
 * every possible arena and do not worry about slop bits, since there are too
 * few to usefully allocate.
 *
 * To actually compute the number of arenas we can allocate in a chunk, we
 * divide the amount of available space less the header info (not including
 * the mark bitmap which is distributed into the arena size) by the size of
 * the arena (with the mark bitmap bytes it uses).
 */
const size_t BytesPerArenaWithHeader = ArenaSize + ArenaBitmapBytes;
const size_t ChunkDecommitBitmapBytes = ChunkSize / ArenaSize / JS_BITS_PER_BYTE;
const size_t ChunkBytesAvailable = ChunkSize - sizeof(ChunkInfo) - ChunkDecommitBitmapBytes;
const size_t ArenasPerChunk = ChunkBytesAvailable / BytesPerArenaWithHeader;
static_assert(ArenasPerChunk == 252, "Do not accidentally change our heap's density.");

/* A chunk bitmap contains enough mark bits for all the cells in a chunk. */
struct ChunkBitmap
{
    volatile uintptr_t bitmap[ArenaBitmapWords * ArenasPerChunk];

  public:
    ChunkBitmap() { }

    MOZ_ALWAYS_INLINE void getMarkWordAndMask(const Cell *cell, uint32_t color,
                                              uintptr_t **wordp, uintptr_t *maskp)
    {
        GetGCThingMarkWordAndMask(cell, color, wordp, maskp);
    }

    MOZ_ALWAYS_INLINE MOZ_TSAN_BLACKLIST bool isMarked(const Cell *cell, uint32_t color) {
        uintptr_t *word, mask;
        getMarkWordAndMask(cell, color, &word, &mask);
        return *word & mask;
    }

    MOZ_ALWAYS_INLINE bool markIfUnmarked(const Cell *cell, uint32_t color) {
        uintptr_t *word, mask;
        getMarkWordAndMask(cell, BLACK, &word, &mask);
        if (*word & mask)
            return false;
        *word |= mask;
        if (color != BLACK) {
            /*
             * We use getMarkWordAndMask to recalculate both mask and word as
             * doing just mask << color may overflow the mask.
             */
            getMarkWordAndMask(cell, color, &word, &mask);
            if (*word & mask)
                return false;
            *word |= mask;
        }
        return true;
    }

    MOZ_ALWAYS_INLINE void unmark(const Cell *cell, uint32_t color) {
        uintptr_t *word, mask;
        getMarkWordAndMask(cell, color, &word, &mask);
        *word &= ~mask;
    }

    void clear() {
        memset((void *)bitmap, 0, sizeof(bitmap));
    }

    uintptr_t *arenaBits(ArenaHeader *aheader) {
        static_assert(ArenaBitmapBits == ArenaBitmapWords * JS_BITS_PER_WORD,
                      "We assume that the part of the bitmap corresponding to the arena "
                      "has the exact number of words so we do not need to deal with a word "
                      "that covers bits from two arenas.");

        uintptr_t *word, unused;
        getMarkWordAndMask(reinterpret_cast<Cell *>(aheader->address()), BLACK, &word, &unused);
        return word;
    }
};

static_assert(ArenaBitmapBytes * ArenasPerChunk == sizeof(ChunkBitmap),
              "Ensure our ChunkBitmap actually covers all arenas.");
static_assert(js::gc::ChunkMarkBitmapBits == ArenaBitmapBits * ArenasPerChunk,
              "Ensure that the mark bitmap has the right number of bits.");

typedef BitArray<ArenasPerChunk> PerArenaBitmap;

const size_t ChunkPadSize = ChunkSize
                            - (sizeof(Arena) * ArenasPerChunk)
                            - sizeof(ChunkBitmap)
                            - sizeof(PerArenaBitmap)
                            - sizeof(ChunkInfo);
static_assert(ChunkPadSize < BytesPerArenaWithHeader,
              "If the chunk padding is larger than an arena, we should have one more arena.");

/*
 * Chunks contain arenas and associated data structures (mark bitmap, delayed
 * marking state).
 */
struct Chunk
{
    Arena           arenas[ArenasPerChunk];

    /* Pad to full size to ensure cache alignment of ChunkInfo. */
    uint8_t         padding[ChunkPadSize];

    ChunkBitmap     bitmap;
    PerArenaBitmap  decommittedArenas;
    ChunkInfo       info;

    static Chunk *fromAddress(uintptr_t addr) {
        addr &= ~ChunkMask;
        return reinterpret_cast<Chunk *>(addr);
    }

    static bool withinArenasRange(uintptr_t addr) {
        uintptr_t offset = addr & ChunkMask;
        return offset < ArenasPerChunk * ArenaSize;
    }

    static size_t arenaIndex(uintptr_t addr) {
        JS_ASSERT(withinArenasRange(addr));
        return (addr & ChunkMask) >> ArenaShift;
    }

    uintptr_t address() const {
        uintptr_t addr = reinterpret_cast<uintptr_t>(this);
        JS_ASSERT(!(addr & ChunkMask));
        return addr;
    }

    bool unused() const {
        return info.numArenasFree == ArenasPerChunk;
    }

    bool hasAvailableArenas() const {
        return info.numArenasFree != 0;
    }

    inline void addToAvailableList(JS::Zone *zone);
    inline void insertToAvailableList(Chunk **insertPoint);
    inline void removeFromAvailableList();

    ArenaHeader *allocateArena(JS::Zone *zone, AllocKind kind);

    void releaseArena(ArenaHeader *aheader);

    static Chunk *allocate(JSRuntime *rt);

    /* Must be called with the GC lock taken. */
    static inline void release(JSRuntime *rt, Chunk *chunk);
    static inline void releaseList(JSRuntime *rt, Chunk *chunkListHead);

    /* Must be called with the GC lock taken. */
    inline void prepareToBeFreed(JSRuntime *rt);

    /*
     * Assuming that the info.prevp points to the next field of the previous
     * chunk in a doubly-linked list, get that chunk.
     */
    Chunk *getPrevious() {
        JS_ASSERT(info.prevp);
        return fromPointerToNext(info.prevp);
    }

    /* Get the chunk from a pointer to its info.next field. */
    static Chunk *fromPointerToNext(Chunk **nextFieldPtr) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(nextFieldPtr);
        JS_ASSERT((addr & ChunkMask) == offsetof(Chunk, info.next));
        return reinterpret_cast<Chunk *>(addr - offsetof(Chunk, info.next));
    }

  private:
    inline void init(JSRuntime *rt);

    /* Search for a decommitted arena to allocate. */
    unsigned findDecommittedArenaOffset();
    ArenaHeader* fetchNextDecommittedArena();

  public:
    /* Unlink and return the freeArenasHead. */
    inline ArenaHeader* fetchNextFreeArena(JSRuntime *rt);

    inline void addArenaToFreeList(JSRuntime *rt, ArenaHeader *aheader);
};

static_assert(sizeof(Chunk) == ChunkSize,
              "Ensure the hardcoded chunk size definition actually matches the struct.");
static_assert(js::gc::ChunkMarkBitmapOffset == offsetof(Chunk, bitmap),
              "The hardcoded API bitmap offset must match the actual offset.");
static_assert(js::gc::ChunkRuntimeOffset == offsetof(Chunk, info) +
                                               offsetof(ChunkInfo, trailer) +
                                               offsetof(ChunkTrailer, runtime),
              "The hardcoded API runtime offset must match the actual offset.");

inline uintptr_t
ArenaHeader::address() const
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(this);
    JS_ASSERT(!(addr & ArenaMask));
    JS_ASSERT(Chunk::withinArenasRange(addr));
    return addr;
}

inline Chunk *
ArenaHeader::chunk() const
{
    return Chunk::fromAddress(address());
}

inline uintptr_t
ArenaHeader::arenaAddress() const
{
    return address();
}

inline Arena *
ArenaHeader::getArena()
{
    return reinterpret_cast<Arena *>(arenaAddress());
}

inline bool
ArenaHeader::isEmpty() const
{
    /* Arena is empty if its first span covers the whole arena. */
    JS_ASSERT(allocated());
    size_t firstThingOffset = Arena::firstThingOffset(getAllocKind());
    return firstFreeSpanOffsets == FreeSpan::encodeOffsets(firstThingOffset, ArenaMask);
}

FreeSpan
ArenaHeader::getFirstFreeSpan() const
{
#ifdef DEBUG
    checkSynchronizedWithFreeList();
#endif
    return FreeSpan::decodeOffsets(arenaAddress(), firstFreeSpanOffsets);
}

void
ArenaHeader::setFirstFreeSpan(const FreeSpan *span)
{
    JS_ASSERT(span->isWithinArena(arenaAddress()));
    firstFreeSpanOffsets = span->encodeAsOffsets();
}

inline ArenaHeader *
ArenaHeader::getNextDelayedMarking() const
{
    JS_ASSERT(hasDelayedMarking);
    return &reinterpret_cast<Arena *>(auxNextLink << ArenaShift)->aheader;
}

inline void
ArenaHeader::setNextDelayedMarking(ArenaHeader *aheader)
{
    JS_ASSERT(!(uintptr_t(aheader) & ArenaMask));
    JS_ASSERT(!auxNextLink && !hasDelayedMarking);
    hasDelayedMarking = 1;
    auxNextLink = aheader->arenaAddress() >> ArenaShift;
}

inline void
ArenaHeader::unsetDelayedMarking()
{
    JS_ASSERT(hasDelayedMarking);
    hasDelayedMarking = 0;
    auxNextLink = 0;
}

inline ArenaHeader *
ArenaHeader::getNextAllocDuringSweep() const
{
    JS_ASSERT(allocatedDuringIncremental);
    return &reinterpret_cast<Arena *>(auxNextLink << ArenaShift)->aheader;
}

inline void
ArenaHeader::setNextAllocDuringSweep(ArenaHeader *aheader)
{
    JS_ASSERT(!auxNextLink && !allocatedDuringIncremental);
    allocatedDuringIncremental = 1;
    auxNextLink = aheader->arenaAddress() >> ArenaShift;
}

inline void
ArenaHeader::unsetAllocDuringSweep()
{
    JS_ASSERT(allocatedDuringIncremental);
    allocatedDuringIncremental = 0;
    auxNextLink = 0;
}

static void
AssertValidColor(const void *thing, uint32_t color)
{
#ifdef DEBUG
    ArenaHeader *aheader = reinterpret_cast<const Cell *>(thing)->arenaHeader();
    JS_ASSERT_IF(color, color < aheader->getThingSize() / CellSize);
#endif
}

inline ArenaHeader *
Cell::arenaHeader() const
{
    JS_ASSERT(isTenured());
    uintptr_t addr = address();
    addr &= ~ArenaMask;
    return reinterpret_cast<ArenaHeader *>(addr);
}

inline JSRuntime *
Cell::runtimeFromMainThread() const
{
    JSRuntime *rt = chunk()->info.trailer.runtime;
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));
    return rt;
}

inline JS::shadow::Runtime *
Cell::shadowRuntimeFromMainThread() const
{
    return reinterpret_cast<JS::shadow::Runtime*>(runtimeFromMainThread());
}

inline JSRuntime *
Cell::runtimeFromAnyThread() const
{
    return chunk()->info.trailer.runtime;
}

inline JS::shadow::Runtime *
Cell::shadowRuntimeFromAnyThread() const
{
    return reinterpret_cast<JS::shadow::Runtime*>(runtimeFromAnyThread());
}

bool
Cell::isMarked(uint32_t color /* = BLACK */) const
{
    JS_ASSERT(isTenured());
    AssertValidColor(this, color);
    return chunk()->bitmap.isMarked(this, color);
}

bool
Cell::markIfUnmarked(uint32_t color /* = BLACK */) const
{
    JS_ASSERT(isTenured());
    AssertValidColor(this, color);
    return chunk()->bitmap.markIfUnmarked(this, color);
}

void
Cell::unmark(uint32_t color) const
{
    JS_ASSERT(isTenured());
    JS_ASSERT(color != BLACK);
    AssertValidColor(this, color);
    chunk()->bitmap.unmark(this, color);
}

JS::Zone *
Cell::tenuredZone() const
{
    JS::Zone *zone = arenaHeader()->zone;
    JS_ASSERT(CurrentThreadCanAccessZone(zone));
    JS_ASSERT(isTenured());
    return zone;
}

JS::Zone *
Cell::tenuredZoneFromAnyThread() const
{
    JS_ASSERT(isTenured());
    return arenaHeader()->zone;
}

bool
Cell::tenuredIsInsideZone(JS::Zone *zone) const
{
    JS_ASSERT(isTenured());
    return zone == arenaHeader()->zone;
}

#ifdef DEBUG
bool
Cell::isAligned() const
{
    return Arena::isAligned(address(), arenaHeader()->getThingSize());
}

bool
Cell::isTenured() const
{
#ifdef JSGC_GENERATIONAL
    JS::shadow::Runtime *rt = js::gc::GetGCThingRuntime(this);
    return !IsInsideNursery(rt, this);
#endif
    return true;
}
#endif

inline uintptr_t
Cell::address() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % CellSize == 0);
    JS_ASSERT(Chunk::withinArenasRange(addr));
    return addr;
}

Chunk *
Cell::chunk() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % CellSize == 0);
    addr &= ~(ChunkSize - 1);
    return reinterpret_cast<Chunk *>(addr);
}

inline bool
InFreeList(ArenaHeader *aheader, void *thing)
{
    if (!aheader->hasFreeThings())
        return false;

    FreeSpan firstSpan(aheader->getFirstFreeSpan());
    uintptr_t addr = reinterpret_cast<uintptr_t>(thing);

    for (const FreeSpan *span = &firstSpan;;) {
        /* If the thing comes before the current span, it's not free. */
        if (addr < span->first)
            return false;

        /*
         * If we find it inside the span, it's dead. We use here "<=" and not
         * "<" even for the last span as we know that thing is inside the
         * arena. Thus, for the last span thing < span->end.
         */
        if (addr <= span->last)
            return true;

        /*
         * The last possible empty span is an the end of the arena. Here
         * span->end < thing < thingsEnd and so we must have more spans.
         */
        span = span->nextSpan();
    }
}

} /* namespace gc */

// Ion compilation mainly occurs off the main thread, and may run while the
// main thread is performing arbitrary VM operations, excepting GC activity.
// The below class is used to mark functions and other operations which can
// safely be performed off thread without racing. When running with thread
// safety checking on, any access to a GC thing outside of AutoThreadSafeAccess
// will cause an access violation.
class AutoThreadSafeAccess
{
public:
#if defined(DEBUG) && defined(JS_CPU_X64) && !defined(XP_WIN)
#define JS_CAN_CHECK_THREADSAFE_ACCESSES

    JSRuntime *runtime;
    gc::ArenaHeader *arena;

    AutoThreadSafeAccess(const gc::Cell *cell MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoThreadSafeAccess();
#else
    AutoThreadSafeAccess(const gc::Cell *cell MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoThreadSafeAccess() {}
#endif
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

gc::AllocKind
gc::Cell::tenuredGetAllocKind() const
{
    AutoThreadSafeAccess ts(this);
    return arenaHeader()->getAllocKind();
}

} /* namespace js */

#endif /* gc_Heap_h */

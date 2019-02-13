/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of XDR primitives. */

#include "xpt_xdr.h"
#include "nspr.h"
#include "nscore.h"
#include <string.h>             /* strchr */

static PRBool
CheckForRepeat(XPTCursor *cursor, void **addrp, XPTPool pool, uint32_t len,
                   XPTCursor *new_cursor, PRBool *already);

#define ENCODING(cursor)                                                      \
  ((cursor)->state->mode == XPT_ENCODE)

#define CURS_POOL_OFFSET_RAW(cursor)                                          \
  ((cursor)->pool == XPT_HEADER                                               \
   ? (cursor)->offset                                                         \
   : (XPT_ASSERT((cursor)->state->data_offset),                               \
      (cursor)->offset + (cursor)->state->data_offset))

#define CURS_POOL_OFFSET(cursor)                                              \
  (CURS_POOL_OFFSET_RAW(cursor) - 1)

/* can be used as lvalue */
#define CURS_POINT(cursor)                                                    \
  ((cursor)->state->pool->data[CURS_POOL_OFFSET(cursor)])

#if defined(DEBUG_shaver) || defined(DEBUG_jband) || defined(DEBUG_mccabe)
#define DBG(x) printf x
#else
#define DBG(x) (0)
#endif

/* XXX fail if XPT_DATA and !state->data_offset */
#define CHECK_COUNT_(cursor, space)                                           \
 /* if we're in the header, then exceeding the data_offset is illegal */      \
((cursor)->pool == XPT_HEADER ?                                               \
 (ENCODING(cursor) &&                                                         \
  ((cursor)->state->data_offset &&                                            \
   ((cursor)->offset - 1 + (space) > (cursor)->state->data_offset))           \
  ? (DBG(("no space left in HEADER %d + %d > %d\n", (cursor)->offset,         \
          (space), (cursor)->state->data_offset)) && PR_FALSE)                \
  : PR_TRUE) :                                                                \
 /* if we're in the data area and we're about to exceed the allocation */     \
 (CURS_POOL_OFFSET(cursor) + (space) > (cursor)->state->pool->allocated ?     \
  /* then grow if we're in ENCODE mode */                                     \
  (ENCODING(cursor) ? GrowPool((cursor)->state->arena,                        \
                               (cursor)->state->pool,                         \
                               (cursor)->state->pool->allocated,              \
                               0, CURS_POOL_OFFSET(cursor) + (space))         \
   /* and fail if we're in DECODE mode */                                     \
   : (DBG(("can't extend in DECODE")) && PR_FALSE))                           \
  /* otherwise we're OK */                                                    \
  : PR_TRUE))

#define CHECK_COUNT(cursor, space)                                            \
  (CHECK_COUNT_(cursor, space)                                                \
   ? PR_TRUE                                                                  \
   : (XPT_ASSERT(0),                                                          \
      fprintf(stderr, "FATAL: can't no room for %d in cursor\n", space),      \
      PR_FALSE))

/* increase the data allocation for the pool by XPT_GROW_CHUNK */
#define XPT_GROW_CHUNK  8192

/*
 * quick and dirty hardcoded hashtable, to avoid dependence on nspr or glib.
 * XXXmccabe it might turn out that we could use a simpler data structure here.
 */
typedef struct XPTHashRecord {
    void *key;
    void *value;
    struct XPTHashRecord *next;
} XPTHashRecord;

#define XPT_HASHSIZE 512

struct XPTHashTable {  /* it's already typedef'ed from before. */
    XPTHashRecord *buckets[XPT_HASHSIZE];
    XPTArena *arena;
};

static XPTHashTable *
XPT_NewHashTable(XPTArena *arena) {
    XPTHashTable *table;
    table = XPT_NEWZAP(arena, XPTHashTable);
    if (table)
        table->arena = arena;
    return table;
}

static void trimrecord(XPTArena* arena, XPTHashRecord *record) {
    if (record == NULL)
        return;
    trimrecord(arena, record->next);
    XPT_DELETE(arena, record);
}

static void
XPT_HashTableDestroy(XPTHashTable *table) {
    int i;
    for (i = 0; i < XPT_HASHSIZE; i++)
        trimrecord(table->arena, table->buckets[i]);
    XPT_FREE(table->arena, table);
}

static void *
XPT_HashTableAdd(XPTHashTable *table, void *key, void *value) {
    XPTHashRecord **bucketloc = table->buckets +
        (NS_PTR_TO_UINT32(key) % XPT_HASHSIZE);
    XPTHashRecord *bucket;

    while (*bucketloc != NULL)
        bucketloc = &((*bucketloc)->next);

    bucket = XPT_NEW(table->arena, XPTHashRecord);
    bucket->key = key;
    bucket->value = value;
    bucket->next = NULL;
    *bucketloc = bucket;
    return value;
}

static void *
XPT_HashTableLookup(XPTHashTable *table, void *key) {
    XPTHashRecord *bucket = table->buckets[NS_PTR_TO_UINT32(key) % XPT_HASHSIZE];
    while (bucket != NULL) {
        if (bucket->key == key)
            return bucket->value;
        bucket = bucket->next;
    }
    return NULL;
}

XPT_PUBLIC_API(XPTState *)
XPT_NewXDRState(XPTMode mode, char *data, uint32_t len)
{
    XPTState *state;
    XPTArena *arena;

    arena = XPT_NewArena(512, sizeof(double), "an XDRState");
    if (!arena)
        return NULL;

    state = XPT_NEWZAP(arena, XPTState);
    if (!state)
        goto err_free_arena;

    state->arena = arena;
    state->mode = mode;
    state->pool = XPT_NEW(arena, XPTDatapool);
    state->next_cursor[0] = state->next_cursor[1] = 1;
    if (!state->pool)
        goto err_free_state;

    state->pool->count = 0;
    state->pool->offset_map = XPT_NewHashTable(arena);

    if (!state->pool->offset_map)
        goto err_free_pool;
    if (mode == XPT_DECODE) {
        state->pool->data = data;
        state->pool->allocated = len;
    } else {
        state->pool->data = (char*)XPT_MALLOC(arena, XPT_GROW_CHUNK);
        if (!state->pool->data)
            goto err_free_hash;
        state->pool->allocated = XPT_GROW_CHUNK;
    }

    return state;

 err_free_hash:
    XPT_HashTableDestroy(state->pool->offset_map);
 err_free_pool:
    XPT_DELETE(arena, state->pool);
 err_free_state:
    XPT_DELETE(arena, state);
 err_free_arena:
    if (arena)
        XPT_DestroyArena(arena);
    return NULL;
}

XPT_PUBLIC_API(void)
XPT_DestroyXDRState(XPTState *state)
{
    XPTArena *arena = state->arena;

    if (state->pool->offset_map)
        XPT_HashTableDestroy(state->pool->offset_map);
    if (state->mode == XPT_ENCODE)
        XPT_DELETE(arena, state->pool->data);
    XPT_DELETE(arena, state->pool);
    XPT_DELETE(arena, state);
    if (arena)
        XPT_DestroyArena(arena);
}

XPT_PUBLIC_API(void)
XPT_GetXDRDataLength(XPTState *state, XPTPool pool, uint32_t *len)
{
    *len = state->next_cursor[pool] - 1;
}

XPT_PUBLIC_API(void)
XPT_GetXDRData(XPTState *state, XPTPool pool, char **data, uint32_t *len)
{
    if (pool == XPT_HEADER) {
        *data = state->pool->data;
    } else {
        *data = state->pool->data + state->data_offset;
    }
    *len = state->next_cursor[pool] - 1;
}

/* All offsets are 1-based */
XPT_PUBLIC_API(void)
XPT_DataOffset(XPTState *state, uint32_t *data_offsetp)
{
    if (state->mode == XPT_DECODE)
        XPT_SetDataOffset(state, *data_offsetp);
    else
        *data_offsetp = state->data_offset;
}

/* if 'exact' is set use that, else grow by the next chunk but
 * be sure to grow no less that 'at_least' so that we can't get
 * behind on required space.
 */
static PRBool
GrowPool(XPTArena *arena, XPTDatapool *pool, uint32_t old_size, 
         uint32_t exact, uint32_t at_least)
{
    uint32_t total_size;
    char *newdata;

    if (exact) {
        XPT_ASSERT(exact > pool->allocated);
        total_size = exact;
    } else {
        total_size = pool->allocated + XPT_GROW_CHUNK;
        if (at_least > total_size)
            total_size = at_least;
    }

    newdata = (char*)XPT_MALLOC(arena, total_size);
    if (!newdata)
        return PR_FALSE;
    if (pool->data) {
        if (old_size)
            memcpy(newdata, pool->data, old_size);
        XPT_FREE(arena, pool->data);
    }
    pool->data = newdata;
    pool->allocated = total_size;
    return PR_TRUE;
}

XPT_PUBLIC_API(void)
XPT_SetDataOffset(XPTState *state, uint32_t data_offset)
{
   state->data_offset = data_offset;
   /* make sure we've allocated enough space for the header */
   if (state->mode == XPT_ENCODE &&
       data_offset > state->pool->allocated) {
       (void)GrowPool(state->arena, state->pool, state->pool->allocated, 
                      data_offset, 0);
   }
}

XPT_PUBLIC_API(PRBool)
XPT_MakeCursor(XPTState *state, XPTPool pool, uint32_t len, XPTCursor *cursor)
{
    cursor->state = state;
    cursor->pool = pool;
    cursor->bits = 0;
    cursor->offset = state->next_cursor[pool];

    if (!(CHECK_COUNT(cursor, len)))
        return PR_FALSE;

    /* this check should be in CHECK_CURSOR */
    if (pool == XPT_DATA && !state->data_offset) {
        fprintf(stderr, "no data offset for XPT_DATA cursor!\n");
        return PR_FALSE;
    }

    state->next_cursor[pool] += len;

    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_SeekTo(XPTCursor *cursor, uint32_t offset)
{
    /* XXX do some real checking and update len and stuff */
    cursor->offset = offset;
    return PR_TRUE;
}

XPT_PUBLIC_API(XPTString *)
XPT_NewString(XPTArena *arena, uint16_t length, const char *bytes)
{
    XPTString *str = XPT_NEW(arena, XPTString);
    if (!str)
        return NULL;
    str->length = length;
    /* Alloc one extra to store the trailing nul. */
    str->bytes = (char*)XPT_MALLOC(arena, length + 1u);
    if (!str->bytes) {
        XPT_DELETE(arena, str);
        return NULL;
    }
    memcpy(str->bytes, bytes, length);
    /* nul-terminate it. */
    str->bytes[length] = '\0';
    return str;
}

XPT_PUBLIC_API(XPTString *)
XPT_NewStringZ(XPTArena *arena, const char *bytes)
{
    uint32_t length = strlen(bytes);
    if (length > 0xffff)
        return NULL;            /* too long */
    return XPT_NewString(arena, (uint16_t)length, bytes);
}

XPT_PUBLIC_API(PRBool)
XPT_DoStringInline(XPTArena *arena, XPTCursor *cursor, XPTString **strp)
{
    XPTString *str = *strp;
    XPTMode mode = cursor->state->mode;
    int i;

    if (mode == XPT_DECODE) {
        str = XPT_NEWZAP(arena, XPTString);
        if (!str)
            return PR_FALSE;
        *strp = str;
    }

    if (!XPT_Do16(cursor, &str->length))
        goto error;

    if (mode == XPT_DECODE)
        if (!(str->bytes = (char*)XPT_MALLOC(arena, str->length + 1u)))
            goto error;

    for (i = 0; i < str->length; i++)
        if (!XPT_Do8(cursor, (uint8_t *)&str->bytes[i]))
            goto error_2;

    if (mode == XPT_DECODE)
        str->bytes[str->length] = 0;

    return PR_TRUE;
 error_2:
    XPT_DELETE(arena, str->bytes);
 error:
    XPT_DELETE(arena, str);
    return PR_FALSE;
}

XPT_PUBLIC_API(PRBool)
XPT_DoString(XPTArena *arena, XPTCursor *cursor, XPTString **strp)
{
    XPTCursor my_cursor;
    XPTString *str = *strp;
    PRBool already;

    XPT_PREAMBLE_NO_ALLOC(cursor, strp, XPT_DATA, str->length + 2u, my_cursor,
                          already)

    return XPT_DoStringInline(arena, &my_cursor, strp);
}

XPT_PUBLIC_API(PRBool)
XPT_DoCString(XPTArena *arena, XPTCursor *cursor, char **identp)
{
    XPTCursor my_cursor;
    char *ident = *identp;
    uint32_t offset = 0;

    XPTMode mode = cursor->state->mode;

    if (mode == XPT_DECODE) {
        char *start, *end;
        int len;

        if (!XPT_Do32(cursor, &offset))
            return PR_FALSE;

        if (!offset) {
            *identp = NULL;
            return PR_TRUE;
        }

        my_cursor.pool = XPT_DATA;
        my_cursor.offset = offset;
        my_cursor.state = cursor->state;
        start = &CURS_POINT(&my_cursor);

        end = strchr(start, 0); /* find the end of the string */
        if (!end) {
            fprintf(stderr, "didn't find end of string on decode!\n");
            return PR_FALSE;
        }
        len = end - start;
        XPT_ASSERT(len > 0);

        ident = (char*)XPT_MALLOC(arena, len + 1u);
        if (!ident)
            return PR_FALSE;

        memcpy(ident, start, (size_t)len);
        ident[len] = 0;
        *identp = ident;

    } else {

        if (!ident) {
            offset = 0;
            if (!XPT_Do32(cursor, &offset))
                return PR_FALSE;
            return PR_TRUE;
        }

        if (!XPT_MakeCursor(cursor->state, XPT_DATA, strlen(ident) + 1,
                            &my_cursor) ||
            !XPT_Do32(cursor, &my_cursor.offset))
            return PR_FALSE;

        while(*ident)
            if (!XPT_Do8(&my_cursor, (uint8_t *)ident++))
                return PR_FALSE;
        if (!XPT_Do8(&my_cursor, (uint8_t *)ident)) /* write trailing zero */
            return PR_FALSE;
    }

    return PR_TRUE;
}

/* XXXjband it bothers me that this is one hashtable instead of two.
 */
XPT_PUBLIC_API(uint32_t)
XPT_GetOffsetForAddr(XPTCursor *cursor, void *addr)
{
    XPTHashTable *table = cursor->state->pool->offset_map;
    return NS_PTR_TO_UINT32(XPT_HashTableLookup(table, addr));
}

XPT_PUBLIC_API(PRBool)
XPT_SetOffsetForAddr(XPTCursor *cursor, void *addr, uint32_t offset)
{
    return XPT_HashTableAdd(cursor->state->pool->offset_map,
                            addr, NS_INT32_TO_PTR(offset)) != NULL;
}

XPT_PUBLIC_API(PRBool)
XPT_SetAddrForOffset(XPTCursor *cursor, uint32_t offset, void *addr)
{
    return XPT_HashTableAdd(cursor->state->pool->offset_map,
                            NS_INT32_TO_PTR(offset), addr) != NULL;
}

XPT_PUBLIC_API(void *)
XPT_GetAddrForOffset(XPTCursor *cursor, uint32_t offset)
{
    return XPT_HashTableLookup(cursor->state->pool->offset_map,
                               NS_INT32_TO_PTR(offset));
}

/* Used by XPT_PREAMBLE_NO_ALLOC. */
static PRBool
CheckForRepeat(XPTCursor *cursor, void **addrp, XPTPool pool, uint32_t len,
               XPTCursor *new_cursor, PRBool *already)
{
    void *last = *addrp;

    *already = PR_FALSE;
    new_cursor->state = cursor->state;
    new_cursor->pool = pool;
    new_cursor->bits = 0;

    if (cursor->state->mode == XPT_DECODE) {

        last = XPT_GetAddrForOffset(new_cursor, new_cursor->offset);

        if (last) {
            *already = PR_TRUE;
            *addrp = last;
        }

    } else {

        new_cursor->offset = XPT_GetOffsetForAddr(new_cursor, last);
        if (new_cursor->offset) {
            *already = PR_TRUE;
            return PR_TRUE;
        }

        /* haven't already found it, so allocate room for it. */
        if (!XPT_MakeCursor(cursor->state, pool, len, new_cursor) ||
            !XPT_SetOffsetForAddr(new_cursor, *addrp, new_cursor->offset))
            return PR_FALSE;
    }
    return PR_TRUE;
}

/*
 * IIDs are written in struct order, in the usual big-endian way.  From the
 * typelib file spec:
 *
 *   "For example, this IID:
 *     {00112233-4455-6677-8899-aabbccddeeff}
 *   is converted to the 128-bit value
 *     0x00112233445566778899aabbccddeeff
 *   Note that the byte storage order corresponds to the layout of the nsIID
 *   C-struct on a big-endian architecture."
 *
 * (http://www.mozilla.org/scriptable/typelib_file.html#iid)
 */
XPT_PUBLIC_API(PRBool)
XPT_DoIID(XPTCursor *cursor, nsID *iidp)
{
    int i;

    if (!XPT_Do32(cursor, &iidp->m0) ||
        !XPT_Do16(cursor, &iidp->m1) ||
        !XPT_Do16(cursor, &iidp->m2))
        return PR_FALSE;

    for (i = 0; i < 8; i++)
        if (!XPT_Do8(cursor, (uint8_t *)&iidp->m3[i]))
            return PR_FALSE;

    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_Do64(XPTCursor *cursor, int64_t *u64p)
{
    return XPT_Do32(cursor, (uint32_t *)u64p) &&
        XPT_Do32(cursor, ((uint32_t *)u64p) + 1);
}

/*
 * When we're writing 32- or 16-bit quantities, we write a byte at a time to
 * avoid alignment issues.  Someone could come and optimize this to detect
 * well-aligned cases and do a single store, if they cared.  I might care
 * later.
 */
XPT_PUBLIC_API(PRBool)
XPT_Do32(XPTCursor *cursor, uint32_t *u32p)
{
    union {
        uint8_t b8[4];
        uint32_t b32;
    } u;

    if (!CHECK_COUNT(cursor, 4))
        return PR_FALSE;

    if (ENCODING(cursor)) {
        u.b32 = XPT_SWAB32(*u32p);
        CURS_POINT(cursor) = u.b8[0];
        cursor->offset++;
        CURS_POINT(cursor) = u.b8[1];
        cursor->offset++;
        CURS_POINT(cursor) = u.b8[2];
        cursor->offset++;
        CURS_POINT(cursor) = u.b8[3];
    } else {
        u.b8[0] = CURS_POINT(cursor);
        cursor->offset++;
        u.b8[1] = CURS_POINT(cursor);
        cursor->offset++;
        u.b8[2] = CURS_POINT(cursor);
        cursor->offset++;
        u.b8[3] = CURS_POINT(cursor);
        *u32p = XPT_SWAB32(u.b32);
    }
    cursor->offset++;
    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_Do16(XPTCursor *cursor, uint16_t *u16p)
{
    union {
        uint8_t b8[2];
        uint16_t b16;
    } u;

    if (!CHECK_COUNT(cursor, 2))
        return PR_FALSE;

    if (ENCODING(cursor)) {
        u.b16 = XPT_SWAB16(*u16p);
        CURS_POINT(cursor) = u.b8[0];
        cursor->offset++;
        CURS_POINT(cursor) = u.b8[1];
    } else {
        u.b8[0] = CURS_POINT(cursor);
        cursor->offset++;
        u.b8[1] = CURS_POINT(cursor);
        *u16p = XPT_SWAB16(u.b16);
    }
    cursor->offset++;

    return PR_TRUE;
}

XPT_PUBLIC_API(PRBool)
XPT_Do8(XPTCursor *cursor, uint8_t *u8p)
{
    if (!CHECK_COUNT(cursor, 1))
        return PR_FALSE;
    if (cursor->state->mode == XPT_ENCODE)
        CURS_POINT(cursor) = *u8p;
    else
        *u8p = CURS_POINT(cursor);

    cursor->offset++;

    return PR_TRUE;
}



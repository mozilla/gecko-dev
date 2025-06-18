#ifndef DIPLOMAT_RUNTIME_C_H
#define DIPLOMAT_RUNTIME_C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

// uchar.h doesn't always exist, but char32_t is always available
// in C++ anyway
#ifndef __cplusplus
#ifdef __APPLE__
#include <stdint.h>
typedef uint16_t char16_t;
typedef uint32_t char32_t;
#else
#include <uchar.h>
#endif
#endif


#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

typedef struct DiplomatWriteable {
    void* context;
    char* buf;
    size_t len;
    size_t cap;
    void (*flush)(struct DiplomatWriteable*);
    bool (*grow)(struct DiplomatWriteable*, size_t);
} DiplomatWriteable;

DiplomatWriteable diplomat_simple_writeable(char* buf, size_t buf_size);

#define MAKE_SLICES(name, c_ty) \
    typedef struct Diplomat##name##View { \
        const c_ty* data; \
        size_t len; \
    } Diplomat##name##View; \
    typedef struct Diplomat##name##Array { \
        const c_ty* data; \
        size_t len; \
    } Diplomat##name##Array;

MAKE_SLICES(I8, int8_t)
MAKE_SLICES(U8, uint8_t)
MAKE_SLICES(I16, int16_t)
MAKE_SLICES(U16, uint16_t)
MAKE_SLICES(I32, int32_t)
MAKE_SLICES(U32, uint32_t)
MAKE_SLICES(I64, int64_t)
MAKE_SLICES(U64, uint64_t)
MAKE_SLICES(Isize, intptr_t)
MAKE_SLICES(Usize, size_t)
MAKE_SLICES(F32, float)
MAKE_SLICES(F64, double)
MAKE_SLICES(Bool, bool)
MAKE_SLICES(Char, char32_t)
MAKE_SLICES(String, char)
MAKE_SLICES(U16String, char16_t)


#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif

#endif

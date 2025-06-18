#ifndef ICU4XTimeZoneIdMapper_H
#define ICU4XTimeZoneIdMapper_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XTimeZoneIdMapper ICU4XTimeZoneIdMapper;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XTimeZoneIdMapper_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XTimeZoneIdMapper_ICU4XError ICU4XTimeZoneIdMapper_create(const ICU4XDataProvider* provider);

diplomat_result_void_ICU4XError ICU4XTimeZoneIdMapper_iana_to_bcp47(const ICU4XTimeZoneIdMapper* self, const char* value_data, size_t value_len, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XTimeZoneIdMapper_normalize_iana(const ICU4XTimeZoneIdMapper* self, const char* value_data, size_t value_len, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XTimeZoneIdMapper_canonicalize_iana(const ICU4XTimeZoneIdMapper* self, const char* value_data, size_t value_len, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XTimeZoneIdMapper_find_canonical_iana_from_bcp47(const ICU4XTimeZoneIdMapper* self, const char* value_data, size_t value_len, DiplomatWriteable* write);
void ICU4XTimeZoneIdMapper_destroy(ICU4XTimeZoneIdMapper* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif

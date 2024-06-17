#ifndef ICU4XTimeZoneIdMapperWithFastCanonicalization_H
#define ICU4XTimeZoneIdMapperWithFastCanonicalization_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XTimeZoneIdMapperWithFastCanonicalization ICU4XTimeZoneIdMapperWithFastCanonicalization;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XTimeZoneIdMapperWithFastCanonicalization_ICU4XError.h"
#include "diplomat_result_void_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XTimeZoneIdMapperWithFastCanonicalization_ICU4XError ICU4XTimeZoneIdMapperWithFastCanonicalization_create(const ICU4XDataProvider* provider);

diplomat_result_void_ICU4XError ICU4XTimeZoneIdMapperWithFastCanonicalization_canonicalize_iana(const ICU4XTimeZoneIdMapperWithFastCanonicalization* self, const char* value_data, size_t value_len, DiplomatWriteable* write);

diplomat_result_void_ICU4XError ICU4XTimeZoneIdMapperWithFastCanonicalization_canonical_iana_from_bcp47(const ICU4XTimeZoneIdMapperWithFastCanonicalization* self, const char* value_data, size_t value_len, DiplomatWriteable* write);
void ICU4XTimeZoneIdMapperWithFastCanonicalization_destroy(ICU4XTimeZoneIdMapperWithFastCanonicalization* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif

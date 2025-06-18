#ifndef ICU4XPluralOperands_H
#define ICU4XPluralOperands_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XPluralOperands ICU4XPluralOperands;
#ifdef __cplusplus
} // namespace capi
#endif
#include "diplomat_result_box_ICU4XPluralOperands_ICU4XError.h"
#include "ICU4XFixedDecimal.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XPluralOperands_ICU4XError ICU4XPluralOperands_create_from_string(const char* s_data, size_t s_len);

ICU4XPluralOperands* ICU4XPluralOperands_create_from_fixed_decimal(const ICU4XFixedDecimal* x);
void ICU4XPluralOperands_destroy(ICU4XPluralOperands* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif

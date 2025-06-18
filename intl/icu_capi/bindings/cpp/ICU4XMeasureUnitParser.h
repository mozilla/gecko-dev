#ifndef ICU4XMeasureUnitParser_H
#define ICU4XMeasureUnitParser_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XMeasureUnitParser ICU4XMeasureUnitParser;
#ifdef __cplusplus
} // namespace capi
#endif
#include "diplomat_result_box_ICU4XMeasureUnit_ICU4XError.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XMeasureUnit_ICU4XError ICU4XMeasureUnitParser_parse(const ICU4XMeasureUnitParser* self, const char* unit_id_data, size_t unit_id_len);
void ICU4XMeasureUnitParser_destroy(ICU4XMeasureUnitParser* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif

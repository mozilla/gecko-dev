#ifndef ICU4XUnitsConverterFactory_H
#define ICU4XUnitsConverterFactory_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XUnitsConverterFactory ICU4XUnitsConverterFactory;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XDataProvider.h"
#include "diplomat_result_box_ICU4XUnitsConverterFactory_ICU4XError.h"
#include "ICU4XMeasureUnit.h"
#include "ICU4XUnitsConverter.h"
#include "ICU4XMeasureUnitParser.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

diplomat_result_box_ICU4XUnitsConverterFactory_ICU4XError ICU4XUnitsConverterFactory_create(const ICU4XDataProvider* provider);

ICU4XUnitsConverter* ICU4XUnitsConverterFactory_converter(const ICU4XUnitsConverterFactory* self, const ICU4XMeasureUnit* from, const ICU4XMeasureUnit* to);

ICU4XMeasureUnitParser* ICU4XUnitsConverterFactory_parser(const ICU4XUnitsConverterFactory* self);
void ICU4XUnitsConverterFactory_destroy(ICU4XUnitsConverterFactory* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif

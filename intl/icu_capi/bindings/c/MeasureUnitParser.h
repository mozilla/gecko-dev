#ifndef MeasureUnitParser_H
#define MeasureUnitParser_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DataError.d.h"
#include "DataProvider.d.h"
#include "MeasureUnit.d.h"

#include "MeasureUnitParser.d.h"






MeasureUnitParser* icu4x_MeasureUnitParser_create_mv1(void);

typedef struct icu4x_MeasureUnitParser_create_with_provider_mv1_result {union {MeasureUnitParser* ok; DataError err;}; bool is_ok;} icu4x_MeasureUnitParser_create_with_provider_mv1_result;
icu4x_MeasureUnitParser_create_with_provider_mv1_result icu4x_MeasureUnitParser_create_with_provider_mv1(const DataProvider* provider);

MeasureUnit* icu4x_MeasureUnitParser_parse_mv1(const MeasureUnitParser* self, DiplomatStringView unit_id);

void icu4x_MeasureUnitParser_destroy_mv1(MeasureUnitParser* self);





#endif // MeasureUnitParser_H

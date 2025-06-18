#ifndef ICU4XCollatorResolvedOptionsV1_H
#define ICU4XCollatorResolvedOptionsV1_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ICU4XCollatorStrength.h"
#include "ICU4XCollatorAlternateHandling.h"
#include "ICU4XCollatorCaseFirst.h"
#include "ICU4XCollatorMaxVariable.h"
#include "ICU4XCollatorCaseLevel.h"
#include "ICU4XCollatorNumeric.h"
#include "ICU4XCollatorBackwardSecondLevel.h"
#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XCollatorResolvedOptionsV1 {
    ICU4XCollatorStrength strength;
    ICU4XCollatorAlternateHandling alternate_handling;
    ICU4XCollatorCaseFirst case_first;
    ICU4XCollatorMaxVariable max_variable;
    ICU4XCollatorCaseLevel case_level;
    ICU4XCollatorNumeric numeric;
    ICU4XCollatorBackwardSecondLevel backward_second_level;
} ICU4XCollatorResolvedOptionsV1;
#ifdef __cplusplus
} // namespace capi
#endif
#include "ICU4XCollatorStrength.h"
#include "ICU4XCollatorAlternateHandling.h"
#include "ICU4XCollatorCaseFirst.h"
#include "ICU4XCollatorMaxVariable.h"
#include "ICU4XCollatorCaseLevel.h"
#include "ICU4XCollatorNumeric.h"
#include "ICU4XCollatorBackwardSecondLevel.h"
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XCollatorResolvedOptionsV1_destroy(ICU4XCollatorResolvedOptionsV1* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif

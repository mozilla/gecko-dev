#ifndef ICU4XWeekendContainsDay_H
#define ICU4XWeekendContainsDay_H
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#ifdef __cplusplus
namespace capi {
#endif

typedef struct ICU4XWeekendContainsDay {
    bool monday;
    bool tuesday;
    bool wednesday;
    bool thursday;
    bool friday;
    bool saturday;
    bool sunday;
} ICU4XWeekendContainsDay;
#ifdef __cplusplus
} // namespace capi
#endif
#ifdef __cplusplus
namespace capi {
extern "C" {
#endif

void ICU4XWeekendContainsDay_destroy(ICU4XWeekendContainsDay* self);

#ifdef __cplusplus
} // extern "C"
} // namespace capi
#endif
#endif

#define VERSION "1.5.1"
#define BUILD "2016-09-20"
#define PACKAGE_NAME "libjpeg-turbo"

/* Need to use Mozilla-specific function inlining. */
#include "mozilla/Attributes.h"
#define INLINE MOZ_ALWAYS_INLINE

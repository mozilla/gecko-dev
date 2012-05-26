/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include "mozilla/Types.h"

#ifdef ANDROID
#define wrap(a) __wrap_ ## a

/* operator new wrapper implementation */
static void *
new(unsigned int size)
{
  return malloc(size);
}
/* operator new(unsigned int) */
MOZ_EXPORT_API(void *)
wrap(_Znwj)(unsigned int) __attribute__((alias("new")));
/* operator new[](unsigned int) */
MOZ_EXPORT_API(void *)
wrap(_Znaj)(unsigned int) __attribute__((alias("new")));

/* operator delete wrapper implementation */
static void
delete(void *ptr)
{
  free(ptr);
}
/* operator delete(void*) */
MOZ_EXPORT_API(void)
wrap(_ZdlPv)(void *ptr) __attribute__((alias("delete")));
/* operator delete[](void*) */
MOZ_EXPORT_API(void)
wrap(_ZdaPv)(void *ptr) __attribute__((alias("delete")));
#endif

#if defined(XP_WIN) || defined(XP_MACOSX)
#define wrap(a) je_ ## a
#endif

#ifdef wrap
void *wrap(malloc)(size_t);

MOZ_EXPORT_API(char *)
wrap(strndup)(const char *src, size_t len)
{
  char* dst = (char*) wrap(malloc)(len + 1);
  if (dst)
    strncpy(dst, src, len + 1);
  return dst; 
}

MOZ_EXPORT_API(char *)
wrap(strdup)(const char *src)
{
  size_t len = strlen(src);
  return wrap(strndup)(src, len);
}
#endif

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

#include <string.h>

/*
** fprintf to a PRFileDesc
*/
PR_IMPLEMENT(PRUint32) PR_fprintf(PRFileDesc* fd, const char* fmt, ...) {
  va_list ap;
  PRUint32 rv;

  va_start(ap, fmt);
  rv = PR_vfprintf(fd, fmt, ap);
  va_end(ap);
  return rv;
}

PR_IMPLEMENT(PRUint32)
PR_vfprintf(PRFileDesc* fd, const char* fmt, va_list ap) {
  /* XXX this could be better */
  PRUint32 rv, len;
  char* msg = PR_vsmprintf(fmt, ap);
  if (NULL == msg) {
    return -1;
  }
  len = strlen(msg);
  rv = PR_Write(fd, msg, len);
  PR_DELETE(msg);
  return rv;
}

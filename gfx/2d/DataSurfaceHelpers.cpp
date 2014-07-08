/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "2D.h"
#include "DataSurfaceHelpers.h"
#include "Logging.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/MathAlgorithms.h"

namespace mozilla {
namespace gfx {

size_t
BufferSizeFromStrideAndHeight(int32_t aStride,
                              int32_t aHeight,
                              int32_t aExtraBytes)
{
  if (MOZ_UNLIKELY(aHeight <= 0)) {
    return 0;
  }

  // We limit the length returned to values that can be represented by int32_t
  // because we don't want to allocate buffers any bigger than that. This
  // allows for a buffer size of over 2 GiB which is already rediculously
  // large and will make the process janky. (Note the choice of the signed type
  // is deliberate because we specifically don't want the returned value to
  // overflow if someone stores the buffer length in an int32_t variable.)

  CheckedInt32 requiredBytes =
    CheckedInt32(aStride) * CheckedInt32(aHeight) + CheckedInt32(aExtraBytes);
  if (MOZ_UNLIKELY(!requiredBytes.isValid())) {
    gfxWarning() << "Buffer size too big; returning zero";
    return 0;
  }
  return requiredBytes.value();
}

}
}


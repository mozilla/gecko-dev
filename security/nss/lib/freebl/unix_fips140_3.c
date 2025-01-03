/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "secerr.h"
#include "secrng.h"
#include "prprf.h"
#include <sys/random.h>
#include "prinit.h"

#ifndef GRND_RANDOM
/* if you don't have get random, you'll need to add your platform specific
 * support for FIPS 104-3 compliant random seed source here */
PR_STATIC_ASSERT("You'll need to add our platform specific solution for FIPS 140-3 RNG" == NULL);
#endif

/* syscall getentropy() is limited to retrieving 256 bytes */
#define GETENTROPY_MAX_BYTES 256

void
RNG_SystemInfoForRNG(void)
{
    PRUint8 bytes[SYSTEM_RNG_SEED_COUNT];
    size_t numBytes = RNG_SystemRNG(bytes, SYSTEM_RNG_SEED_COUNT);
    if (!numBytes) {
        /* error is set */
        return;
    }
    RNG_RandomUpdate(bytes, numBytes);
    PORT_SaveZero(bytes, sizeof(bytes));
}

static unsigned int rng_grndFlags = 0;
static PRCallOnceType rng_KernelFips;

static PRStatus
rng_getKernelFips()
{
    if (NSS_GetSystemFIPSEnabled()) {
        rng_grndFlags = GRND_RANDOM;
    }
    return PR_SUCCESS;
}

size_t
RNG_SystemRNG(void *dest, size_t maxLen)
{

    size_t fileBytes = 0;
    unsigned char *buffer = dest;
    ssize_t result;

    PR_CallOnce(&rng_KernelFips, rng_getKernelFips);

    while (fileBytes < maxLen) {
        size_t getBytes = maxLen - fileBytes;
        if (getBytes > GETENTROPY_MAX_BYTES) {
            getBytes = GETENTROPY_MAX_BYTES;
        }
        /* FIP 140-3 requires full kernel reseeding for chained entropy sources
         * so we need to use getrandom with GRND_RANDOM.
         * getrandom returns -1 on failure, otherwise returns
         * the number of bytes, which can be less than getBytes */
        result = getrandom(buffer, getBytes, rng_grndFlags);
        if (result < 0) {
            break;
        }
        fileBytes += result;
        buffer += result;
    }
    if (fileBytes == maxLen) { /* success */
        return maxLen;
    }
    /* in FIPS 104-3 we don't fallback, just fail */
    PORT_SetError(SEC_ERROR_NEED_RANDOM);
    return 0;
}

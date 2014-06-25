/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Xdr.h"

#include <string.h>

#include "jsapi.h"
#include "jsscript.h"

#include "vm/Debugger.h"

using namespace js;

void
XDRBuffer::freeBuffer()
{
    js_free(base);
#ifdef DEBUG
    memset(this, 0xe2, sizeof *this);
#endif
}

bool
XDRBuffer::grow(size_t n)
{
    JS_ASSERT(n > size_t(limit - cursor));

    const size_t MEM_BLOCK = 8192;
    size_t offset = cursor - base;
    size_t newCapacity = JS_ROUNDUP(offset + n, MEM_BLOCK);
    if (isUint32Overflow(newCapacity)) {
        js::gc::AutoSuppressGC suppressGC(cx());
        JS_ReportErrorNumber(cx(), js_GetErrorMessage, nullptr, JSMSG_TOO_BIG_TO_ENCODE);
        return false;
    }

    void *data = js_realloc(base, newCapacity);
    if (!data) {
        js_ReportOutOfMemory(cx());
        return false;
    }
    base = static_cast<uint8_t *>(data);
    cursor = base + offset;
    limit = base + newCapacity;
    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeChars(const Latin1Char *chars, size_t nchars)
{
    static_assert(sizeof(Latin1Char) == sizeof(uint8_t), "Latin1Char must fit in 1 byte");

    MOZ_ASSERT(mode == XDR_ENCODE);

    uint8_t *ptr = buf.write(nchars);
    if (!ptr)
        return false;

    mozilla::PodCopy(ptr, chars, nchars);
    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeChars(jschar *chars, size_t nchars)
{
    size_t nbytes = nchars * sizeof(jschar);
    if (mode == XDR_ENCODE) {
        uint8_t *ptr = buf.write(nbytes);
        if (!ptr)
            return false;
        mozilla::NativeEndian::copyAndSwapToLittleEndian(ptr, chars, nchars);
    } else {
        const uint8_t *ptr = buf.read(nbytes);
        mozilla::NativeEndian::copyAndSwapFromLittleEndian(chars, ptr, nchars);
    }
    return true;
}

template<XDRMode mode>
static bool
VersionCheck(XDRState<mode> *xdr)
{
    uint32_t bytecodeVer;
    if (mode == XDR_ENCODE)
        bytecodeVer = XDR_BYTECODE_VERSION;

    if (!xdr->codeUint32(&bytecodeVer))
        return false;

    if (mode == XDR_DECODE && bytecodeVer != XDR_BYTECODE_VERSION) {
        /* We do not provide binary compatibility with older scripts. */
        JS_ReportErrorNumber(xdr->cx(), js_GetErrorMessage, nullptr, JSMSG_BAD_SCRIPT_MAGIC);
        return false;
    }

    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeFunction(MutableHandleObject objp)
{
    if (mode == XDR_DECODE)
        objp.set(nullptr);

    if (!VersionCheck(this))
        return false;

    return XDRInterpretedFunction(this, NullPtr(), NullPtr(), objp);
}

template<XDRMode mode>
bool
XDRState<mode>::codeScript(MutableHandleScript scriptp)
{
    if (mode == XDR_DECODE)
        scriptp.set(nullptr);

    if (!VersionCheck(this))
        return false;

    if (!XDRScript(this, NullPtr(), NullPtr(), NullPtr(), scriptp))
        return false;

    return true;
}

template<XDRMode mode>
bool
XDRState<mode>::codeConstValue(MutableHandleValue vp)
{
    return XDRScriptConst(this, vp);
}

XDRDecoder::XDRDecoder(JSContext *cx, const void *data, uint32_t length,
                       JSPrincipals *originPrincipals)
  : XDRState<XDR_DECODE>(cx)
{
    buf.setData(data, length);
    this->originPrincipals_ = originPrincipals;
}

template class js::XDRState<XDR_ENCODE>;
template class js::XDRState<XDR_DECODE>;

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StructuredCloneUtils.h"

#include "nsIDOMDOMException.h"
#include "nsIMutable.h"
#include "nsIXPConnect.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/File.h"
#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "MainThreadUtils.h"
#include "StructuredCloneTags.h"
#include "jsapi.h"

using namespace mozilla::dom;

namespace {

void
Error(JSContext* aCx, uint32_t aErrorId)
{
  if (NS_IsMainThread()) {
    NS_DOMStructuredCloneError(aCx, aErrorId);
  } else {
    Throw(aCx, NS_ERROR_DOM_DATA_CLONE_ERR);
  }
}

JSObject*
Read(JSContext* aCx, JSStructuredCloneReader* aReader, uint32_t aTag,
     uint32_t aData, void* aClosure)
{
  MOZ_ASSERT(aClosure);

  StructuredCloneClosure* closure =
    static_cast<StructuredCloneClosure*>(aClosure);

  if (aTag == SCTAG_DOM_BLOB) {
    // nsRefPtr<File> needs to go out of scope before toObjectOrNull() is
    // called because the static analysis thinks dereferencing XPCOM objects
    // can GC (because in some cases it can!), and a return statement with a
    // JSObject* type means that JSObject* is on the stack as a raw pointer
    // while destructors are running.
    JS::Rooted<JS::Value> val(aCx);
    {
      MOZ_ASSERT(aData < closure->mBlobImpls.Length());
      nsRefPtr<BlobImpl> blobImpl = closure->mBlobImpls[aData];

#ifdef DEBUG
      {
        // Blob should not be mutable.
        bool isMutable;
        MOZ_ASSERT(NS_SUCCEEDED(blobImpl->GetMutable(&isMutable)));
        MOZ_ASSERT(!isMutable);
      }
#endif

      // Let's create a new blob with the correct parent.
      nsIGlobalObject *global = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
      MOZ_ASSERT(global);

      nsRefPtr<Blob> newBlob = Blob::Create(global, blobImpl);
      if (!ToJSValue(aCx, newBlob, &val)) {
        return nullptr;
      }
    }

    return &val.toObject();
  }

  return NS_DOMReadStructuredClone(aCx, aReader, aTag, aData, nullptr);
}

bool
Write(JSContext* aCx, JSStructuredCloneWriter* aWriter,
      JS::Handle<JSObject*> aObj, void* aClosure)
{
  MOZ_ASSERT(aClosure);

  StructuredCloneClosure* closure =
    static_cast<StructuredCloneClosure*>(aClosure);

  // See if the wrapped native is a File/Blob.
  {
    Blob* blob = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob)) &&
        NS_SUCCEEDED(blob->SetMutable(false)) &&
        JS_WriteUint32Pair(aWriter, SCTAG_DOM_BLOB,
                           closure->mBlobImpls.Length())) {
      closure->mBlobImpls.AppendElement(blob->Impl());
      return true;
    }
  }

  return NS_DOMWriteStructuredClone(aCx, aWriter, aObj, nullptr);
}

const JSStructuredCloneCallbacks gCallbacks = {
  Read,
  Write,
  Error,
  nullptr,
  nullptr,
  nullptr
};

} // anonymous namespace

namespace mozilla {
namespace dom {

bool
ReadStructuredClone(JSContext* aCx, uint64_t* aData, size_t aDataLength,
                    const StructuredCloneClosure& aClosure,
                    JS::MutableHandle<JS::Value> aClone)
{
  void* closure = &const_cast<StructuredCloneClosure&>(aClosure);
  return !!JS_ReadStructuredClone(aCx, aData, aDataLength,
                                  JS_STRUCTURED_CLONE_VERSION, aClone,
                                  &gCallbacks, closure);
}

bool
WriteStructuredClone(JSContext* aCx, JS::Handle<JS::Value> aSource,
                     JSAutoStructuredCloneBuffer& aBuffer,
                     StructuredCloneClosure& aClosure)
{
  return aBuffer.write(aCx, aSource, &gCallbacks, &aClosure);
}

} // namespace dom
} // namespace mozilla

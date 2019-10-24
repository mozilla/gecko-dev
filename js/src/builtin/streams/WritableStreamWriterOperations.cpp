/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream writer abstract operations. */

#include "builtin/streams/WritableStreamWriterOperations.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"        // JS_ReportErrorNumberASCII
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_*

#include "builtin/Promise.h"                          // js::PromiseObject
#include "builtin/streams/MiscellaneousOperations.h"  // js::PromiseRejectedWithPendingError
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultController.h"  // js::WritableStream::controller
#include "builtin/streams/WritableStreamDefaultControllerOperations.h"  // js::WritableStreamDefaultController{Close,GetDesiredSize}
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::WritableStreamDefaultWriter
#include "builtin/streams/WritableStreamOperations.h"  // js::WritableStreamCloseQueuedOrInFlight
#include "js/Value.h"  // JS::Value, JS::{Int32,Null}Value
#include "vm/Compartment.h"  // JS::Compartment
#include "vm/JSContext.h"    // JSContext

#include "builtin/streams/MiscellaneousOperations-inl.h"  // js::ResolveUnwrappedPromiseWithUndefined
#include "builtin/streams/WritableStream-inl.h"  // js::WritableStream::setCloseRequest
#include "builtin/streams/WritableStreamDefaultWriter-inl.h"  // js::UnwrapStreamFromWriter
#include "vm/Compartment-inl.h"  // js::UnwrapAndTypeCheckThis
#include "vm/JSContext-inl.h"    // JSContext::check
#include "vm/Realm-inl.h"        // js::AutoRealm

using JS::Handle;
using JS::Int32Value;
using JS::MutableHandle;
using JS::NullValue;
using JS::NumberValue;
using JS::Rooted;
using JS::Value;

using js::PromiseObject;

/*** 4.6. Writable stream writer abstract operations ************************/

/**
 * Streams spec, 4.6.3.
 * WritableStreamDefaultWriterClose ( writer )
 */
JSObject* js::WritableStreamDefaultWriterClose(
    JSContext* cx, Handle<WritableStreamDefaultWriter*> unwrappedWriter) {
  // Step 1: Let stream be writer.[[ownerWritableStream]].
  // Step 2: Assert: stream is not undefined.
  MOZ_ASSERT(unwrappedWriter->hasStream());
  Rooted<WritableStream*> unwrappedStream(
      cx, UnwrapStreamFromWriter(cx, unwrappedWriter));
  if (!unwrappedStream) {
    return PromiseRejectedWithPendingError(cx);
  }

  // Step 3: Let state be stream.[[state]].
  // Step 4: If state is "closed" or "errored", return a promise rejected with a
  //         TypeError exception.
  if (unwrappedStream->closed() || unwrappedStream->errored()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAM_CLOSED_OR_ERRORED);
    return PromiseRejectedWithPendingError(cx);
  }

  // Step 5: Assert: state is "writable" or "erroring".
  MOZ_ASSERT(unwrappedStream->writable() ^ unwrappedStream->erroring());

  // Step 6: Assert: ! WritableStreamCloseQueuedOrInFlight(stream) is false.
  MOZ_ASSERT(!WritableStreamCloseQueuedOrInFlight(unwrappedStream));

  // Step 7: Let promise be a new promise.
  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return nullptr;
  }

  // Step 8: Set stream.[[closeRequest]] to promise.
  {
    AutoRealm ar(cx, unwrappedStream);
    Rooted<JSObject*> closeRequest(cx, promise);
    if (!cx->compartment()->wrap(cx, &closeRequest)) {
      return nullptr;
    }

    unwrappedStream->setCloseRequest(closeRequest);
  }

  // Step 9: If stream.[[backpressure]] is true and state is "writable", resolve
  //         writer.[[readyPromise]] with undefined.
  if (unwrappedStream->backpressure() && unwrappedStream->writable()) {
    if (!ResolveUnwrappedPromiseWithUndefined(
            cx, unwrappedWriter->readyPromise())) {
      return nullptr;
    }
  }

  // Step 10: Perform
  //          ! WritableStreamDefaultControllerClose(
  //              stream.[[writableStreamController]]).
  Rooted<WritableStreamDefaultController*> unwrappedController(
      cx, unwrappedStream->controller());
  if (!WritableStreamDefaultControllerClose(cx, unwrappedController)) {
    return nullptr;
  }

  // Step 11: Return promise.
  return promise;
}

/**
 * Streams spec, 4.6.7.
 * WritableStreamDefaultWriterGetDesiredSize ( writer )
 */
bool js::WritableStreamDefaultWriterGetDesiredSize(
    JSContext* cx, Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    MutableHandle<Value> size) {
  // Step 1: Let stream be writer.[[ownerWritableStream]].
  const WritableStream* unwrappedStream =
      UnwrapStreamFromWriter(cx, unwrappedWriter);
  if (!unwrappedStream) {
    return false;
  }

  // Step 2: Let state be stream.[[state]].
  // Step 3: If state is "errored" or "erroring", return null.
  if (unwrappedStream->errored() || unwrappedStream->erroring()) {
    size.setNull();
  }
  // Step 4: If state is "closed", return 0.
  else if (unwrappedStream->closed()) {
    size.setInt32(0);
  }
  // Step 5: Return
  //         ! WritableStreamDefaultControllerGetDesiredSize(
  //             stream.[[writableStreamController]]).
  else {
    size.setNumber(WritableStreamDefaultControllerGetDesiredSize(
        unwrappedStream->controller()));
  }

  return true;
}

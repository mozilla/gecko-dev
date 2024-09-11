/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Explicit Resource Management Proposal
// DisposeResources ( disposeCapability, completion )
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-disposeresources
//
// This implementation of DisposeResources is specifically for
// AsyncDisposableStack and called from the disposeAsync method of the same.
//
// TODO: Consider unifying the implementation of DisposeResources by introducing
// a special call like syntax that directly generates bytecode (Bug 1917491).
async function DisposeResources(disposeCapability) {

  var hadError = false;
  var latestException = undefined;

  // Step 1. Let needsAwait be false.
  var needsAwait = false;

  // Step 2. Let hasAwaited be false.
  var hasAwaited = false;

  // Step 3. For each element resource of
  // disposeCapability.[[DisposableResourceStack]], in reverse list order, do
  var index = disposeCapability.length - 1;
  while (index >= 0) {
    var resource = disposeCapability[index--];

    // Step 3.a. Let value be resource.[[ResourceValue]].
    var value = resource.value;

    // Step 3.b. Let hint be resource.[[Hint]].
    var hint = resource.hint;
    assert(hint === USING_HINT_ASYNC, "expected async-dispose hint for AsyncDisposableStack");

    // Step 3.c. Let method be resource.[[DisposeMethod]].
    var method = resource.method;

    // Step 3.e. If method is not undefined, then
    if (method !== undefined) {
      var result;
      try {
        // Step 3.e.i. Let result be Completion(Call(method, value)).
        result = callContentFunction(method, value);

        // Step 3.e.ii. If result is a normal completion and hint is
        // async-dispose, then
        // (implicit since AsyncDisposableStack has only async-dispose)
        // Step 3.e.ii.1. Set result to Completion(Await(result.[[Value]])).
        await result;

        // Step 3.e.ii.2. Set hasAwaited to true.
        hasAwaited = true;
      } catch (e) {
        // Step 3.e.iii. If result is a throw completion, then

        // Step 3.e.iii.1. If completion is a throw completion, then
        if (hadError) {
          // Steps 3.e.iii.1.a-f
          latestException = CreateSuppressedError(e, latestException);
        } else {
          // Step 3.e.iii.2. Else,
          // Step 3.e.iii.2.a. Set completion to result.
          latestException = e;
          hadError = true;
        }
      }
    } else {
      // Step 3.f. Else,
      // Step 3.f.ii. Set needsAwait to true.
      needsAwait = true;
    }
  }

  // Step 4. If needsAwait is true and hasAwaited is false, then
  if (needsAwait && !hasAwaited) {
    // Step 4.a. Perform ! Await(undefined).
    await undefined;
  }

  // Step 6. Set disposeCapability.[[DisposableResourceStack]] to a
  // new empty List.
  // (done by the caller)

  // Step 7. Return ? completion.
  if (hadError) {
    throw latestException;
  }
}

// Explicit Resource Management
// 27.4.3.3 AsyncDisposableStack.prototype.disposeAsync ( )
// https://arai-a.github.io/ecma262-compare/?pr=3000&id=sec-asyncdisposablestack.prototype.disposeAsync
async function $AsyncDisposableStackDisposeAsync() {
  // Step 1. Let asyncDisposableStack be the this value.
  var asyncDisposableStack = this;

  if (!IsObject(asyncDisposableStack) || (asyncDisposableStack = GuardToAsyncDisposableStackHelper(asyncDisposableStack)) === null) {
    return callFunction(
      CallAsyncDisposableStackMethodIfWrapped,
      this,
      "$AsyncDisposableStackDisposeAsync"
    );
  }

  // Step 2. Let promiseCapability be ! NewPromiseCapability(%Promise%).
  // (implicit)
  // Step 3. If asyncDisposableStack does not have an [[AsyncDisposableState]] internal slot, then
  var state = UnsafeGetReservedSlot(asyncDisposableStack, DISPOSABLE_STACK_STATE_SLOT);
  if (state === undefined) {
    // Step 3.a. Perform ! Call(promiseCapability.[[Reject]], undefined, « a newly created TypeError object »).
    // Step 3.b. Return promiseCapability.[[Promise]].
    // (implicit)
    ThrowTypeError(JSMSG_INCOMPATIBLE_METHOD, 'disposeAsync', 'method', 'AsyncDisposableStack');
  }

  // Step 4. If asyncDisposableStack.[[AsyncDisposableState]] is disposed, then
  if (state === DISPOSABLE_STACK_STATE_DISPOSED) {
    // Step 4.a. Perform ! Call(promiseCapability.[[Resolve]], undefined, « undefined »).
    // Step 4.b. Return promiseCapability.[[Promise]].
    return undefined;
  }

  // Step 5. Set asyncDisposableStack.[[AsyncDisposableState]] to disposed.
  UnsafeSetReservedSlot(asyncDisposableStack, DISPOSABLE_STACK_STATE_SLOT, DISPOSABLE_STACK_STATE_DISPOSED);

  // Step 6. Let result be Completion(DisposeResources(asyncDisposableStack.[[DisposeCapability]], NormalCompletion(undefined))).
  // Step 7. IfAbruptRejectPromise(result, promiseCapability).
  var disposeCapability = UnsafeGetReservedSlot(asyncDisposableStack, DISPOSABLE_STACK_DISPOSABLE_RESOURCE_STACK_SLOT);
  UnsafeSetReservedSlot(asyncDisposableStack, DISPOSABLE_STACK_DISPOSABLE_RESOURCE_STACK_SLOT, undefined);
  if (disposeCapability === undefined) {
    return undefined;
  }
  // TODO: This is technically not equivalent to the spec, this can cause additional
  // promise reactions, figure out a test case that can observe this effect.
  // (Bug 1917486)
  await DisposeResources(disposeCapability);

  // Step 8. Perform ! Call(promiseCapability.[[Resolve]], undefined, « result »).
  // Step 9. Return promiseCapability.[[Promise]].
  return undefined;
}
SetCanonicalName($AsyncDisposableStackDisposeAsync, 'disposeAsync');

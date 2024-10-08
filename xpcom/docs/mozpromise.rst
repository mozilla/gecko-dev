MozPromise: C++ promises in Gecko
=================================

.. contents:: Table of Contents
    :depth: 2
    :local:
    :backlinks: none

MozPromise is a powerful and flexible promise implementation in C++ designed to
manage asynchronous operations within Gecko. Its set of feature mirror the
spirit of JavaScript promises, including the ability to attach resolve and
reject callbacks, chain promises, and handle asynchronous tasks across different
threads. ``MozPromise`` supports both exclusive and non-exclusive promises.
Exclusive promises enforce that there is at most one call to either
``Then(...)``, ensuring that the promise is used in a
predictable and controlled manner. Non-exclusive promises, on the other hand,
allow multiple resolve or reject operations, providing flexibility for more
complex use cases.

Additionally, ``MozPromise`` offers mechanisms for disconnecting promises,
allowing consumers to cancel the delivery of resolve or reject values when they
are no longer needed. These features make ``MozPromise`` a versatile tool for
managing asynchronous workflows spanning multiple threads, ensuring thread
safety, and maintaining the correct sequence of operations in complex
applications, in contrast to the usual pattern seen in the Gecko code base,
which is dispatching ``Runnable`` manually.

Another option for syncing state across threads is to use `State Mirroring <https://searchfox.org/mozilla-central/source/xpcom/threads/StateMirroring.h>`_.

``MozPromise`` aren't really related to the DOM ``Promise`` class, but can be often used in
conjunction. This is done manually by for example calling
``dom::Promise::MaybeResolve`` from a lambda passed to the ``Then(...)`` of a
``MozPromise``, on the main thread.

Throughout this document, the **producer** is the piece of code that creates,
then resolves or rejects a promise, and typically starts or does the work
that needs to happen as part of the asynchronous operation. The **consumer**
is the piece of code that will react to the promise being resolved or rejected,
and therefore gets to know about the completion of the work.

Guarantees of MozPromise
~~~~~~~~~~~~~~~~~~~~~~~~

``MozPromise`` provides several guarantees to ensure predictable and reliable
behavior. These include:

- **Ordering**: ``MozPromise`` ensures that the resolve or reject callbacks are
  dispatched in the order they are attached, maintaining the correct sequence of
  operations. Attaching callbacks after a promise has completed will call them.
- **Thread Safety**: They are designed to be thread-safe, allowing promises to
  be created, chained, resolved, and rejected on different threads without
  causing race conditions. The resolve/reject handlers are always destroyed
  on their target threads. ``MozPromiseHolder`` and ``MozPromiseRequestHolder``
  themselves need to be synchronized however.
- **Completion**: Once a promise is resolved or rejected, it cannot be changed,
  ensuring that the state of the promise is consistent and immutable. ``*Holder``
  instances can be reused.

Typical uses
~~~~~~~~~~~~

If the workload is synchronous:

From the producer side:
 - Do the work
 - Return a resolved or rejected promise via ``MozPromise::CreateAndResolve``
   or ``MozPromise::CreateAndReject``

From the consumer side:
 - Call the method returning a promise
 - Call ``Then()`` on the promise to set the actions to run on a given thread
   once the promise has settled.

If the workload is asynchronous:

From the producer side:
- Allocate a ``MozPromise`` (probably via a ``MozPromiseHolder``) and return
it to the consumer has a ``RefPtr<MozPromise>``
- Dispatch the async work to any thread with ``InvokeAsync`` forward its
returned promise to the caller.
- Once the work has been completed, return its result directly in the async
task using ``MozPromise::CreateAndResolve`` or ``MozPromise::CreateAndReject``.

From the consumer side:
- Call the method returning a ``MozPromise``
- Call ``Then()`` on the promise to set the actions to run on a given thread
once the promise has settled.

In both case, it is possible to  ``Track()`` the promise,  to cancel the delivery of
the resolve/reject result and prevent callbacks to be run.

Concepts
~~~~~~~~

Threads on which Promises Run
-----------------------------

Promises in Mozilla's ``MozPromise`` framework can run on various threads,
depending on the context in which they are created and resolved. The ``Then``
method allows specifying the target thread on which the resolve or reject
callbacks should be executed. ``InvokeAsync`` allows selecting the thread
the work will happen on, which commonly also is where the promise gets
rejected or resolved.

The ``Then(...)`` method
------------------------

This method is called to set reject/resolve callbacks to be called when a
promise has settled. There are two ways of specifying those. The first style is
to pass a single callback, using the type ``MyPromise::ResolveOrRejectValue``,
if ``MyPromise`` has been aliased to a particular type, and to handle
rejection / resolution by checking the value itself:

.. code-block:: c++

    RefPtr<MyPromise> promise = /* from somehere */

    promise->Then(mainThread, __func__,
        [this, self = RefPtr{this}]
        (const MyPromise::ResolveOrRejectValue& aValue) {
        if (aValue.IsResolve())) {
            /* HandleResolvedValue(value); */
            return;
        }
        /* HandleRejectedValue(); */
        });

The other style is by passing two callbacks, one for resolution (first parameter),
and one for rejection (second parameter):

.. code:: c++

    RefPtr<MyPromise> promise = /* from somewhere */

    // Granted those functions have the correct parameters types.
    promise->Then(
        mainThread, __func__, &HandleResolvedValue, &HandleRejectedValue);

The exact argument type to use in those callback depends on the exclusivity
of the promise, see section below.

When method pointers are passed in, a refcounted instance pointers is necessary
in the capture list. Both function pointers and lambda can be used.

The `Then(...)` method returns an object that can be used to do two things:

- Convert it back to a ``MozPromise``, that will be resolved once the resolve/reject
  of the first promise are called. This allow chaining multiple promises by in turn
  calling ``Then(...)`` on that converted promise.
- Track the first promise, to be able to cancel the delivery of the callbacks, if
  they haven't already been called. This is done by disconnecting a
  ``MozPromiseRequestHolder``.

Reference counting
------------------

Since promises are by essence asynchronous, and can run on various threads, it
is important to ensure that the objects that are going to be used in the callback
functions are still alive when the promise is rejected or resolved. This is typically
done by adding reference counting to a class, and by passing an addrefed copy of the
``this`` pointer into the lambdas, like so:

.. code:: c++

    class SomeClass {
    public:
        // Adding refcounting to a class:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SomeClass)

        RefPtr<MyPromise> DoIt() {
            RefPtr<MyPromise> promise = mHolder.Ensure(__func__);

            promise->Then(
                backgroundThread, __func__,
                // Make the lambda has keeping a reference to the class via
                // the capture list, by creating a new RefPtr in the lambda's
                // scope
                [this, self = RefPtr{this}](int value) {
                    /* handle resolution */
                },
                // Reject and resolve have the same lifetime, no need
                // to do anything here if we don't need a reference to this
                // (e.g. we're just logging, etc.)
                [](nsresult error) { /* HandleRejectedValue(error); */ });

            return promise.forget();
        }
    private:
        MozPromiseHolder<MyPromise> mHolder;
    };

Exclusivity
-----------

Exclusivity in ``MozPromise`` refers to the ability to enforce that a promise
resolves or rejects to a single set of callbacks. When the ``IsExclusive``
template argument is set to ``true``, the promise prevents multiple
resolution or rejection callbacks on a single promise when it is not a
feature that is desirable for a particular use, for instance when that
could lead to unexpected behavior.

This invariant is checked with an assertion, that is enabled in release builds,
and will fail when attempting to install the second set of callbacks on an exclusive
promise.

When a promise is exclusive, the result value is moved into the resolve callback using an
rvalue reference. A typical signature is therefore:

.. code:: c++

    using MyPromise = MozPromise<int, nsresult, true>;
    void Callback(MyPromise::ResolveOrRejectValue&& aResult);

This means that the callback is the sole owner of the value. The callback's closure
will be deleted on the thread they are called on. It follows that the types used
for rejection/resolution values need to be movable or copiable.

If however the promise isn't exclusive, the result is passed using a const
lvalue reference:

.. code:: c++

    void Callback(const MyPromise::ResolveOrRejectValue& aResult);

This allows multiple callbacks to have a reference to the value.

Main classes
~~~~~~~~~~~~

MozPromise
----------

``MozPromise`` is a template class that represents a promise in C++, similar to
JavaScript promises. It manages an asynchronous request that may or may not be
able to be fulfilled immediately. The template arguments for ``MozPromise`` are:

- ``ResolveValueT``: The type of the value that the promise resolves to.
- ``RejectValueT``: The type of the value that the promise rejects with.
- ``IsExclusive``: A boolean flag indicating whether the promise is exclusive,
   meaning it can only be resolved or rejected once.

Aliasing the promises types using ``typedef`` or ``using`` is a common practice to
simplify the usage of ``MozPromise`` with specific resolve and reject types. For
example:

.. code-block:: c++

    using CustomBoolPromise = MozPromise<bool, nsresult, true>;

defines a generic exclusive promise type that resolves to a boolean and rejects with an
``nsresult``.

This makes the code more readable and easier to maintain, as the specific types
of the promises are clearly defined and can be reused throughout the codebase.

MozPromiseHolder
----------------

``MozPromiseHolder`` is a template class designed to encapsulate a ``MozPromise``.
It is useful for classes whose methods return promises, i.e., the "inside" of
the asynchronous request: the part that will eventually resolve or reject.
It is suitable for advanced cases where ``InvokeAsync`` is not enough.

Typically, you store a ``MozPromiseHolder`` in a class that will return
promises to callers and internally resolve those promises. For good
measure a ``MozPromiseHolder`` shouldn't be leaked outside its owner
class or into nested classes, much like JS promise resolve/reject
functions shouldn't leak outside of the constructor scope.

``MozPromiseHolder`` provides methods to ensure a promise is created, check if it
is empty, steal the private promise, resolve or reject the promise, and set task
dispatching and priority. It allows managing promises **within** a
class, ensuring that the promise is properly handled and can be resolved or
rejected as needed. Note that ``MozPromiseHolder`` is not thread-safe in itself,
although the promise it encapsulates is.


.. code-block:: c++

    class SomeClass {
    public:
        RefPtr<MyPromise> DoIt() {
            RefPtr<MyPromise> promise = mHolder.Ensure(__func__);
            MOZ_ASSERT(!mHolder.IsEmpty());

            // ... deep inside some async code, potentially on a different thread,
            // resolve the promise via the holder:
            // mHolder.Resolve(42, __func__);
            // It is empty after resolving
            // MOZ_ASSERT(mHolder.IsEmpty());

            return promise.forget();
        }
    private:
        MozPromiseHolder<MyPromise> mHolder;
    };


MozPromise::Request / MozPromiseRequestHolder
---------------------------------------------

``MozPromiseRequestHolder`` is a template class that encapsulates a
``MozPromise::Request`` reference, that is rarely use directly.
It is used by classes which may want to disconnect from waiting on
a ``MozPromise``, i.e. the "outside" of the asynchronous request. This
class provides methods to track a request, complete it, disconnect it,
and check if it exists. It is useful for managing the lifecycle of a
promise request, ensuring that the request can be properly tracked,
completed, or disconnected as needed.

In essence, this is a handle on a particular request made with within the
``MozPromise`` framework.

Disconnecting a request **must** happen on the target thread of the resolve/reject
handler it is tracking. This handler is released when ``Disconnect()`` is called.

When dealing with ``MozPromise`` close to the WebIDL binding layer,
another option is `DOMMozPromiseRequestHolder <https://searchfox.org/mozilla-central/source/dom/base/DOMMozPromiseRequestHolder.h>`_,
that will disconnect promises appropriately when the global goes away.
It works in the same way otherwise.

To associate a ``MozPromiseRequestHolder`` with a ``MozPromise``, the
``Track(...)`` method is used:

.. code-block:: c++

    class SomeClass {
    public:
        // refcounting is mandatory
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SomeClass)
        RefPtr<MyPromise> DoIt() {
            RefPtr<MyPromise> promise = mHolder.Ensure(__func__);
            MOZ_ASSERT(!mHolder.IsEmpty());

            promise->Then(
                backgroundThread, __func__,
                [this, self = RefPtr{this}](int value) {
                  // Resolved: mark as complete
                  mRequestHandle.Complete();
                  /* do something with value */
                },
                [](nsresult error) {
                  // Rejected: also mark as complete
                  mRequestHandle.Complete();
                  /* HandleRejectedValue(error); */
            }).Track(mRequestHandle);

            // ... deep inside some async code, potentially on a different thread,
            // resolve the promise:
            // promise.Resolve(42, __func__);

            return promise.forget();
        }
        void CancelIt() {
            // Functions passed to Then() won't be called. This must
            // be called on `backgroundThread`
            mRequestHandle.DisconnectIfExists();
        }
    private:
        MozPromiseHolder<MyPromise> mHolder;
        MozPromiseRequestHolder<MyPromise> mRequestHandle;
    };

The InvokeAsync Function
------------------------

The ``InvokeAsync`` function is used to invoke a promise-returning function
asynchronously on a given thread. It dispatches a task to invoke the function on
the proper thread and also chains the resulting promise to the one that the
caller received, so that resolve/reject values are forwarded through. This
function is useful for scheduling asynchronous tasks that return promises,
ensuring that the tasks are executed on the correct thread and that the promises
are properly chained.

.. code-block:: c++

    class SomeClass {
        public:
        NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SomeClass)
        RefPtr<MyPromise> AsyncFunction(nsISerialEventTarget* target) {
            return InvokeAsync(target, __func__, []() -> RefPtr<MyPromise> {
                // ... some expensive async work is happening
                int result = 42;
                return MyPromise::CreateAndResolve(result, __func__);
            });
        }

        RefPtr<MyPromise> DoItAsync() {
            nsCOMPtr<nsISerialEventTarget> backgroundThread = /* from somewhere */;
            nsCOMPtr<nsISerialEventTarget> mainThread = do_GetMainThread();

            // Call the async function on the background task queue
            RefPtr<MyPromise> promise = AsyncFunction(backgroundThread);

            // But get the completion callbacks on the main thread
            promise->Then(
                mainThread, __func__,
                [this, self = RefPtr{this}](int value) {
                  /* HandleResolvedValue(value); */
                },
                [](nsresult error) {
                  /* HandleRejectedValue(error); */
            });

            return promise.forget());
        }
    };


Advanced features
~~~~~~~~~~~~~~~~~

Direct Task Dispatch
--------------------

`Direct task dispatch <https://searchfox.org/mozilla-central/source/xpcom/threads/nsIDirectTaskDispatcher.idl>`_
is a feature in ``MozPromise`` that allows the resolve or
reject callbacks to be executed on the direct task queue instead of the normal
event loop. This is particularly useful for scenarios where multiple
asynchronous steps are involved, as it avoids a full trip to the back of the
event queue for each additional asynchronous step. By using direct task
dispatch, the callbacks are executed more promptly, reducing latency and
improving the overall responsiveness of the application.

This is only available when the callbacks are set to run on the same
thread the caller is on.

In Web land, this would be akin to executing something in a microtask
checkpoint, and not a regular event loop task. While it is the default for Web
Promises, it is opt-in in ``MozPromise``.

To enable direct task dispatch, the ``UseDirectTaskDispatch`` method is called
on the ``MozPromiseHolder`` instance. This method sets the promise to use the
direct event queue for dispatching the resolve or reject callbacks.

A related concept is `"tail dispatching" <https://searchfox.org/mozilla-central/rev/9fa446ad77af13847a7da250135fc58b1a1bd5b9/xpcom/threads/AbstractThread.h#72-76>`_
of ``Runnable``.

Synchronous Dispatch
--------------------

Synchronous dispatch is another feature in MozPromise that allows the resolve or
reject callbacks to be executed synchronously on the same thread, rather than
being dispatched asynchronously. This is useful in scenarios where the callbacks
need to be executed immediately, without waiting for the event loop to process
them. Synchronous dispatch ensures that the callbacks are executed in a
predictable and timely manner, which can be crucial for certain types of
operations.

This is only available when the callbacks are set to run on the same
thread the caller is on.

To enable synchronous dispatch, the UseSynchronousTaskDispatch method is called
on the MozPromiseHolder instance. This method sets the promise to execute the
resolve or reject callbacks synchronously on the same thread. When the promise
is resolved or rejected, the callbacks are executed immediately, without being
dispatched to the event loop.

However, synchronous dispatch can introduce potential issues, such as deadlocks.
A deadlock occurs when two or more threads are waiting for each other to release
resources, resulting in a situation where neither thread can proceed. In the
context of MozPromise, a deadlock can occur if the resolve or reject callbacks
are waiting for a resource that is held by the same thread, causing the thread
to block indefinitely.

To mitigate the risk of deadlocks, it is important to use synchronous dispatch
judiciously and ensure that the callbacks do not depend on resources that are
held by the same thread.

Caveats
~~~~~~~

It is an error to destroy a promise that hasn't been resolved or rejected.
Teardown of an object owning a ``MozPromiseHolder`` is therefore going to
assert in this case.

When dealing with ``MozPromise`` (like most asynchronous constructs), the shutdown
phase can be a problem. Since there's no way to
handle the failure to dispatch to a thread, it's an error to have a promise chain
set to run some handler on a thread that may have shut down. One way to fix this is
to provide threading guarantees, by blocking shutdown, or to disconnect the promise
via a ``MozPromiseRequestHolder`` when shutting down. Both can possibly be needed.

When using ``MozPromiseHolder::Ensure``, a new ``MozPromise`` will be created even
if the previous one was already settled. Sometimes external bookkeeping (for example
keeping the ``MozPromise`` around to check if it's the same) is necessary to ensure that the
handlers are set on the correct ``MozPromise``, and not potentially another one.

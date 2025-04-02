# Category manager indirection (callModulesFromCategory)

Firefox front-end code uses the category manager as a publish/subscribe
mechanism for dependency injection, so consumers can be notified of interesting
things happening without having to directly talk to the publisher/actor who
decides the interesting thing is happening.

There are 2 parts to this:

1. Consumers registering with the category manager
2. Publishers/actors invoking consumers via the category manager.

## Consumer registration with the category manager.

The category manager is used for various purposes within Firefox; it is more or
less an arbitrary double string-keyed data store.

For this particular usecase, the publisher/consumer have to use the same
primary key (i.e. category name), such as `browser-idle-startup`.

The secondary key is a full URL to a `sys.mjs` module. Note that because this
is a key, **only one consumer per module & category combination is possible**.

The "value" part is an `Object.method` notation, where the expectation is that
`Object` is an exported symbol from the module identified as the secondary key,
and `method` is some method on that object.

At compile-time, registration can happen with an entry in a `.manifest` file
like [BrowserComponents.manifest](https://searchfox.org/mozilla-central/source/browser/components/BrowserComponents.manifest).
Note that any manifest successfully processed by the build system would do,
we don't need to use `BrowserComponents.manifest` specifically. In fact, it
would be preferable if components used their own manifest files.

An example registration looks like:

```
category browser-idle-startup moz-src://browser/components/tabbrowser/TabUnloader.sys.mjs TabUnloader.init
```

This will ensure that when the `browser-idle-startup` publisher is invoked,
the `TabUnloader.sys.mjs` module is loaded and the `init` method on the exported
`TabUnloader` object is invoked.

### Runtime registration

Runtime registration is less-often used, but can be done using the category
manager's XPCOM API:

```js
Services.catMan.addCategoryEntry(
    "browser-idle-startup",
    "moz-src://browser/components/tabbrowser/TabUnloader.sys.mjs",
    "TabUnloader.init"
)
```

## Publishers/actors invoking consumers

Publishers call `BrowserUtils.callModulesFromCategory` with a dictionary of
options as the first argument. If provided, any other arguments are passed
straight through to any consumers.

```{js:autofunction} BrowserUtils.callModulesFromCategory
```

Example:

```js
BrowserUtils.callModulesFromCategory({
    categoryName: "my-fancy-category-name",
    profilerMarker: "markMyCategories",
    idleDispatch: true,
    someArgument
});
```

This will pass `someArgument` to each consumer registered for
`my-fancy-category-name`. Each consumer will be invoked via an idle task, and
each task will get a profiler marker (labelled `"markMyCategories"`) in the
[Firefox Profiler](https://profiler.firefox.com/) so it's easy to find in
performance profiles.

You should consider using `idleDispatch: true` if invocation of the consumers
does not need to happen synchronously.

If you need to care about errors produced by consumers, you can specify
a function for `failureHandler` and handle any exceptions/errors using your own
logic. Note that it may be invoked asynchronously if the consumers are async.

## Caveats

Any errors thrown by consumers are automatically caught and reported via the
[Browser Console](/devtools-user/browser_console/index.rst).

Async functions are not awaited before invoking other consumers. Note that
rejections (exceptions from async code) are still caught and reported to the
console, and that the async duration of a given consumer will be what
determines the length of the profiler marker, if the publisher asks for profiler
markers.

## Why not just call consumers directly?

There are a number of benefits over direct method calls and module imports.

### Reducing direct dependencies between different parts of the code-base

Code that looks like this:

```
Foo.thingHappened(arg);
Bar.thingHappened(arg);
Baz.thingHappened(arg);
```

is not only repetitive, it also means that the code in question has to directly
import all the modules that provide `Foo`, `Bar` and `Baz`. It means that if
those modules change or move or are refactored, the "publisher" code has to
be updated, with all the added burdens that comes with (potential for merge
conflicts, more automatically added reviewer groups for trivial changes, easy
to miss if dependencies are widespread).

### Avoiding a bootstrap problem in favour of a "just in time" approach

To make sure code is invoked later, when using the observer service, DOM event
listeners, or other mechanisms, it usually needs to add a listener
before the event of interest happens. If not managed carefully, this often leads
to component initialization being front-loaded to make sure not to "miss" it
later. This in turn makes browser startup more heavyweight rather than it needs
to be, because we set up listeners for _everything_, potentially loading entire
JS modules just to do that.

### Unified error-handling, performance inspection, and scheduling

Using the `BrowserUtils.callModulesFromCategory` API allows specifying error
handling, performance profiler markers, and scheduling (use of idle tasks) in
one place. This abstracts away the fact that we never want observers, event
listeners or other mechanisms like this to break and stop notifying (or worse,
propagate an exception themselves) when one of the consumers breaks.

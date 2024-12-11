# Editing toolkit/moz.configure

## Prerequisites

Some of the files that configure the build system are written in a restricted python dialect. It is probably easiest to think of them as "python-like DSLs". They must be formatted using `black`. Correct formatting is checked on CI.

To run `black` on `toolkit/moz.configure`:

```
./mach lint -l black toolkit/moz.configure
```

## moz.configure

These files describe one of the first steps of the build. This step does not run tool chains or produce any other kind of artifacts. It only produces a few key/value dictionaries that later parts of the build will use.

Two important dictionaries declared in `moz.configure` are *configs* and *defines*. The former is used in `moz.build` files, the later is used to feed C and C++ compilers, as shown below.

This is typically the right place to add logic for:
- Declaring options for the mozconfig file.
- Deciding whether to enable/disable some build-time features based on the build configuration and environment.
- Generating some `#define` identifiers for the C++ code based on the build configuration or environment.

It contains a lot of code that looks like:

```python
# In toolkit/moz.configure:

# Adds a config key/value pair
set_config("FOO", foo)
# Adds a define key/value pair
set_define("BAR", bar)
```

We'll see later how the lower case `foo` symbol above is defined.
Configurations can be accessed in various parts of the build system, such as `moz.build` files for example:

```python
# In a moz.build file:

if CONFIG["FOO"]:
    # For example let's add an exported header for our C++ code.
    EXPORTS.mozilla += [
        "foo.h"
    ]

# or
if CONFIG["FOO"] == "something":
    # etc.
```

Defines map directly to C++ defines in the code as well as other files that use a C-like preprocessor, for example `modules/libref/init/all.js`, or `toolkit/content/license.html`.

### The dependency graph

It is tempting to look at the code in `moz.configure` and read its logic in with an imperative programming mindset, however a better mental model is to imagine this file as a script that declares a task graph which is evaluated later.

Let's look at a simple example:

```python
# In toolki/moz.configure

# Declare a build option that can be set via `ac_add_option` in the `mozconfig` file.
option("--enable-doodad", help="Enable a fancy feature")

@depends("--enable-doodad", target)
def doodad(enabled, target):
    # Return True if --enable-doodad was set in mozconfig and
    # if we are on Windows.
    return enabled and target.os =!== "WINNT"
```

The code above declares a `doodad` function that is decorated with `@depends`.

We will never directly call this `doodad` function ourselves. The `@depends`
decoration wraps it into a node of the dependency graph that will
be lazily evaluated later. Elsewhere in `moz.configure`, when we write `doodad`, it refers to the node that wraps the function.

The parameters in `@depends` correspond to `doodad`'s node dependency and map to the function parameters. So `enabled` inside the function will only evaluate to `True` if `--enable-doodad` is set in mozconfig.

The body of the function is evaluated in the second stage when the graph is evaluated. It runs in a sand-boxed environment and has access to very few things other than what is provided as input to the node.

Only declaring a node has no effect, unless that node is used, so let's use our `doodad` node:

```python
# Specify `doodad` as a dependency to resolving the "DOODAD" config key.
set_config("DOODAD", doodad)
# Specify a define. The syntax is the same as with `set_config`.
set_define("MOZ_DOODAD", 1, when=doodad)
```

Note the `when=` syntax: the define will only be set if doodad evaluates to `True`. This syntax can also be used with `set_config` and `@depends`.

Since `set_config` is run when declaring the graph, and before evaluating it, we could not have expressed this condition using an `if` statement:

```python
# This does *not* work. `doodad` is not a value, it is a node.
if doodad:
    set_define("MOZ_DOODAD", 1)
```

Another way to express this condition is via `with only_when` blocks:

```python
# This works!
with only_when(doodad):
    set_define("MOZ_DOODAD", 1)
```

Now let's add a slightly more complicated example. This time the node will not evaluate to

```python
with only_when(compile_environment):
    # Depend on the doodad node we defined earlier
    @depends(doodad, target)
    def advanced_doodad(basic_doodad, target):
        # If the doodad is not enabled, don't enable the advanced
        # version.
        if not basic_doodad:
            return Namespace(enabled=False)
        header_name = "doodad_" + target.cpu + ".h"
        return Namespace(
            enabled=True,
            header_name=header_name
        )

    with only_when(advanced_doodad.enabled):
        set_config("DOODAD_ARCH_HEADER", advanced_doodad.header_name)
```

The `advanced_doodad` node evaluates to a dictionary instead of just a boolean.

This is useful to write more expressive configurations and for, example, generate strings or path names based on earlier configuration.

# `{fmt}` in Gecko

[`{fmt}`](https://fmt.dev/) is a library implementation of C++20's [`std::format`](https://en.cppreference.com/w/cpp/header/format) formatting API which enables type-informed string formatting. Unlike `printf` this format string style does not require specifying the types of each format parameter in the format string. For example, instead of:

```c++
#include <mozilla/Sprintf.h>
#include <inttypes.h>
// ...
char buf[1024];
int64_t a = 123;
uint64_t a = 456;
auto literal = "A literal"_ns;
mozilla::SprintfBuf(buf, 1024,
                    "Formatting a number: %" PRId64
                    " and another one: " PRIu64
                    ", and finally a string: %s",
                    a, b, literal.get());
```

one can do:

```c++
#include <fmt/format.h>
// ...
char buf[1024];
int64_t a = 123;
uint64_t a = 456;
auto literal = "A literal"_ns;
fmt::format_to_n(res, 1024,
                 FMT_STRING("Formatting a number: {} and another one: {} "
                            "and finally a string: {}"),
                 a, b, literal.get());
```


# User-defined types

Formatting a [user-defined type](https://fmt.dev/11.0/api/#formatting-user-defined-types) can be done once, and then used with all sorts of formatting function in `{fmt}`. Given an example object:

```c++
struct POD {
  double mA;
  uint64_t mB;
};
```

one can write a custom formatter like so:

```c++
auto format_as(POD aInstance) -> std::string {
  return fmt::format(FMT_STRING("POD: mA: {}, mB: {}"), aInstance.mA,
                     aInstance.mB);
}
```

and use it as expected in a variety of ways:

```c++
char bufFmt[1024] = {};

POD p{4.3, 8};
auto [out, size] = fmt::format_to(bufFmt, "{}", p);
*out = 0; // Write the null terminator

assert(!strcmp("POD: mA: 4.3, mB: 8", bufFmt));
fmt::println(FMT_STRING("### debug: {}"), p);
fmt::print(stderr, FMT_STRING("### debug to stderr {}\n"), p);
MOZ_LOG_FMT(gLogModule, "Important: {}", p);
```

# Formatting sequences

Containers that can work with with range-based for-loop can be formatted easily:

```c++
nsTArray<uint8_t> array(4);
for (uint32_t i = 0; i < 4; i++) {
    array.AppendElement((123 * 5 * (i+1)) % 255);
}
auto [out, size] = fmt::format_to(bufFmt, FMT_STRING("{:#04x}"), fmt::join(array, ", "));
*out = 0; // Write the null terminator
ASSERT_STREQ("0x69, 0xd2, 0x3c, 0xa5", bufFmt);
```

# `MOZ_LOG` integration

`MOZ_LOG_FMT`  is like `MOZ_LOG`, but takes an `{fmt}`-style format string:

```c++
MOZ_LOG_FMT(gLogModule, "{}x{} = {}", 3, 3, 3*3);
```

Unlike with `MOZ_LOG`, it is unnecessary to put an extra pair of parenthesis around the format and argument list.

# `ns*String` integration

It is possible to append an `{fmt}`-style format string to an `nsString` like so:

```c++
nsCString aLovelyString("Here is a value: ");
aLovelyString.AppendFmt(FMT_SRING("{}"), 4);

nsString aLovelyWideString(u"Here are two values: ");
aLovelyString.AppendFmt(FMT_SRING(u"{}, {}"), 4, u"wide");
```

Or directly use `nsFmt[C]String`:

```c++
nsFmtCString str(FMT_STRING("{},{},{},{}"), 1, 1, 2, 3);
nsFmtString str(FMT_STRING(u"{},{},{},{}"), 1, 1, 2, u"wide string");
// use it as usual
```

# Useful links

- The syntax of `{fmt}` format string: <https://fmt.dev/latest/syntax/>
- The complete API of the library: <https://fmt.dev/latest/api/>

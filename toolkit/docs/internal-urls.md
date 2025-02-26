# Internal URLs

Firefox and other Gecko applications use several URL schemes (protocols) for
internal or "special" resources.

The main ones are:

<!-- no toc -->
  - [`moz-src` URLs](#moz-src-urls)
  - [`resource` URLs](#resource-urls)
  - [`chrome` URLs](#chrome-urls)

There are other special protocols like `moz-icon` and various places URLs,
but at the moment they are not covered here.

## `moz-src` URLs
URLs look like: `moz-src:///browser/components/BrowserGlue.sys.mjs`.

Note that there is no "host" component (nothing between the second and third
slash). In future we may support non-empty hosts with specific meanings. For
now, always use a triple slash.

Everything after this is the full source path of the file you are referencing.

Of course this is an abstraction: the URL will only work if the file is actually
 packaged.

### Packaging

To package a file for use via `moz-src`, include it in a
[`moz.build` file][mozbuild-files] inside a `MOZ_SRC_FILES` instruction.

All files are packaged in the toolkit `omni.ja` file. Internally, the URL is
translated into a `jar:file` URL into the toolkit (`gre`) `omni.ja` file.

### Security considerations

Only privileged (system principal / "chrome" privileged) code can load
or link to `moz-src` URLs.

The intention is that in future we make it possible to have more fine-grained
restrictions for `moz-src` URLs with non-empty "host" portions
(e.g. `moz-src://about/` for content only accessible to about: pages), but this
has not yet been implemented.

## `resource` URLs
URLs look like `resource://mapping/optional/path/components/file.txt`.

Here, `mapping` is an arbitrary identifier chosen elsewhere. There are
3 builtin mappings of note:

- `gre` ("Gecko Runtime Environment") for some of the content inside the
  "toolkit" omni.ja file
- `app`, for some of the content inside the "browser" (or other
  application-specific) omni.ja file.
- `android`, on Android, for APK contents.

The `app` mapping is the default, which means that if you use a resource URL
without a host component (`resource:///whatever`), it is equivalent to the
`app` URL: `resource://app/whatever`.

Each mapping is resolved to a URL in the resource URL protocol handler, after
which any subsequent path/file components are resolved relative to that URL.

Additional mappings can be added via a
[chrome.manifest instruction][resource-map]. Builtin extensions make use of
this, as do [some components][searchfox-res-reg].

### Packaging
The builtin `app` and `gre` mappings, as well as any additional mappings from
`jar.mn` files and other non-extension parts of the build, always resolve into
the app-specific and toolkit `omni.ja` file, respectively.

To package files for access via `resource` URLs, use
[jar manifest file instructions][jar-manifest-files] to package files into
the mapped directory inside `omni.ja` .

### Security considerations
By default `resource` URLs cannot be accessed by web content and are restricted
to privileged (system principal / "chrome" privileged) code. However, you can
register a `resource` mapping with `contentaccessible=yes` in order to
"holepunch" this restriction. Note that this means _everything_ packaged under
that path becomes linkable and loadable by all web content. With few exceptions
(like user agent stylesheets), that is not what you want.

Unfortunately more fine-grained restrictions are not available for `resource`
URLs.

## `chrome` URLs
`chrome` URLs take one of three forms:

- `chrome://browser/content/browser.xhtml`
- `chrome://browser/skin/browser.css`
- `chrome://browser/locale/browser.properties`

The first ("host") component after `chrome://` is the **package name**. Common
ones are `browser` (for front-end Desktop Firefox files) and `global` (for
content used by Gecko/toolkit code), but different parts of the codebase can and
do register their own packages.

The second component after `chrome://{packagename}/` is always one of `content`,
`skin` or `locale`, known as "providers". They cater to JS and HTML (`content`),
style information (`skin`) and old-style `.properties` localization files
(`locale`).

Any path portions after the provider are relative paths to where that particular
package & provider combination is pointing.

More information is available in
[the build system's chrome registration docs][chrome-registration].

### Packaging
To package files for access via `chrome` URLs, use
[jar manifest file instructions][jar-manifest-files] to package files into
the mapped directory inside `omni.ja` .

Depending on how and where [the chrome is registered][chrome-registration],
the files are typically packaged into either the app or toolkit `omni.ja` file,
though it is possible to use `chrome` URLs inside builtin extensions as well.

### Security considerations
By default `chrome` URLs cannot be accessed by web content and are restricted
to privileged (system principal / "chrome" privileged) code. However, you can
register a chrome `content` mapping with `contentaccessible=yes` in order to
"holepunch" this restriction. Note that this means _everything_ packaged under
**the content, skin and locale packages for that identifier** becomes linkable
and loadable by all web content. Please do not use this functionality going
forward.

Unfortunately more fine-grained restrictions are not available for `chrome`
URLs.


[chrome-registration]: ../build/buildsystem/chrome-registration
[resource-map]: ../build/buildsystem/chrome-registration#resource
[mozbuild-files]: ../build/buildsystem/mozbuild-files
[searchfox-res-reg]: https://searchfox.org/mozilla-central/search?q=%25+resource&path=jar.mn&case=false&regexp=false
[jar-manifest-files]: ../build/buildsystem/jar-manifests

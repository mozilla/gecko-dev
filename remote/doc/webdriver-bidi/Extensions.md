# Extensions

The WebDriver BiDi specification provides a flexible framework that allows browser vendors to define custom modules and arguments. This document outlines the Firefox-specific extensions implemented in WebDriver BiDi, including any custom functionality beyond the core specification.

## Modules

There are currently no custom modules defined in the Firefox implementation of WebDriver BiDi.

## Parameters

Firefox provides additional parameters for certain commands, as detailed in the list below.

### webExtension.install

```CDDL
webExtension.InstallParameters = {
   extensionData: webExtension.ExtensionData,
   ? moz:permanent: bool .default false,
}
```

Description:

* `moz:permanent`: When set to `true`, the web extension will be installed permanently. This requires the extension to be signed. Unsigned extensions can only be installed temporarily, which is the default behavior.

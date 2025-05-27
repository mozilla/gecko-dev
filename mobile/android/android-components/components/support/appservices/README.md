# [Android Components](../../../README.md) > Support > AppServices

A collection of helpers for integrating native Application Services' components.

## Usage

During Application initialization (typically in `Application#onCreate`), add the following line:

```kotlin
AppServicesInitializer.init(crashReporter)
```

You may also need to initialize the networking layer with a concept-fetch implementation:

```kotlin
RustHttpConfig.setClient(lazy { HttpClient() })

// To ensure emulators still work, add something similar.
if (isDebug) {
    RustHttpConfig.allowEmulatorLoopback()
}
```

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:support-appservices:{latest-version}"
```

### Rust Log

A bridge allowing log messages from Rust code to be sent to the log
system in support-base

### Rust HTTP

A bridge allowing configuration of Rust HTTP requests without directly depending
on the application services library.

This essentially wraps the rust HTTP config library so that consumers who don't
use a custom megazord don't have to depend on application-services code.

It's separate from RustLog since it's plausible users might only want to
initialize logging, and not use any app-services network functionality.

### Rust Errors

A bridge for reporting Rust errors to Sentry/Glean.

This component defines and installs an application-services `ApplicationErrorReporter` class that:
- Forwards error reports and breadcrumbs to `SentryServices`
- Reports error counts to Glean

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/

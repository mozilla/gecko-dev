# Preferences

There are a couple of preferences associated with the Remote Agent:

## Configurable preferences

### `remote.active-protocols`

Defines the remote protocols that are active. Currently available protocols are,
WebDriver BiDi (`1`).

Previously used to define the remote protocols that are active. With the end
of the CDP support, WebDriver BiDi is the only available protocol, and the
preference was removed in Firefox 141.

### `remote.events.async.enabled`

This preference determines whether processing of action sequences happens in the
parent process and dispatching of individual actions is forwarded to the content
process. Starting with Firefox 135 its value is set to `true` by default.

The preference and fallback to process actions entirely in the content process
were removed in Firefox 139.

### `remote.experimental.enabled`

Defines if WebDriver BiDi experimental commands and events are available for usage.
Defaults to `true` in Nightly builds, and `false` otherwise.

### `remote.log.level`

Defines the verbosity of the internal logger.  Available levels
are, in descending order of severity, `Trace`, `Debug`, `Config`,
`Info`, `Warn`, `Error`, and `Fatal`.  Note that the value is
treated case-sensitively.

### `remote.log.truncate`

Defines whether long log messages should be truncated. Defaults to true.

### `remote.prefs.recommended`

By default remote protocols attempts to set a range of preferences deemed
suitable in automation when it starts.  These include the likes of
disabling auto-updates, Telemetry, and first-run UX. Set this preference to
`false` to skip setting those preferences, which is mostly useful for internal
Firefox CI suites.

The user preference file takes precedence over the recommended
preferences, meaning any user-defined preference value will not be
overridden.

### `remote.retry-on-abort`

This preference defines whether certain IPC calls from the parent process to
content processes should be retried when a browsing context is replaced due
to cross-origin navigation, or made inactive when a page moved into BFCache.

Introduced in Firefox 132, the preference is set to `true` by default.

### `remote.system-access-check.enabled`

Temporary preference to allow WebDriver clients to disable the system access checks
when trying to switch with Marionette into chrome scope (parent process) testing.

Instead of switching the preference value, the client should ideally fix the breakage
by passing `-remote-allow-system-access` as an argument to the Firefox binary.

This preference was introduced in Firefox 138 with a default value of `true`
and was removed in Firefox 141.

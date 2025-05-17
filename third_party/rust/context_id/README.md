# Context ID

A context ID is a UUID that is used when making requests to MARS or to Merino to help reduce click fraud.

The **Context ID Rust component** creates a shared mechanism for managing context IDs, and to allow them to be rotated after they have exceeded some age.

It is currently under construction and not yet used.

## Tests

Tests are run with

```shell
cargo test -p context_id
```

## Bugs

We use Bugzilla to track bugs and feature work. You can use [this link](bugzilla.mozilla.org/enter_bug.cgi?product=Firefox&component=New Tab Page) to file bugs in the `Firefox :: New Tab Page` bug component.

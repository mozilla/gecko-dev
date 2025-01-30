# Security Policy

This document describes how security vulnerabilities in this project should be reported.

## Supported Versions

Support for `mtu` is based on the Firefox version in which it has landed.
Versions of `mtu` in [current versions of Firefox](https://whattrainisitnow.com/calendar/) are actively supported.

The version of `mtu` that is active can be found in the Firefox repositories:

- [release](https://hg.mozilla.org/mozilla-unified/file/release/third_party/rust/mtu-transport/Cargo.toml),
- [beta](https://hg.mozilla.org/mozilla-unified/file/beta/third_party/rust/mtu-transport/Cargo.toml), and
- [trunk/central](https://hg.mozilla.org/mozilla-unified/file/central/third_party/rust/mtu-transport/Cargo.toml),
- [ESR 115](https://hg.mozilla.org/mozilla-unified/file/esr115/third_party/rust/mtu-transport/Cargo.toml).

The listed version in these files corresponds to [tags](https://github.com/mozilla/mtu/tags) on this repository.
Releases do not always correspond to a branch.

We welcome reports of security vulnerabilities in any of these released versions or the latest code on the `main` branch.

## Reporting a Vulnerability

To report a security problem with `mtu`, create a bug in Mozilla's Bugzilla instance in the [Core :: Networking](https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Networking) component.

**IMPORTANT: For security issues, please make sure that you check the box labelled "Many users could be harmed by this security problem".**
We advise that you check this option for anything that involves anything security-relevant, including memory safety, crashes, race conditions, and handling of confidential information.

Review Mozilla's [guides on bug reporting](https://bugzilla.mozilla.org/page.cgi?id=bug-writing.html) before you open a bug.

Mozilla operates a [bug bounty program](https://www.mozilla.org/en-US/security/bug-bounty/), for which this project is eligible.

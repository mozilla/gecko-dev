cargo-audit
===========

`cargo-audit <https://crates.io/crates/cargo-audit>`__ audit the root Cargo.lock files for security vulnerabilities.

Run Locally
-----------

The mozlint integration of cargo-audit can be run using mach:
.. parsed-literal::

   $ mach lint --linter cargo-audit

Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/cargo-audit.yml>`
* :searchfox:`Source <tools/lint/cargo-audit/__init__.py>`

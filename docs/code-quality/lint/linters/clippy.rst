clippy
======

`clippy`_ is the tool for Rust static analysis.

Run Locally
-----------

The mozlint integration of clippy can be run using mach:

.. parsed-literal::

    $ mach lint --linter clippy <file paths>

    # Return warnings

    $ mach lint --warnings --linter clippy <file paths>

.. note::

   clippy expects a path or a .rs file. It doesn't accept Cargo.toml
   as it would break the mozlint workflow.

To use Rust nightly, you can set the environment variable `RUSTUP_TOOLCHAIN` to `nightly`:

.. parsed-literal:

    # Note that you need to have nightly installed with Rustup
    # A clobber might be necessary after switching toolchains
    $ RUSTUP_TOOLCHAIN=nightly ./mach lint --warning -l clippy .

Configuration
-------------

To enable clippy on new directory, add the path to the include
section in the `clippy.yml <https://searchfox.org/mozilla-central/source/tools/lint/clippy.yml>`_ file.


Sources
-------

* `Configuration (YAML) <https://searchfox.org/mozilla-central/source/tools/lint/clippy.yml>`_
* `Source <https://searchfox.org/mozilla-central/source/tools/lint/clippy/__init__.py>`_

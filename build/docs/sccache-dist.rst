.. _sccache_dist:

==================================
Distributed sccache (sccache-dist)
==================================

`sccache <https://github.com/mozilla/sccache>`_ is a ccache-like tool written in
rust.

Distributed sccache (also referred to as sccache-dist) is being rolled out to
Mozilla offices as a replacement for icecc. The steps for setting up your
machine as an sccache-dist server as well as distributing your build to servers
in your office are detailed below.

In addition to improved security properties, distributed sccache offers
distribution and caching of rust compilation, so it should be an improvement
above and beyond what we see with icecc. Build servers run on linux and
distributing builds is currently supported from macOS and linux machines.
Distribution from Windows is supported in principle but hasn't seen sufficient
testing.

Steps for distributing a build as an sccache-dist client
========================================================

Start by following the instructions at https://github.com/mozilla/sccache/blob/master/docs/DistributedQuickstart.md#configure-a-client
to configure your sccache distributed client. Ignore the note about custom
toolchains if you're distributing compilation from linux.
sccache 0.2.11 or above is recommended, and the auth section of your config
must read::

    [dist.auth]
    type = "mozilla"

* The scheduler url to use is: ``https://sccache1.corpdmz.<OFFICE>.mozilla.com``,
  where <OFFICE> is, for instance, sfo1. A complete list of office short names
  to be used can be found in the `Office Addressing Schemes spreadsheet <https://docs.google.com/spreadsheets/d/1alscUTcfFyu3L0vs_S_cGi9JxF4uPrfsmwJko9annWE/edit#gid=0>`_.

* To use distributed sccache from a Mozilla office, you must be on the corporate
  network. Use the ``Mozilla`` ssid for wireless. The corp vlan is the default
  if wired.

* If you're compiling from a macOS client, there are a handful of additional
  considerations detailed here:
  https://github.com/mozilla/sccache/blob/master/docs/DistributedQuickstart.md#considerations-when-distributing-from-macos.
  In particular, custom toolchains will need to be specified.
  Run ``./mach bootstrap`` to download prebuilt toolchains to
  ``~/.mozbuild/clang-dist-toolchain.tar.xz`` and
  ``~/.mozbuild/rustc-dist-toolchain.tar.xz``. This is an example of the paths
  that should be added to your client config to specify toolchains to build on
  macOS, located at ``~/Library/Preferences/Mozilla.sccache/config``::

    [[dist.toolchains]]
    type = "path_override"
    compiler_executable = "/path/to/home/.rustup/toolchains/stable-x86_64-apple-darwin/bin/rustc"
    archive = "/path/to/home/.mozbuild/rustc-dist-toolchain.tar.xz"
    archive_compiler_executable = "/builds/worker/toolchains/rustc/bin/rustc"

    [[dist.toolchains]]
    type = "path_override"
    compiler_executable = "/path/to/home/.mozbuild/clang/bin/clang"
    archive = "/path/to/home/.mozbuild/clang-dist-toolchain.tar.xz"
    archive_compiler_executable = "/builds/worker/toolchains/clang/bin/clang"

    [[dist.toolchains]]
    type = "path_override"
    compiler_executable = "/path/to/home/.mozbuild/clang/bin/clang++"
    archive = "/path/to/home/.mozbuild/clang-dist-toolchain.tar.xz"
    archive_compiler_executable = "/builds/worker/toolchains/clang/bin/clang"

  Note that the version of ``rustc`` found in ``rustc-dist-toolchain.tar.xz``
  must match the version of ``rustc`` used locally. The distributed archive
  will contain the version of ``rustc`` used by automation builds, which may
  lag behind stable for a few days after Rust releases, which is specified by
  the task definition in
  `this file <https://hg.mozilla.org/mozilla-central/file/tip/taskcluster/ci/toolchain/dist-toolchains.yml>`_.
  For instance, to specify 1.37.0 rather than the current stable, run
  ``rustup toolchain add 1.37.0`` and point to
  ``~/.rustup/toolchains/1.37.0-x86_64-apple-darwin/bin/rustc`` in your
  client config.

* Add the following to your mozconfig::

    ac_add_options CCACHE=/path/to/sccache

  If you're compiling from a macOS client, you might need some additional configuration::

    # Set the target flag to Darwin
    export CFLAGS="--target=x86_64-apple-darwin16.0.0"
    export CXXFLAGS="--target=x86_64-apple-darwin16.0.0"
    export HOST_CFLAGS="--target=x86_64-apple-darwin16.0.0"
    export HOST_CXXFLAGS="--target=x86_64-apple-darwin16.0.0"

    # Specify the macOS SDK to use
    ac_add_options --with-macos-sdk=/path/to/MacOSX-SDKs/MacOSX10.11.sdk

  You can get the right macOS SDK from the `MacOSX-SDKs repository <https://github.com/phracker/MacOSX-SDKs/>`_
  or by downloading an old version of XCode from `developer.apple.com <https://developer.apple.com>`_ and unpacking the SKD from it.

* When attempting to get your client running, the output of ``sccache -s`` should
  be consulted to confirm compilations are being distributed. To receive helpful
  logging from the local daemon in case they aren't, run
  ``SCCACHE_NO_DAEMON=1 RUST_LOG=sccache=trace path/to/sccache --start-server``
  in a terminal window separate from your build prior to building.

* Run ``./mach build -j<value>`` with an appropriately large ``<value>``.
  ``sccache --dist-status`` should provide the number of cores available to you
  (or a message if you're not connected). In the future this will be integrated
  with the build system to automatically select an appropriate value.

This should be enough to distribute your build and replace your use of icecc.
Bear in mind there may be a few speedbumps, and please ensure your version of
sccache is current before investigating further. Please see the common questions
section below and ask for help if anything is preventing you from using it over
email (dev-builds), on slack in #sccache, or in #build on irc.

Steps for setting up a server
=============================

Build servers must run linux and use bubblewrap 3.0+ for sandboxing of compile
processes. This requires a kernel 4.6 or greater, so Ubuntu 18+, RHEL 8, or
similar.

* Run ``./mach bootstrap`` or
  ``./mach artifact toolchain --from-build linux64-sccache`` to acquire a recent
  version of ``sccache-dist``. Please use a ``sccache-dist`` binary acquired in
  this fashion to ensure compatibility with statically linked dependencies.

* Collect the IP of your builder and request assignment of a static IP in a bug
  filed in
  `NetOps :: Other <https://bugzilla.mozilla.org/enter_bug.cgi?product=Infrastructure%20%26%20Operations&component=NetOps%3A%20Office%20Other>`_
  This bug should include your office (SFO, YVR, etc.), your MAC address, and a
  description of why you want a static IP (“To serve as an sccache builder”
  should be sufficient).

* Visit the ``sccache`` section of https://login.mozilla.com to generate an auth
  token for your builder.

* The instructions at https://github.com/mozilla/sccache/blob/master/docs/DistributedQuickstart.md#configure-a-build-server
  should contain everything else required to configure and run the server.

  *NOTE* Port 10500 will be used by convention for builders in offices.
  Please use port 10500 in the ``public_addr`` section of your builder config.

  Extra logging may be helpful when setting up a server. To enable logging,
  run your server with
  ``sudo env RUST_LOG=sccache=trace ~/.mozbuild/sccache/sccache-dist server --config ~/.config/sccache/server.conf``
  (or similar). *NOTE* ``sudo`` *must* come before setting environment variables
  for this to work.

  As when configuring a client, the scheduler url to use is:
  ``https://sccache1.corpdmz.<OFFICE>.mozilla.com``, where <OFFICE> is an
  office abbreviation found
  `here <https://docs.google.com/spreadsheets/d/1alscUTcfFyu3L0vs_S_cGi9JxF4uPrfsmwJko9annWE/edit#gid=0>`_.


Common questions/considerations
===============================

* My build is still slow: scache-dist can only do so much with parts of the
  build that aren't able to be parallelized. To start debugging a slow build,
  ensure the "Successful distributed compilations" line in the output of
  ``sccache -s`` dominates other counts. For a full build, at least a 2-3x
  improvement should be observed.

* My build output is incomprehensible due to a flood of warnings: clang will
  treat some warnings differently when its fed preprocessed code in a separate
  invocation (preprocessing occurs locally with sccache-dist). See the
  following note about disabling problematic warnings:
  https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Using_Icecream#I_get_build_failures_due_to_-Werror_when_building_remotely_but_not_when_building_locally

* My build fails with a message about incompatible versions of rustc between
  dependent crates: if you're using a custom toolchain check that the version
  of rustc in your ``rustc-dist-toolchain.tar.xz`` is the same as the version
  you're running locally.

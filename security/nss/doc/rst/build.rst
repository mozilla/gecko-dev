.. _mozilla_projects_nss_building:

Building NSS
============

`Introduction <#introduction>`__
--------------------------------

.. container::

   This page has detailed information on how to build NSS. Because NSS is a
   cross-platform library that builds on many different platforms and has many
   options, it may be complex to build._ Two build systems are maintained
   concurrently: a ``Make`` based and a ``gyp`` based system.

.. _build_environment:

`Prerequisites <#build_environment>`__
------------------------------------------

.. container::

   NSS needs a C and C++ compiler.  It has minimal dependencies, including only
   standard C and C++ libraries, plus `zlib <https://www.zlib.net/>`__.
   For building, you also need `make <https://www.gnu.org/software/make/>`__.
   Ideally, also install `gyp-next <https://github.com/nodejs/gyp-next>`__ and `ninja
   <https://ninja-build.org/>`__ and put them on your path. This is
   recommended, as the build is faster and more reliable.

   To install prerequisites on different platforms, one can run the following
   commands:

   **On Linux:**

   .. code::

      sudo apt install mercurial git ninja-build python3-pip
      python3 -m pip install gyp-next

   **On MacOS:**

   .. code::

      brew install mercurial git ninja python3-pip
      python3 -m pip install gyp-next

   It is also necessary to make sure that a `python` (not just `python3`)
   executable is in the path.
   The Homebrew Python installation has the necessary symlink but may require
   explicit adding to the PATH variable, for example like this:

   .. code::

      export PATH="/opt/homebrew/opt/python/libexec/bin:$PATH"

   **On Windows:**

   .. code::

      <TODO>

.. note::
   To retrieve the source code from the project repositories, users will need to
   download a release or pull the source code with their favourite Version
   Control System (git or Mercurial). Installing a VCS is not necessary to build
   an NSS release when downloaded as a compressed archive.

   By default Mozilla uses a Mercurial repository for NSS. If you whish to
   contribute to NSS and use ``git`` instead of Mercurial, we encourage you to
   install `git-cinnabar <https://github.com/glandium/git-cinnabar>`__.

..
   `Windows <#windows>`__
   ~~~~~~~~~~~~~~~~~~~~~~

   .. container::

      NSS compilation on Windows uses the same shared build system as Mozilla
      Firefox. You must first install the `Windows Prerequisites
      <https://firefox-source-docs.mozilla.org/setup/windows_build.html>`__,
      including **MozillaBuild**.

      You can also build NSS on the Windows Subsystem for Linux, but the resulting binaries aren't
      usable by other Windows applications.

.. _get_the_source:

`Source code <#get_the_source>`__
---------------------------------

.. container::

   NSS and NSPR use Mercurial for source control like other Mozilla projects. To
   check out the latest sources for NSS and NSPR--which may not be part of a
   stable release--use the following commands:

   .. code::

      hg clone https://hg.mozilla.org/projects/nspr
      hg clone https://hg.mozilla.org/projects/nss


   **To get the source of a specific release, see:**
   ref:`mozilla_projects_nss_releases` **.**

   To download the source using ``git-cinnabar`` instead:

   .. code::

      git clone hg::https://hg.mozilla.org/projects/nspr
      git clone hg::https://hg.mozilla.org/projects/nss


`Build with gyp and ninja <#build>`__
-------------------------------------

.. container::

   Build NSS and NSPR using our build script from the ``nss`` directory:

   .. code::

      cd nss
      ./build.sh

   This builds both NSPR and NSS in a parent directory called ``dist``.

   Build options are available for this script: ``-o`` will build in **Release**
   mode instead of the **Debug** mode and ``-c`` will **clean** the ``dist``
   directory before the build.

   Other build options can be displayed by running ``./build.sh --help``

.. _build_with_make:

`Build with make <#build_with_make>`__
--------------------------------------

.. container::

   Alternatively, there is a ``make`` target, which produces a similar
   result. This supports some alternative options, but can be a lot slower.

   .. code::

      USE_64=1 make -j

   The make-based build system for NSS uses a variety of variables to control
   the build. Below are some of the variables, along with possible values they
   may be set to.

.. csv-table::
   :header: "BUILD_OPT", ""
   :widths: 10,50

   "0", "Build a debug (non-optimized) version of NSS. **This is the default.**"
   "1", "Build an optimized (non-debug) version of NSS."

.. csv-table::
   :header: "USE_64", ""
   :widths: 10,50

   "0", "Build for a 32-bit environment/ABI. **This is the default.**"
   "1", "Build for a 64-bit environment/ABI. *This is recommended.*"

.. csv-table::
   :header: "USE_ASAN", ""
   :widths: 10,50

   "0", "Do not create an `AddressSanitizer
   <http://clang.llvm.org/docs/AddressSanitizer.html>`__ build. **This is the default.**"
   "1", "Create an AddressSanitizer build."


.. _unit_testing:

`Unit testing <#unit_testing>`__
--------------------------------

.. container::

   NSS contains extensive unit tests.  Scripts to run these are found in the ``tests`` directory.
   Run the standard suite by:

   .. code::

      HOST=localhost DOMSUF=localdomain USE_64=1 ./tests/all.sh

.. _unit_test_configuration:

`Unit test configuration <#unit_test_configuration>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   NSS tests are configured using environment variables.
   The scripts will attempt to infer values for ``HOST`` and ``DOMSUF``, but
   can fail. Replace ``localhost`` and ``localdomain`` with the hostname and
   domain suffix for your host. You need to be able to connect to
   ``$HOST.$DOMSUF``.

   If you don't have a domain suffix you can add an entry to ``/etc/hosts`` (on
   Windows,\ ``c:\Windows\System32\drivers\etc\hosts``) as follows:

   .. code::

      127.0.0.1 localhost.localdomain

   Validate this opening a command shell and typing: ``ping localhost.localdomain``.

   Remove the ``USE_64=1`` override if using a 32-bit build.

.. _test_results:

`Test results <#test_results>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Running all tests can take a considerable amount of time.

   Test output is stored in ``tests_results/security/$HOST.$NUMBER/``.  The file
   ``results.html`` summarizes the results, ``output.log`` captures all the test
   output.

   Other subdirectories of ``nss/tests`` contain scripts that run a subset of
   the full suite. Those can be run directly instead of ``all.sh``, which might
   save some time at the cost of coverage.

.. _mozilla_projects_nss_build_artifacts:

`Build artifacts <#build_artifacts>`__
--------------------------------------

.. _shared_libraries:

`Shared libraries <#shared_libraries>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Network Security Services provides both static libraries and shared libraries. Applications that
   use the shared libraries must use only the APIs that they export. Three shared libraries export
   public functions:

   -  The SSL/TLS library supports core TLS operations.
   -  The S/MIME library supports core S/MIME operations.
   -  The freebl library supports core crypto operations.

.. note::

   We guarantee that applications using the exported APIs will remain compatible with future
   versions of those libraries until deprecated.

.. _naming_conventions_and_special_libraries:

`Naming conventions <#naming_conventions_and_special_libraries>`__
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. container::

   Windows and Unix use different naming conventions for static and dynamic
   libraries:

   ======= ======== ===============================
           Windows  Unix
   static  ``.lib`` ``.a``
   dynamic ``.dll`` ``.so`` or ``.dylib`` or ``.sl``
   ======= ======== ===============================

   In addition, Windows has "import" libraries that bind to dynamic
   libraries. So the NSS library has the following forms:

   -  ``libnss3.so`` - Linux shared library
   -  ``libnss3.dylib`` - MacOS shared library
   -  ``libnss3.sl`` - HP-UX shared library
   -  ``libnss.a`` - Unix static library
   -  ``nss3.dll`` - Windows shared library
   -  ``nss3.lib`` - Windows import library binding to ``nss3.dll``
   -  ``nss.lib`` - Windows static library

   NSS, SSL, and S/MIME have all of the above forms.

   The following static libraries aren't included in any shared libraries

   -  ``libcrmf.a``/``crmf.lib`` provides an API for CRMF operations.
   -  ``libjar.a``/``jar.lib`` provides an API for creating JAR files.

   The following static libraries are included only in external loadable PKCS
   #11 modules:

   -  ``libnssckfw.a``/``nssckfw.lib`` provides an API for writing PKCS #11 modules.
   -  ``libswfci.a``/``swfci.lib`` provides support for software FORTEZZA.

   The following shared libraries are standalone loadable modules, not meant to
   be linked with directly:

   -  ``libfort.so``/``libfort.sl``/``fort32.dll`` provides support for hardware FORTEZZA.
   -  ``libswft.so``/``libswft.sl``/``swft32.dll`` provides support for software FORTEZZA.
   -  ``libnssckbi.so``/``libnssckbi.sl``/``nssckbi.dll`` defines the default set
      of trusted root certificates.
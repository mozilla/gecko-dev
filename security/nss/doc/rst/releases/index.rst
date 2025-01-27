.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_101_3.rst
   nss_3_107.rst
   nss_3_106.rst
   nss_3_105.rst
   nss_3_104.rst
   nss_3_103.rst
   nss_3_102_1.rst
   nss_3_102.rst
   nss_3_101.2.rst
   nss_3_101_1.rst
   nss_3_101.rst
   nss_3_100.rst
   nss_3_99.rst
   nss_3_98.rst
   nss_3_97.rst
   nss_3_96_1.rst
   nss_3_96.rst
   nss_3_95.rst
   nss_3_94.rst
   nss_3_93.rst
   nss_3_92.rst
   nss_3_91.rst
   nss_3_90_4.rst
   nss_3_90_3.rst
   nss_3_90_2.rst
   nss_3_90_1.rst
   nss_3_90.rst
   nss_3_89_1.rst
   nss_3_89.rst
   nss_3_88_1.rst
   nss_3_88.rst
   nss_3_87_1.rst
   nss_3_87.rst
   nss_3_86.rst
   nss_3_85.rst
   nss_3_84.rst
   nss_3_83.rst
   nss_3_82.rst
   nss_3_81.rst
   nss_3_80.rst
   nss_3_79_4.rst
   nss_3_79_3.rst
   nss_3_79_2.rst
   nss_3_79_1.rst
   nss_3_79.rst
   nss_3_78_1.rst
   nss_3_78.rst
   nss_3_77.rst
   nss_3_76_1.rst
   nss_3_76.rst
   nss_3_75.rst
   nss_3_74.rst
   nss_3_73_1.rst
   nss_3_73.rst
   nss_3_72_1.rst
   nss_3_72.rst
   nss_3_71.rst
   nss_3_70.rst
   nss_3_69_1.rst
   nss_3_69.rst
   nss_3_68_4.rst
   nss_3_68_3.rst
   nss_3_68_2.rst
   nss_3_68_1.rst
   nss_3_68.rst
   nss_3_67.rst
   nss_3_66.rst
   nss_3_65.rst
   nss_3_64.rst

.. note::

   **NSS 3.107** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_107_release_notes`

   **NSS 3.101.3 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_3_release_notes`

.. container::

   Changes in 3.107 included in this release:

   - Bug 1923038 - Remove MPI fuzz targets.
   - Bug 1925512 - Remove globals `lockStatus` and `locksEverDisabled`.
   - Bug 1919015 - Enable PKCS8 fuzz target.
   - Bug 1923037 - Integrate Cryptofuzz in CI.
   - Bug 1913677 - Part 2: Set tls server target socket options in config class.
   - Bug 1913677 - Part 1: Set tls client target socket options in config class.
   - Bug 1913680 - Support building with thread sanitizer.
   - Bug 1922392 - set nssckbi version number to 2.72.
   - Bug 1919913 - remove Websites Trust Bit from Entrust Root Certification Authority - G4.
   - Bug 1920641 - remove Security Communication RootCA3 root cert.
   - Bug 1918559 - remove SecureSign RootCA11 root cert.
   - Bug 1922387 - Add distrust-after for TLS to Entrust Roots.
   - Bug 1927096 - update expected error code in pk12util pbmac1 tests.
   - Bug 1929041 - Use random tstclnt args with handshake collection script.
   - Bug 1920466 - Remove extraneous assert in ssl3gthr.c.
   - Bug 1928402 - Adding missing release notes for NSS_3_105.
   - Bug 1874451 - Enable the disabled mlkem tests for dtls.
   - Bug 1874451 - NSS gtests filter cleans up the constucted buffer before the use.
   - Bug 1925505 - Make ssl_SetDefaultsFromEnvironment thread-safe.
   - Bug 1925503 - Remove short circuit test from ssl_Init.

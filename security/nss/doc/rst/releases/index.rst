.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.104** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_104_release_notes`

   **NSS 3.101.2 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_1_release_notes`

.. container::

   Changes in 3.104 included in this release:

   - Bug 1910071 - Copy original corpus to heap-allocated buffer
   - Bug 1910079 - Fix min ssl version for DTLS client fuzzer
   - Bug 1908990 - Remove OS2 support just like we did on NSPR
   - Bug 1910605 - clang-format NSS improvements
   - Bug 1902078 - Adding basicutil.h to use HexString2SECItem function
   - Bug 1908990 - removing dirent.c from build
   - Bug 1902078 - Allow handing in keymaterial to shlibsign to make the output reproducible (
   - Bug 1908990 - remove nec4.3, sunos4, riscos and SNI references
   - Bug 1908990 - remove other old OS (BSDI, old HP UX, NCR, openunix, sco, unixware or reliantUnix
   - Bug 1908990 - remove mentions of WIN95
   - Bug 1908990 - remove mentions of WIN16
   - Bug 1913750 - More explicit directory naming
   - Bug 1913755 - Add more options to TLS server fuzz target
   - Bug 1913675 - Add more options to TLS client fuzz target
   - Bug 1835240 - Use OSS-Fuzz corpus in NSS CI
   - Bug 1908012 - set nssckbi version number to 2.70.
   - Bug 1914499 - Remove Email Trust bit from ACCVRAIZ1 root cert.
   - Bug 1908009 - Remove Email Trust bit from certSIGN ROOT CA.
   - Bug 1908006 - Add Cybertrust Japan Roots to NSS.
   - Bug 1908004 - Add Taiwan CA Roots to NSS.
   - Bug 1911354 - remove search by decoded serial in nssToken_FindCertificateByIssuerAndSerialNumber.
   - Bug 1913132 - Fix tstclnt CI build failure
   - Bug 1913047 - vfyserv: ensure peer cert chain is in db for CERT_VerifyCertificateNow.
   - Bug 1912427 - Enable all supported protocol versions for UDP
   - Bug 1910361 - Actually use random PSK hash type
   - Bug 1911576: Initialize NSS DB once
   - Bug 1910361 - Additional ECH cipher suites and PSK hash types
   - Bug 1903604: Automate corpus file generation for TLS client Fuzzer
   - Bug 1910364 - Fix crash with UNSAFE_FUZZER_MODE
   - Bug 1910605 - clang-format shlibsign.c


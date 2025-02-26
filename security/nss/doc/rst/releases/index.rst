.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_108.rst
   nss_3_101_3.rst
   nss_3_107.rst
   nss_3_106.rst
   nss_3_105.rst
   nss_3_104.rst
   nss_3_103.rst
   nss_3_102_1.rst
   nss_3_102.rst
   nss_3_101_2.rst
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

   **NSS 3.108** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_108_release_notes`

   **NSS 3.101.3 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_3_release_notes`

.. container::

   Changes in 3.108 included in this release:

   - Bug 1923285 - libclang-16 -> libclang-19
   - Bug 1939086 - Turn off Secure Email Trust Bit for Security Communication ECC RootCA1.
   - Bug 1937332 - Turn off Secure Email Trust Bit for BJCA Global Root CA1 and BJCA Global Root CA2.
   - Bug 1915902 - Remove SwissSign Silver CA – G2.
   - Bug 1938245 - Add D-Trust 2023 TLS Roots to NSS
   - Bug 1942301 - fix fips test failure on windows.
   - Bug 1935925 - change default sensitivity of KEM keys.
   - Bug 1936001 - Part 1: Introduce frida hooks and script,
   - Bug 1942350 - add missing arm_neon.h include to gcm.c.
   - Bug 1831552 - ci: update windows workers to win2022 r=nss-reviewers,nkulatova NSS_3_108_BETA2
   - Bug 1831552 - strip trailing carriage returns in tools tests r=nss-reviewers,nkulatova
   - Bug 1880256 - work around unix/windows path translation issues in cert test script r=nss-reviewers,nkulatova
   - Bug 1831552 - ci: let the windows setup script work without $m r=nss-reviewers,nkulatova
   - Bug 1880255 - detect msys r=nss-reviewers,nkulatova
   - Bug 1936680 - add a specialized CTR_Update variant for AES-GCM. r=nss-reviewers,keeler
   - Bug 1930807 NSS policy updates - cavs NSS_3_108_BETA1
   - Bug 1930806 FIPS changes need to be upstreamed: FIPS 140-3 RNG
   - Bug 1930806 FIPS changes need to be upstreamed: Add SafeZero
   - Bug 1930806 FIPS changes need to be upstreamed - updated POST
   - Bug 1933031 Segmentation fault in SECITEM_Hash during pkcs12 processing
   - Bug 1929922 - Extending NSS with LoadModuleFromFunction functionality r=keeler,nss-reviewers
   - Bug 1935984 - Ensure zero-initialization of collectArgs.cert, r=djackson,nss-reviewers
   - Bug 1934526 - pkcs7 fuzz target use CERT_DestroyCertificate, r=djackson,nss-reviewers
   - Bug 1915898 - Fix actual underlying ODR violations issue, r=djackson,nss-reviewers
   - Bug 1184059 - mozilla::pkix: allow reference ID labels to begin and/or end with hyphens r=jschanck
   - Bug 1927953 - don't look for secmod.db in nssutil_ReadSecmodDB if NSS_DISABLE_DBM is set r=jschanck
   - Bug 1934526 - Fix memory leak in pkcs7 fuzz target, r=djackson,nss-reviewers
   - Bug 1934529 - Set -O2 for ASan builds in CI, r=djackson,nss-reviewers
   - Bug 1934543 - Change branch of tlsfuzzer dependency, r=djackson,nss-reviewers
   - Bug 1915898 - Run tests in CI for ASan builds with detect_odr_violation=1, r=djackson,nss-reviewers
   - Bug 1934241 - Fix coverage failure in CI, r=djackson,nss-reviewers
   - Bug 1934213 - Add fuzzing for delegated credentials, DTLS short header and Tls13BackendEch, r=djackson,nss-reviewers
   - Bug 1927142 - Add fuzzing for SSL_EnableTls13GreaseEch and SSL_SetDtls13VersionWorkaround, r=djackson,nss-reviewers
   - Bug 1913677 - Part 3: Restructure fuzz/, r=djackson,nss-reviewers
   - Bug 1931925 - Extract testcases from ssl gtests for fuzzing, r=djackson,nss-reviewers
   - Bug 1923037 - Force Cryptofuzz to use NSS in CI, r=nss-reviewers,nkulatova
   - Bug 1923037 - Fix Cryptofuzz on 32 bit in CI, r=nss-reviewers,nkulatova
   - Bug 1933154 - Update Cryptofuzz repository link, r=nss-reviewers,nkulatova
   - Bug 1926256 - fix build error from 9505f79d r=jschanck
   - Bug 1926256 - simplify error handling in get_token_objects_for_cache. r=rrelyea
   - Bug 1931973 - nss doc: fix a warning r=bbeurdouche
   - Bug 1930797 pkcs12 fixes from RHEL need to be picked up.

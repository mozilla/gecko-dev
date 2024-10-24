.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_106.rst
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

   **NSS 3.106** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_106_release_notes`

   **NSS 3.101.2 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_1_release_notes`

.. container::

   Changes in 3.106 included in this release:

   - Bug 1925975 - NSS 3.106 should be distributed with NSPR 4.36.
   - Bug 1923767 - pk12util: improve error handling in p12U_ReadPKCS12File.
   - Bug 1899402 - Correctly destroy bulkkey in error scenario.
   - Bug 1919997 - PKCS7 fuzz target, r=djackson,nss-reviewers.
   - Bug 1923002 - Extract certificates with handshake collection script.
   - Bug 1923006 - Specify len_control for fuzz targets.
   - Bug 1923280 - Fix memory leak in dumpCertificatePEM.
   - Bug 1102981 - Fix UBSan errors for SECU_PrintCertificate and SECU_PrintCertificateBasicInfo.
   - Bug 1921528 - add new error codes to mozilla::pkix for Firefox to use.
   - Bug 1921768 - allow null phKey in NSC_DeriveKey.
   - Bug 1921801 - Only create seed corpus zip from existing corpus.
   - Bug 1826035 - Use explicit allowlist for for KDF PRFS.
   - Bug 1920138 - Increase optimization level for fuzz builds.
   - Bug 1920470 - Remove incorrect assert.
   - Bug 1914870 - Use libFuzzer options from fuzz/options/\*.options in CI.
   - Bug 1920945 - Polish corpus collection for automation.
   - Bug 1917572 - Detect new and unfuzzed SSL options.
   - Bug 1804646 - PKCS12 fuzzing target.


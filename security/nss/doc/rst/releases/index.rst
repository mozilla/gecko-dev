.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_110.rst
   nss_3_109.rst
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

   **NSS 3.110** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_110_release_notes`

   **NSS 3.101.3 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_3_release_notes`

.. container::

   Changes in 3.110 included in this release:

   - Bug 1930806 - FIPS changes need to be upstreamed: force ems policy.
   - Bug 1954724 - Prevent excess allocations in sslBuffer_Grow.
   - Bug 1953429 - Remove Crl templates from ASN1 fuzz target.
   - Bug 1953429 - Remove CERT_CrlTemplate from ASN1 fuzz target.
   - Bug 1952855 - Fix memory leak in NSS_CMSMessage_IsSigned.
   - Bug 1930807 - NSS policy updates.
   - Bug 1951161 - Improve locking in nssPKIObject_GetInstances.
   - Bug 1951394 - Fix race in sdb_GetMetaData.
   - Bug 1951800 - Fix member access within null pointer.
   - Bug 1950077 - Increase smime fuzzer memory limit.
   - Bug 1949677 - Enable resumption when using custom extensions.
   - Bug 1952568 - change CN of server12 test certificate.
   - Bug 1949118 - Part 2: Add missing check in NSS_CMSDigestContext_FinishSingle.
   - Bug 1949118 - Part 1: Fix smime UBSan errors.
   - Bug 1930806 - FIPS changes need to be upstreamed: updated key checks.
   - Bug 1951491 - Don't build libpkix in static builds.
   - Bug 1951395 - handle `-p all` in try syntax.
   - Bug 1951346 - fix opt-make builds to actually be opt.
   - Bug 1951346 - fix opt-static builds to actually be opt.
   - Bug 1916439 - Remove extraneous assert.

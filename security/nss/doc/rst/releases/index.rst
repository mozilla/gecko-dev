.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

   nss_3_113.rst
   nss_3_112.rst
   nss_3_111.rst
   nss_3_110.rst
   nss_3_101_4.rst
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

   **NSS 3.112** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_112_release_notes`

   **NSS 3.101.4 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_4_release_notes`

.. container::

   Changes in 3.112 included in this release:

   - Bug 1963792 - Fix alias for mac workers on try.
   - Bug 1966786 - ensure all options can be configured with SSL_OptionSet and SSL_OptionSetDefault.
   - Bug 1931930 - ABI/API break in ssl certificate processing
   - Bug 1955971 - remove unnecessary assertion in sec_asn1d_init_state_based_on_template.
   - Bug 1965754 - update taskgraph to v14.2.1.
   - Bug 1964358 - Workflow for automation of the release on GitHub when pushing a tag
   - Bug 1952860 - fix faulty assertions in SEC_ASN1DecoderUpdate
   - Bug 1934877 - Renegotiations should use a fresh ECH GREASE buffer.
   - Bug 1951396 - update taskgraph to v14.1.1
   - Bug 1962503 - Partial fix for ACVP build CI job
   - Bug 1961827 - Initialize find in sftk_searchDatabase.
   - Bug 1963121 - Add clang-18 to extra builds.
   - Bug 1963044 - Fault tolerant git fetch for fuzzing.
   - Bug 1962556 - Tolerate intermittent failures in ssl_policy_pkix_ocsp.
   - Bug 1962770 - fix compiler warnings when DEBUG_ASN1D_STATES or CMSDEBUG are set.
   - Bug 1961835 - fix content type tag check in NSS_CMSMessage_ContainsCertsOrCrls.
   - Bug 1963102 - Remove Cryptofuzz CI version check

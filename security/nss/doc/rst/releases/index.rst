.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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

   **NSS 3.109** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_109_release_notes`

   **NSS 3.101.3 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_3_release_notes`

.. container::

   Changes in 3.109 included in this release:

   - Bug 1939512 - Call BL_Init before RNG_RNGInit() so that special SHA instructions can be used if available
   - Bug 1930807 - NSS policy updates - fix inaccurate key policy issues
   - Bug 1945883 - SMIME fuzz target
   - Bug 1914256 - ASN1 decoder fuzz target
   - Bug 1936001 - Part 2: Revert "Extract testcases from ssl gtests for fuzzing"
   - Bug 1915155 - Add fuzz/README.md
   - Bug 1936001 - Part 4: Fix tstclnt arguments script
   - Bug 1944545 - Extend pkcs7 fuzz target
   - Bug 1912320 - Extend certDN fuzz target
   - Bug 1854095 - delete old docker image definitions and task scheduling code  
   - Bug 1854095 - apply nspr patch in acvp script
   - Bug 1854095 - parse try syntax on pushes to nss-try
   - Bug 1854095 - add "fuzz" task kind
   - Bug 1854095 - add "test" task kind
   - Bug 1854095 - add "certs" task kind
   - Bug 1854095 - add "build" task kind
   - Bug 1854095 - add "tools" task kind
   - Bug 1854095 - add "fuzz" docker image
   - Bug 1854095 - add "gcc-4.4" docker image
   - Bug 1854095 - add "clang-format" docker image
   - Bug 1854095 - add "acvp" docker image
   - Bug 1854095 - add "builds" docker image
   - Bug 1854095 - switch .taskcluster.yml to taskgraph
   - Bug 1944300 - restore alloca.h include
   - Bug 1944300 - refactor run_hacl.sh slightly
   - Bug 1944300 - ignore all libcrux files in run_hacl.sh
   - Bug 1944300 - use `diff -u` in HACL* consistency check
   - Bug 1944300 - revert changes to HACL* files from bug 1866841
   - Bug 1936001 - Part 3: Package frida corpus script


.. _mozilla_projects_nss_releases:

Releases
========

.. toctree::
   :maxdepth: 0
   :glob:
   :hidden:

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
   nss_3_91_0.rst
   nss_3_90_4.rst
   nss_3_90_3.rst
   nss_3_90_2.rst
   nss_3_90_1.rst
   nss_3_90_0.rst
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

   **NSS 3.103** is the latest version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_103_release_notes`

   **NSS 3.101.2 (ESR)** is the latest ESR version of NSS.
   Complete release notes are available here: :ref:`mozilla_projects_nss_nss_3_101_1_release_notes`

.. container::

   Changes in 3.103 included in this release:

   - Bug 1908623 - move list size check after lock acquisition in sftk_PutObjectToList.
   - Bug 1899542 - Add fuzzing support for SSL_ENABLE_POST_HANDSHAKE_AUTH.
   - Bug 1909638 - Follow-up to fix test for presence of file nspr.patch.
   - Bug 1903783 - Adjust libFuzzer size limits.
   - Bug 1899542 - Add fuzzing support for SSL_SetCertificateCompressionAlgorithm, SSL_SetClientEchConfigs, SSL_VersionRangeSet and SSL_AddExternalPsk.
   - Bug 1899542 - Add fuzzing support for SSL_ENABLE_GREASE and SSL_ENABLE_CH_EXTENSION_PERMUTATION.
   - Bug 1909638 - NSS automation should always cleanup the NSPR tree.
   - Bug 590806 - Freeing symKey in pk11_PubDeriveECKeyWithKDF when a key_size is 0 and wrong kd.
   - Bug 1908831 - Don't link zlib where it's not needed.
   - Bug 1908597 - Removing dead code from X25519 seckey.
   - Bug 1905691 - ChaChaXor to return after the functio.
   - Bug 1900416 - NSS Support of X25519 import/export functionalit.
   - Bug 1890618 - add PeerCertificateChainDER function to libssl.
   - Bug 1908190 - fix definitions of freeblCipher_native_aes_*_worker on arm.
   - Bug 1907743 - pk11mode: avoid passing null phKey to C_DeriveKey.
   - Bug 1902119 - reuse X25519 share when offering both X25519 and Xyber768d00.
   - Set nssckbi version number to 2.69.
   - Bug 1904404 - add NSS_DISABLE_NSPR_TESTS option to makefile.
   - Bug 1905746 - avoid calling functions through pointers of incompatible type.
   - Bug 1905783 - merge docker-fuzz32 and docker-fuzz images.
   - Bug 1903373 - fix several scan-build warnings.

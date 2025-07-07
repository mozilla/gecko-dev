# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi',
  ],
  'target_defaults': {
    'variables': {
      'debug_optimization_level': '3',
    },
    'target_conditions': [
      [ '_type=="executable"', {
        'libraries!': [
          '<@(nspr_libs)',
        ],
        'libraries': [
          '<(nss_dist_obj_dir)/lib/libplds4.a',
          '<(nss_dist_obj_dir)/lib/libnspr4.a',
          '<(nss_dist_obj_dir)/lib/libplc4.a',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'nssfuzz_base',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/lib/certdb/certdb.gyp:certdb',
        '<(DEPTH)/lib/certhigh/certhigh.gyp:certhi',
        '<(DEPTH)/lib/cryptohi/cryptohi.gyp:cryptohi',
        '<(DEPTH)/lib/ssl/ssl.gyp:ssl',
        '<(DEPTH)/lib/base/base.gyp:nssb',
        '<(DEPTH)/lib/dev/dev.gyp:nssdev',
        '<(DEPTH)/lib/pki/pki.gyp:nsspki',
        '<(DEPTH)/lib/util/util.gyp:nssutil',
        '<(DEPTH)/lib/nss/nss.gyp:nss_static',
        '<(DEPTH)/lib/pkcs7/pkcs7.gyp:pkcs7',
        '<(DEPTH)/lib/pkcs12/pkcs12.gyp:pkcs12',
        '<(DEPTH)/lib/smime/smime.gyp:smime',
        # This is a static build of pk11wrap, softoken, and freebl.
        '<(DEPTH)/lib/pk11wrap/pk11wrap.gyp:pk11wrap_static',
        '<(DEPTH)/lib/libpkix/libpkix.gyp:libpkix',
      ],
      'direct_dependent_settings': {
        'include_dirs': [ '<(DEPTH)/fuzz/targets/lib' ],
      },
      'conditions': [
        ['fuzz_oss==0', {
          'all_dependent_settings': {
            'libraries': [
              '-fsanitize=fuzzer',
            ],
          }
        }, {
          'all_dependent_settings': {
            'libraries': [ '-lFuzzingEngine' ],
          }
        }]
      ],
    },
    {
      'target_name': 'nssfuzz-asn1',
      'type': 'executable',
      'sources': [
        'asn1.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/asn1/asn1.gyp:asn1',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-certDN',
      'type': 'executable',
      'sources': [
        'certDN.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-dtls-client',
      'type': 'executable',
      'sources': [
        'tls_client.cc',
      ],
      'defines': [ 'IS_DTLS_FUZZ' ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        '<(DEPTH)/fuzz/targets/lib/tls/tls.gyp:tls_client',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-dtls-server',
      'type': 'executable',
      'sources': [
        'tls_server.cc',
      ],
      'defines': [ 'IS_DTLS_FUZZ' ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        '<(DEPTH)/fuzz/targets/lib/tls/tls.gyp:tls_server',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-pkcs7',
      'type': 'executable',
      'sources': [
        'pkcs7.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/asn1/asn1.gyp:asn1',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-pkcs8',
      'type': 'executable',
      'sources': [
        'pkcs8.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/asn1/asn1.gyp:asn1',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-pkcs12',
      'type': 'executable',
      'sources': [
        'pkcs12.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/asn1/asn1.gyp:asn1',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-quickder',
      'type': 'executable',
      'sources': [
        'quickder.cc',
      ],
      'dependencies': [
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/asn1/asn1.gyp:asn1',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-smime',
      'type': 'executable',
      'sources': [
        'smime.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/asn1/asn1.gyp:asn1',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-tls-client',
      'type': 'executable',
      'sources': [
        'tls_client.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        '<(DEPTH)/fuzz/targets/lib/tls/tls.gyp:tls_client',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz-tls-server',
      'type': 'executable',
      'sources': [
        'tls_server.cc',
      ],
      'dependencies': [
        '<(DEPTH)/cpputil/cpputil.gyp:cpputil',
        '<(DEPTH)/exports.gyp:nss_exports',
        '<(DEPTH)/fuzz/targets/lib/base/base.gyp:base',
        '<(DEPTH)/fuzz/targets/lib/tls/tls.gyp:tls_server',
        'nssfuzz_base',
      ],
    },
    {
      'target_name': 'nssfuzz',
      'type': 'none',
      'dependencies': [
        'nssfuzz-asn1',
        'nssfuzz-certDN',
        'nssfuzz-dtls-client',
        'nssfuzz-dtls-server',
        'nssfuzz-pkcs7',
        'nssfuzz-pkcs8',
        'nssfuzz-pkcs12',
        'nssfuzz-quickder',
        'nssfuzz-smime',
        'nssfuzz-tls-client',
        'nssfuzz-tls-server',
      ],
    },
  ],
}

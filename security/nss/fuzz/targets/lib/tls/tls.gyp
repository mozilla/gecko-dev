# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../../../coreconf/config.gypi',
  ],
  'targets': [
    {
      'target_name': 'tls_client',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'client_config.cc',
          'common.cc',
          'mutators.cc',
          'socket.cc'
        ],
        'include_dirs': [
          '<(DEPTH)/lib/freebl',
          '<(DEPTH)/lib/ssl',
        ],
      },
    },
    {
      'target_name': 'tls_server',
      'type': 'none',
      'direct_dependent_settings': {
        'sources': [
          'common.cc',
          'mutators.cc',
          'server_certs.cc',
          'server_config.cc',
          'socket.cc'
        ],
        'include_dirs': [
          '<(DEPTH)/lib/freebl',
          '<(DEPTH)/lib/ssl',
        ],
      },
    },
  ],
}

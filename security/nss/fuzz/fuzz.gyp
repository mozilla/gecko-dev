{
  'includes': [
    '../coreconf/config.gypi',
  ],
  'targets': [
    {
      'target_name': 'fuzz',
      'type': 'none',
      'dependencies': [
        '<(DEPTH)/fuzz/targets/targets.gyp:nssfuzz',
      ],
    },
  ]
}

config = {
    'default_actions': [
        'clobber',
        'clone-tools',
        'setup-mock',
        'package-source',
    ],
    'stage_platform': 'source',
    'purge_minsize': 3,
    'src_mozconfig': 'browser/config/mozconfigs/linux64/release',
    'enable_signing': True,
    'enable_talos_sendchange': False,
}

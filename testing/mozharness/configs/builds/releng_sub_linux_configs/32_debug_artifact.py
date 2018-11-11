import os

config = {
    #########################################################################
    ######## LINUX GENERIC CONFIG KEYS/VAlUES
    # if you are updating this with custom 32 bit keys/values please add them
    # below under the '32 bit specific' code block otherwise, update in this
    # code block and also make sure this is synced with
    # releng_base_linux_64_builds.py

    # note: overridden by MOZHARNESS_ACTIONS in TaskCluster tasks
    'default_actions': [
        'build',
    ],
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'vcs_share_base': '/builds/hg-shared',
    # debug specific
    'debug_build': True,
    # allows triggering of test jobs when --artifact try syntax is detected
    #########################################################################


    #########################################################################
    ###### 32 bit specific ######
    'platform': 'linux',
    'stage_platform': 'linux-debug',
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': '/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/builds',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        # debug-specific
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # 32 bit specific
        'PATH': '/usr/local/bin:\
/bin:/usr/bin:/usr/local/sbin:/usr/sbin:/sbin',
    },
    'mozconfig_variant': 'debug-artifact',
    #########################################################################
}

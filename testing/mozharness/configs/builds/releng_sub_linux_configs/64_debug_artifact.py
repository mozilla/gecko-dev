import os

config = {
    # note: overridden by MOZHARNESS_ACTIONS in TaskCluster tasks
    'default_actions': [
        'build',
    ],
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    'vcs_share_base': '/builds/hg-shared',
    # debug specific
    'debug_build': True,
    # decides whether we want to use moz_sign_cmd in env
    # allows triggering of test jobs when --artifact try syntax is detected
    #########################################################################


    #########################################################################
    ###### 64 bit specific ######
    'platform': 'linux64',
    'stage_platform': 'linux64-debug',
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
        ## 64 bit specific
        'PATH': '/usr/local/bin:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin',
        ##
    },
    'mozconfig_variant': 'debug-artifact',
    #######################
}
